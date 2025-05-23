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
#include "hal/color_types.h"
#include "hal/ppa_types.h"
#include "pax_types.h"
extern "C" {
#include <stddef.h>
#include "bsp/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "freertos/idf_additions.h"
}
#ifdef USE_GFXP4
#include "GfxP4.hpp"
#include "HardwareInitializationException.h"
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

static const char* TAG = "GfxP4";

// Global variables
// static esp_lcd_panel_handle_t       display_lcd_panel    = NULL;
// static esp_lcd_panel_io_handle_t    display_lcd_panel_io = NULL;
// static size_t                       display_h_res        = 0;
// static size_t                       display_v_res        = 0;
// static lcd_color_rgb_pixel_format_t display_color_format;
// static pax_buf_t                    fb                = {};
// static QueueHandle_t                input_event_queue = NULL;

// static uint32_t lu_pinbitmask[256];

void GfxP4::blit(void)
{
    esp_lcd_panel_draw_bitmap(this->display_lcd_panel, 0, 0, this->display_h_res, this->display_v_res,
                              pax_buf_get_pixels(&this->fb));
}

void GfxP4::writeCmd(uint8_t cmd)
{
    // Write a command to the display
}

void GfxP4::writeData(uint8_t data)
{
    // Write a byte of data to the display
}

void GfxP4::init()
{
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
    raw_fb = (uint16_t*)heap_caps_calloc(display_v_res * display_v_res, sizeof(uint16_t),
                                         MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "Register PPA client for SRM operation");
    ppa_srm_config = {
        .oper_type             = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));
    ppa_fill_config = {
        .oper_type             = PPA_OPERATION_FILL,
        .max_pending_trans_num = 1,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_fill_config, &ppa_fill_handle));

    // Setup rotation and scale configuration
    // Initialize input configuration
    GfxP4::active_config.in.pic_w          = 320;
    GfxP4::active_config.in.pic_h          = 200;
    GfxP4::active_config.in.block_w        = 320;
    GfxP4::active_config.in.block_h        = 200;
    GfxP4::active_config.in.block_offset_x = 0;
    GfxP4::active_config.in.block_offset_y = 0;
    GfxP4::active_config.in.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    // Initialize output configuration
    GfxP4::active_config.out.buffer         = raw_fb;
    GfxP4::active_config.out.buffer_size    = display_h_res * display_v_res * 2;
    GfxP4::active_config.out.pic_w          = display_h_res;
    GfxP4::active_config.out.pic_h          = display_v_res;
    GfxP4::active_config.out.block_offset_x = border_height;  // x_offset;
    GfxP4::active_config.out.block_offset_y = border_width;   // y_offset;
    GfxP4::active_config.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    // Initialize other configuration parameters
    GfxP4::active_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
    GfxP4::active_config.scale_x        = 2.0;
    GfxP4::active_config.scale_y        = 2.0;
    GfxP4::active_config.rgb_swap       = 0;
    GfxP4::active_config.byte_swap      = 0;
    GfxP4::active_config.mode           = PPA_TRANS_MODE_BLOCKING;

    // Fill config for the border
    GfxP4::fill_config.out.buffer         = raw_fb;
    GfxP4::fill_config.out.buffer_size    = display_h_res * display_v_res * sizeof(uint16_t);
    GfxP4::fill_config.out.pic_w          = display_h_res;
    GfxP4::fill_config.out.pic_h          = display_v_res;
    GfxP4::fill_config.out.block_offset_x = 0;
    GfxP4::fill_config.out.block_offset_y = 0;
    GfxP4::fill_config.out.fill_cm        = PPA_FILL_COLOR_MODE_RGB565;  // Color mode matches buffer
    GfxP4::fill_config.fill_block_w       = 0;
    GfxP4::fill_config.fill_block_h       = 0;
    GfxP4::fill_config.fill_argb_color    = {
           .val = 0,  // Only lower 16 bits used for RGB565
    };
    GfxP4::fill_config.mode      = PPA_TRANS_MODE_BLOCKING;
    GfxP4::fill_config.user_data = NULL;
}

void GfxP4::copyinit(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h)
{
    // Start copy on display?? not sure
    // uint16_t x1 = x0 + w - 1;
    // uint16_t y1 = y0 + h - 1;
}

void GfxP4::copycopy(uint16_t data, uint32_t clearMask)
{
    // Copy data on display ?? not sure
}

void GfxP4::copyend()
{
    // End copy on display?? not sure
}

void GfxP4::copyColor(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t data)
{
    uint32_t argb = rgb565ToRgb8888(data);

    // Make sure the x and y coordinates are switched to rotate the display
    // the raw_fb has the same dimensions as the display
    // start drawing at x0, y0 and make it w wide and h tall
    fill_config.out.block_offset_x = y0;
    fill_config.out.block_offset_y = x0;
    fill_config.fill_block_w       = h;
    fill_config.fill_block_h       = w;
    fill_config.fill_argb_color    = {.val = argb};
    ppa_do_fill(ppa_fill_handle, &fill_config);
}

uint32_t GfxP4::rgb565ToRgb8888(uint16_t rgb565)
{

    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5) & 0x3F;
    uint8_t b5 = rgb565 & 0x1F;

    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return (0xFF << 24) | (r8 << 16) | (g8 << 8) | b8;  // ARGB8888
}

void IRAM_ATTR GfxP4::drawFrame(uint16_t* frameColors)
{

    // Left bar
    // Use PPA to rotate and scale the bitmap to the display.
    active_config.in.buffer                = frameColors;  // 34 is raster line 50 => 50 - 16
    GfxP4::active_config.in.pic_w          = 270;
    GfxP4::active_config.in.pic_h          = 1;
    GfxP4::active_config.in.block_w        = 200;
    GfxP4::active_config.in.block_h        = 1;
    GfxP4::active_config.in.block_offset_x = 34;
    GfxP4::active_config.in.block_offset_y = 0;

    // Initialize output configuration
    GfxP4::active_config.out.buffer         = raw_fb;
    GfxP4::active_config.out.buffer_size    = display_h_res * display_v_res * 2;
    GfxP4::active_config.out.pic_w          = display_h_res;
    GfxP4::active_config.out.pic_h          = display_v_res;
    GfxP4::active_config.out.block_offset_x = border_height;  // x_offset;
    GfxP4::active_config.out.block_offset_y = 0;              // y_offset;

    // Initialize other configuration parameters
    GfxP4::active_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_180;
    GfxP4::active_config.scale_x        = 2.0;
    GfxP4::active_config.scale_y        = border_width;
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));

    // Right bar
    GfxP4::active_config.out.block_offset_y = border_width + vic_h_width;  // y_offset;
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));

    // Top bar
    uint16_t top_border_height      = border_height / 2;
    uint16_t top_border_start       = 34 - top_border_height;
    active_config.in.block_w        = top_border_height;
    active_config.in.block_offset_x = top_border_start;

    active_config.out.block_offset_x = 0;

    // TODO: Put into a function? 
    active_config.scale_x = 2.0;
    uint16_t blits_left   = display_v_res;
    while (blits_left > 0) {
        if (blits_left >= 255) {
            active_config.scale_y  = 255;
            blits_left            -= 255;
        } else {
            active_config.scale_y = blits_left;
            blits_left            = 0;
        }
        active_config.out.block_offset_y = blits_left;
        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));
    }

    // GfxP4::copyColor(0, 0, display_v_res, border_height, frameColors[0]);
    // Bottom bar
    uint16_t bottom_border_start    = 32 + 200;
    active_config.in.block_offset_x = bottom_border_start;
    active_config.out.block_offset_x = border_height + vic_v_height;
    active_config.scale_x = 2.0;
    blits_left            = display_v_res;
    // TODO: Put into a function? 
    while (blits_left > 0) {
        if (blits_left >= 255) {
            active_config.scale_y  = 255;
            blits_left            -= 255;
        } else {
            active_config.scale_y = blits_left;
            blits_left            = 0;
        }
        active_config.out.block_offset_y = blits_left;
        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));
    }
}

void GfxP4::drawMenuOverlay()
{
    active_config.in.buffer                 = (uint16_t*)fb.buf_16bpp;
    GfxP4::active_config.in.pic_w           = 640;
    GfxP4::active_config.in.pic_h           = 400;
    GfxP4::active_config.in.block_w         = 640;
    GfxP4::active_config.in.block_h         = 400;
    GfxP4::active_config.in.block_offset_x  = 0;
    GfxP4::active_config.in.block_offset_y  = 0;
    GfxP4::active_config.out.block_offset_x = border_height;  // x_offset;
    GfxP4::active_config.out.block_offset_y = border_width;   // y_offset;
    GfxP4::active_config.scale_x            = 1.0;
    GfxP4::active_config.scale_y            = 1.0;
    GfxP4::active_config.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;

    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));
}

void GfxP4::drawBitmap(uint16_t* bitmap)
{
    // Overlay the menu if enabled
    if (menu_overlay_enabled) {
        // Draw menu
        drawMenuOverlay();
    } else {
        // Set bitmap to configuration.
        active_config.in.buffer = bitmap;

        // Use PPA to rotate and scale the bitmap to the display.
        GfxP4::active_config.in.pic_w          = 320;
        GfxP4::active_config.in.pic_h          = 200;
        GfxP4::active_config.in.block_w        = 320;
        GfxP4::active_config.in.block_h        = 200;
        GfxP4::active_config.in.block_offset_x = 0;
        GfxP4::active_config.in.block_offset_y = 0;

        // Initialize output configuration
        GfxP4::active_config.out.buffer         = raw_fb;
        GfxP4::active_config.out.buffer_size    = display_h_res * display_v_res * 2;
        GfxP4::active_config.out.pic_w          = display_h_res;
        GfxP4::active_config.out.pic_h          = display_v_res;
        GfxP4::active_config.out.block_offset_x = border_height;  // x_offset;
        GfxP4::active_config.out.block_offset_y = border_width;   // y_offset;

        // Initialize other configuration parameters
        GfxP4::active_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
        GfxP4::active_config.scale_x        = 2.0;
        GfxP4::active_config.scale_y        = 2.0;
        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &active_config));
    }

    // Send the frame to the display over MIPI DSI.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(display_lcd_panel, 0, 0, display_h_res, display_v_res, raw_fb));
}

void GfxP4::enableMenuOverlay(bool enable)
{
    menu_overlay_enabled = enable;
}

pax_buf_t* GfxP4::getMenuFb()
{
    return &fb;
}

const uint16_t* GfxP4::getC64Colors() const
{
    return c64Colors;
}
#endif
