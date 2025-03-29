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
#ifdef __cplusplus
extern "C" {
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "hal/lcd_types.h"
#include "nvs_flash.h"
}
#endif
// #include "Pax.hpp"
#include "pax_types.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "esp_log.h"
#include "src/C64Emu.hpp"
// #include "src/Config.hpp"
// Constants
static char const* TAG = "app_main";

// Global variables
// esp_lcd_panel_handle_t       display_lcd_panel    = NULL;
// esp_lcd_panel_io_handle_t    display_lcd_panel_io = NULL;
// size_t                       display_h_res        = 0;
// size_t                       display_v_res        = 0;
// lcd_color_rgb_pixel_format_t display_color_format;

C64Emu c64Emu;

void setup() {
    ESP_LOGI(TAG, "start setup...");
    // ESP_LOGI(TAG, "setup() running on core %d", xPortGetCoreID());
    try {
        c64Emu.setup();
    } catch (...) {
        ESP_LOGE(TAG, "setup() failed");
        while (true) {
        }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "setup done");
}

void loop() {
    // Run the C64 emulator
    c64Emu.loop();
}

// // Global variables
// static esp_lcd_panel_handle_t       display_lcd_panel    = NULL;
// static esp_lcd_panel_io_handle_t    display_lcd_panel_io = NULL;
// static size_t                       display_h_res        = 0;
// static size_t                       display_v_res        = 0;
// static lcd_color_rgb_pixel_format_t display_color_format;
// static pax_buf_t                    fb                = {};
// static QueueHandle_t                input_event_queue = NULL;

// void blit(void) {
//     esp_lcd_panel_draw_bitmap(display_lcd_panel, 0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
// }


// bool last_event = false;
// void test_input_routine() {
//     // Fetch the handle for using the screen, this works even when
//     esp_err_t res = bsp_display_get_panel(&display_lcd_panel);
//     ESP_ERROR_CHECK(res);                             // Check that the display handle has been initialized
//     bsp_display_get_panel_io(&display_lcd_panel_io);  // Do not check result of panel IO handle: not all types of
//                                                       // display expose a panel IO handle
//     res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format);
//     ESP_ERROR_CHECK(res);  // Check that the display parameters have been initialized

//     ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

//     // Initialize the graphics stack
//     pax_buf_init(&fb, NULL, display_h_res, display_v_res, PAX_BUF_16_565RGB);
//     pax_buf_reversed(&fb, false);
//     pax_buf_set_orientation(&fb, PAX_O_ROT_CW);

//     ESP_LOGW(TAG, "Hello world!");

//     pax_background(&fb, 0xFFFFFFFF);
//     pax_draw_text(&fb, 0xFF000000, pax_font_sky_mono, 16, 0, 0, "Hello world!");
//     blit();

//     while (1) {
//         bsp_input_event_t event;
//         if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
//             if (last_event == false) {
//             pax_background(&fb, 0xFFFFFFFF);
//             pax_draw_text(&fb, 0xFF000000, pax_font_sky_mono, 16, 0, 0, "Button pressed!");
//             blit();
//             }
//             last_event = true;
//         } else {
//             if (last_event == true) {
//             pax_draw_text(&fb, 0xFF000000, pax_font_sky_mono, 16, 0, 0, "Nothing!");
//             blit();
//             pax_background(&fb, 0xFF0000FF);
//             blit();
//             }
//             last_event = false;
//         }
//     }
// }

extern "C" void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Initialize the Board Support Package
    ESP_ERROR_CHECK(bsp_device_initialize());
    setup();  // Initialize the C64 emulator and the display driver

    // test_input_routine();

    // Main loop to run the C64 emulator and the display driver.
    while (true) {
        loop();
    }
}
