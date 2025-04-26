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
#include "bsp/led.h"
#include "esp_lcd_panel_io.h"
#include "esp_rom_gpio.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/usb_serial_jtag_ll.h"
#include "portmacro.h"
#include "targets/tanmatsu/tanmatsu_hardware.h"
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
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"
#include "src/C64Emu.hpp"
#include "src/konsoolled.hpp"
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
    ESP_LOGI(TAG, "Setup TE refresh interrupt");
    ESP_ERROR_CHECK(bsp_display_set_tearing_effect_mode(BSP_DISPLAY_TE_V_BLANKING));
    ESP_LOGI(TAG, "setup done");
}

extern "C" void app_main(void) {
    SemaphoreHandle_t semaphore = NULL;

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

    bsp_display_get_tearing_effect_semaphore(&semaphore);

    float to50hz = 0;

    // Main loop outputs C64 screen contents to the display
    while (true) {
        // Wait for display refresh signal
        xSemaphoreTake(semaphore, 100 / portTICK_PERIOD_MS);

        // We only want 50Hz output, so we'll skip some frames
        if (to50hz > 1.0) {
        c64Emu.loop();
         to50hz -= 1.0;
        }
        to50hz += PAL_FRAMERATE;
    }
}
