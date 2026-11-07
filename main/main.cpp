/*
 Copyright (C) 2024 retroelec <retroelec42@gmail.com>

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 3 of the License, or (at your
 option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 For the complete text of the GNU General Public License see
 http://www.gnu.org/licenses/.
*/

// using namespace std;

// #include <chrono>
// #include <iostream>
// #include <thread>
#include "Config.hpp"
// #include "bsp/led.h"
// #include "esp_lcd_panel_io.h"
// #include "esp_rom_gpio.h"
#include "freertos/idf_additions.h"
// #include "freertos/projdefs.h"
// #include "hal/gpio_types.h"
// #include "hal/usb_serial_jtag_ll.h"
#include "freertos/projdefs.h"
#include "esp_app_desc.h"
#include "menuoverlay/Theme.hpp"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"
#include "portmacro.h"
// #include "targets/tanmatsu/tanmatsu_hardware.h"
#ifdef __cplusplus
extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "bsp/device.h"
#include "bsp/display.h"
// #include "bsp/input.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
// #include "hal/lcd_types.h"
#include "nvs_flash.h"
#include "usb_msc.h"
}
#endif
#include "driver/gpio.h"
#include "esp_log.h"
// #include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "src/C64Emu.hpp"
// #include "src/konsoolled.hpp"
// #include "src/Config.hpp"

#include "esp_vfs_fat.h"
#include "global_event_handler.h"
#include "hid_keyboard.h"

extern "C" {
#include "pax_gfx.h"
uint8_t* fb_memory;
}

// Constants
static char const* TAG = "app_main";

static wl_handle_t wl_handle = WL_INVALID_HANDLE;

C64Emu c64Emu;

static void on_usb_mount(bool mounted, void* arg)
{
    ESP_LOGI(TAG, "/usb %s", mounted ? "mounted" : "unmounted");
}

void setup()
{
    ESP_LOGI(TAG, "start setup...");
    // ESP_LOGI(TAG, "setup() running on core %d", xPortGetCoreID());
    try {
        c64Emu.setup();
    } catch (...) {
        ESP_LOGE(TAG, "setup() failed");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "Setup TE refresh interrupt");
    ESP_ERROR_CHECK(bsp_display_set_tearing_effect_mode(BSP_DISPLAY_TE_V_BLANKING));
    ESP_LOGI(TAG, "setup done");
}

extern "C" void app_main(void)
{
    SemaphoreHandle_t semaphore      = NULL;
    SemaphoreHandle_t frameRateMutex = NULL;

    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    esp_vfs_fat_mount_config_t fat_mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 10,
        .allocation_unit_size     = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    res = esp_vfs_fat_spiflash_mount_rw_wl("/int", "locfd", &fat_mount_config, &wl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FAT filesystem: %s", esp_err_to_name(res));
    }

    // Initialize the Board Support Package
    const bsp_configuration_t bsp_configuration = {
        .display =
            {
                .requested_color_format = BSP_DISPLAY_COLOR_FORMAT_16_565RGB,
                .num_fbs                = 1,
            },
    };
    esp_err_t bsp_init_result = bsp_device_initialize(&bsp_configuration);
    ESP_ERROR_CHECK(bsp_init_result);

    pax_buf_t framebuffer;
    fb_memory = (uint8_t*)malloc(800 * 480 * 2);
    pax_buf_init(&framebuffer, fb_memory, 480, 800, PAX_BUF_16_565RGB);
    pax_buf_reversed(&framebuffer, false);
    pax_buf_set_orientation(&framebuffer, PAX_O_ROT_CW);

    // The boot mark: the Commodore C=, on the same ground the menu uses. Drawn
    // rather than loaded, because a logo that can fail to appear is worse than
    // a plain one -- the icons under /int/icons are read at runtime and
    // icons.c already has a path for them being missing.
    //
    // SVG canvas 130x122, scaled 2x to 260x244 and centred, with the name set
    // underneath. C shape: outer circle r=61, inner r=32, both centred at SVG
    // (61,61), opening to the right at SVG x=78. Bars: upper trapezoid
    // (78,34)->(130,34)->(106,58)->(78,58), lower (78,64)->(106,64)->(130,88)
    // ->(78,88), in SVG coordinates after translate -5,-9.
    {
        const pax_col_t C_BLUE = Theme::ACCENT;
        const pax_col_t C_RED  = Theme::DANGER;
        const pax_col_t C_BG   = Theme::BACKGROUND;
        const float     S      = 2.0f;
        const float     OX     = (800.0f - 130.0f * S) / 2.0f;
        const float     OY     = 74.0f;
        auto            sx     = [=](float x) { return x * S + OX; };
        auto            sy     = [=](float y) { return y * S + OY; };

        pax_background(&framebuffer, C_BG);

        // Outer disk, erase the inner disk, then erase the right side to open
        // the C. Painting the cut-outs in the background colour is what keeps
        // the ring an even width without drawing an arc.
        pax_draw_circle(&framebuffer, C_BLUE, sx(61), sy(61), 61 * S);
        pax_draw_circle(&framebuffer, C_BG, sx(61), sy(61), 32 * S);
        pax_draw_rect(&framebuffer, C_BG, sx(78), sy(0), 200 * S, 122 * S);

        // Upper "=" bar
        pax_draw_tri(&framebuffer, C_BLUE, sx(78), sy(34), sx(130), sy(34), sx(78), sy(58));
        pax_draw_tri(&framebuffer, C_BLUE, sx(130), sy(34), sx(106), sy(58), sx(78), sy(58));

        // Lower "=" bar
        pax_draw_tri(&framebuffer, C_RED, sx(78), sy(64), sx(106), sy(64), sx(78), sy(88));
        pax_draw_tri(&framebuffer, C_RED, sx(106), sy(64), sx(130), sy(88), sx(78), sy(88));

        // The name, centred on the real measured width rather than a guess.
        const char* name = "Konsool 64";
        const char* sub  = "COMMODORE 64 EMULATOR";
        const float NAME_SIZE = 36.0f;
        pax_vec2f   nsz       = pax_text_size(pax_font_saira_regular, NAME_SIZE, name);
        pax_vec2f   ssz       = pax_text_size(pax_font_saira_regular, Theme::BODY_SIZE, sub);

        // Centre on the measured width, but never off the side of the screen:
        // a measurement that comes back as zero or nonsense would otherwise
        // put the text where it cannot be seen.
        auto centre = [](float w, float fallback) {
            float x = (800.0f - (w > 1.0f ? w : fallback)) / 2.0f;
            return x < 20.0f ? 20.0f : x;
        };
        pax_draw_text(&framebuffer, Theme::TEXT_PRIMARY, pax_font_saira_regular, NAME_SIZE,
                      centre(nsz.x, NAME_SIZE * 0.55f * 10), sy(122) + 24.0f, name);
        pax_draw_text(&framebuffer, Theme::TEXT_MUTED, pax_font_saira_regular, Theme::BODY_SIZE,
                      centre(ssz.x, Theme::BODY_SIZE * 0.55f * 21), sy(122) + 74.0f, sub);

        // Which build this actually is. The version esp-idf puts in the image
        // is "git describe" of the tree it came from, so it names the commit
        // and says "-dirty" when it was built over uncommitted changes, which
        // is what you want to know when a badge has been reflashed all
        // evening and the launcher only shows an AppFS version number.
        const esp_app_desc_t* app = esp_app_get_description();
        if (app != nullptr) {
            pax_vec2f vsz = pax_text_size(pax_font_saira_regular, 16, app->version);
            pax_draw_text(&framebuffer, Theme::TEXT_MUTED, pax_font_saira_regular, 16,
                          centre(vsz.x, 16 * 0.55f * 16), sy(122) + 104.0f, app->version);
        }
    }

    esp_lcd_panel_handle_t display_lcd_panel;
    bsp_display_get_panel(&display_lcd_panel);
    esp_lcd_panel_draw_bitmap(display_lcd_panel, 0, 0, 480, 800, pax_buf_get_pixels(&framebuffer));

    setup();  // Initialize the C64 emulator and the display driver

    bsp_display_get_tearing_effect_semaphore(&semaphore);

    global_event_handler_initialize();

    float to50hz = 0;

    // Get 50Hz frame rate semaphore
    frameRateMutex = c64Emu.cpu.getFrameRateMutex();

    hid_kbd_init();
    ESP_ERROR_CHECK(usb_msc_init(on_usb_mount, NULL));

    // Main loop outputs C64 screen contents to the display
    while (true) {
        // Wait for display refresh signal
        xSemaphoreTake(semaphore, 100 / portTICK_PERIOD_MS);
        handle_hid_events();
        // We only want 50Hz output, so we'll skip some frames
        if (to50hz > 1.0) {
            c64Emu.loop();
            xSemaphoreGive(frameRateMutex);
            to50hz -= 1.0;
        }
        // Make sure we always have 50Hz output
        to50hz += PAL_TO_NTSC_RATIO;
    }
}
