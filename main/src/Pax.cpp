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
#include <cstdint>
#include "Config.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "hal/ppa_types.h"
#include "pax_types.h"
extern "C" {
#include <stddef.h>
#include "bsp/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "freertos/idf_additions.h"
}
#ifdef USE_PAX
#include "HardwareInitializationException.h"
#include "Pax.hpp"
// #include <FreeRTOS.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include "hal/lcd_types.h"

// #include <task.h>
#include "bsp/display.h"
#include "driver/ppa.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"

static const char* TAG = "Pax";

// Global variables
// static esp_lcd_panel_handle_t       display_lcd_panel    = NULL;
// static esp_lcd_panel_io_handle_t    display_lcd_panel_io = NULL;
// static size_t                       display_h_res        = 0;
// static size_t                       display_v_res        = 0;
// static lcd_color_rgb_pixel_format_t display_color_format;
// static pax_buf_t                    fb                = {};
// static QueueHandle_t                input_event_queue = NULL;

// static uint32_t lu_pinbitmask[256];

void Pax::blit(void) {
    esp_lcd_panel_draw_bitmap(this->display_lcd_panel, 0, 0, this->display_h_res, this->display_v_res,
                              pax_buf_get_pixels(&this->fb));
}

void Pax::writeCmd(uint8_t cmd) {
    // Write a command to the display
}

void Pax::writeData(uint8_t data) {
    // Write a byte of data to the display
}

void Pax::init() {
    // Enable backlight
    esp_err_t res = bsp_display_get_panel(&display_lcd_panel);
    ESP_ERROR_CHECK(res);                             // Check that the display handle has been initialized
    bsp_display_get_panel_io(&display_lcd_panel_io);  // Do not check result of panel IO handle: not all types of
                                                      // display expose a panel IO handle
    res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format);
    ESP_ERROR_CHECK(res);  // Check that the display parameters have been initialized

    // Setup variables
    border_width  = (display_v_res - vic_h_width) / 2;
    border_height = (display_h_res - vic_v_height) / 2;

    // allocate raw framebuffer memory
    // raw_fb = (uint16_t*)calloc(display_h_res * display_v_res, sizeof(uint16_t));
    raw_fb = (uint16_t*)heap_caps_calloc(
        display_v_res * display_v_res,
        sizeof(uint16_t),
        MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM
    );

    ESP_LOGI(TAG, "Register PPA client for SRM operation");
    ppa_srm_config = {
        .oper_type             = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));

    // Off set to the border
    size_t    x_offset = border_width;
    size_t    y_offset = display_h_res - border_height / 2;  // Bottom of screen up, start drawing top down

    // Setup rotation and scale configuration
    // Initialize input configuration
    Pax::srm_config.in.pic_w = 320;
    Pax::srm_config.in.pic_h = 200;
    Pax::srm_config.in.block_w = 320;
    Pax::srm_config.in.block_h = 200;
    Pax::srm_config.in.block_offset_x = 0;
    Pax::srm_config.in.block_offset_y = 0;
    Pax::srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

    // Initialize output configuration
    Pax::srm_config.out.buffer = raw_fb;
    Pax::srm_config.out.buffer_size = display_h_res * display_v_res * 2;
    Pax::srm_config.out.pic_w = display_h_res;
    Pax::srm_config.out.pic_h = display_v_res;
    Pax::srm_config.out.block_offset_x = border_height; // x_offset;
    Pax::srm_config.out.block_offset_y = border_width; // y_offset;
    Pax::srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

    // Initialize other configuration parameters
    Pax::srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
    Pax::srm_config.scale_x = 2.0;
    Pax::srm_config.scale_y = 2.0;
    Pax::srm_config.rgb_swap = 0;
    Pax::srm_config.byte_swap = 0;
    Pax::srm_config.mode = PPA_TRANS_MODE_BLOCKING;

    // Initialize the graphics stack
    // pax_buf_init(&fb, NULL, display_h_res, display_v_res, PAX_BUF_16_565RGB);
    // pax_buf_reversed(&fb, false);
    // pax_buf_set_orientation(&fb, PAX_O_ROT_CW);

    // pax_background(&fb, 0xff887ecb);
    // // pax_draw_text(&fb, 0xff000000, pax_font_sky_mono, 16, 0, 0, "Hello, World!");
    // blit();

    // Pax::frame_mem_size = MAX( 320 * Pax::border_height, Pax::border_width * display_h_res);
}

void Pax::copyinit(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h) {
    // Start copy on display?? not sure
    // uint16_t x1 = x0 + w - 1;
    // uint16_t y1 = y0 + h - 1;
}

void Pax::copycopy(uint16_t data, uint32_t clearMask) {
    // Copy data on display ?? not sure
}

void Pax::copyend() {
    // End copy on display?? not sure
}

void Pax::copyColor(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t data) {
    // Make sure the x and y coordinates are switched to rotate the display
    // the raw_fb has the same dimensions as the display
    // start drawing at x0, y0 and make it w wide and h tall
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            // Rotate 90 degrees clockwise
            uint16_t dest_x                         = y0 + y;
            uint16_t dest_y                         = x0 + x;  // Assuming display_h_res is width
            raw_fb[dest_y * display_h_res + dest_x] = data;
        }
    }
}

void Pax::copyData(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t* data) {
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            Pax::copyColor(x0 + x, y0 + y, 1, 1, *data++);
        }
    }

    esp_lcd_panel_draw_bitmap(this->display_lcd_panel, x0, y0, w, h,
                              // data
                              pax_buf_get_pixels(&fb));
    // copyinit(x0, y0, w, h);
    // uint32_t clearMask = lu_pinbitmask[255];
    // for (uint32_t i = 0; i < w * h; i++) {
    //     copycopy(*data++, clearMask);
    // }
    // copyend();
}

void Pax::drawFrame(uint16_t frameColor) {

    // Top bar
    Pax::copyColor(border_width, 0, vic_h_width, border_height, frameColor);
    // Bottom bar
    Pax::copyColor(border_width, vic_v_height + border_height, vic_h_width, border_height, frameColor);
    // Left bar
    Pax::copyColor(0, 0, border_width, display_h_res, frameColor);
    // Right bar
    Pax::copyColor(border_width + vic_h_width, 0, border_width, display_h_res, frameColor);
}

void Pax::drawBitmap(uint16_t* bitmap) {
    // Set bitmap to configuration.
    srm_config.in.buffer = bitmap;

    // Use PPA to rotate and scale the bitmap to the display.
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config));
    // Send the frame to the display over MIPI DSI.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(display_lcd_panel, 0, 0, display_h_res, display_v_res, raw_fb));
}

const uint16_t* Pax::getC64Colors() const {
    return c64Colors;
}
#endif
