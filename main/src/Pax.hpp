#pragma once
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
#include "DisplayDriver.hpp"
#include "esp_lcd_types.h"
#include "hal/lcd_types.h"
#include "pax_types.h"
// #include <cstdint>

// no elegant/simple solution for max() at compile time in C++11
// #define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

class Pax : public DisplayDriver {
   private:
    static const uint16_t c64_black      = 0x0000;
    static const uint16_t c64_white      = 0xffff;
    static const uint16_t c64_red        = 0x8000;
    static const uint16_t c64_turquoise  = 0xa7fc;
    static const uint16_t c64_purple     = 0xc218;
    static const uint16_t c64_green      = 0x064a;
    static const uint16_t c64_blue       = 0x0014;
    static const uint16_t c64_yellow     = 0xe74e;
    static const uint16_t c64_orange     = 0xd42a;
    static const uint16_t c64_brown      = 0x6200;
    static const uint16_t c64_lightred   = 0xfbae;
    static const uint16_t c64_grey1      = 0x3186;
    static const uint16_t c64_grey2      = 0x73ae;
    static const uint16_t c64_lightgreen = 0xa7ec;
    static const uint16_t c64_lightblue  = 0x043f;
    static const uint16_t c64_grey3      = 0xb5d6;

    static const uint16_t vic_h_width  = 320 * 2;
    static const uint16_t vic_v_height = 200 * 2;

    pax_buf_t c64_buf;

    pax_buf_t                    fb;
    uint16_t*                     raw_fb;
    esp_lcd_panel_handle_t       display_lcd_panel;
    esp_lcd_panel_io_handle_t    display_lcd_panel_io;
    lcd_color_rgb_pixel_format_t display_color_format;
    size_t                       border_width;
    size_t                       border_height;
    size_t                       display_h_res;
    size_t                       display_v_res;
    uint16_t                     frame_mem_size;

    const uint16_t c64Colors[16] = {c64_black, c64_white,      c64_red,       c64_turquoise, c64_purple,   c64_green,
                                    c64_blue,  c64_yellow,     c64_orange,    c64_brown,     c64_lightred, c64_grey1,
                                    c64_grey2, c64_lightgreen, c64_lightblue, c64_grey3};

    //   const uint16_t BORDERWIDTH = (display_h_res - 320) / 2;
    //   const uint16_t BORDERHEIGHT = (display_v_res - 200) / 2;
    //   const uint16_t FRAMEMEMSIZE =
    //   MAX(320 * BORDERHEIGHT, BORDERWIDTH *Config::LCDHEIGHT);

    inline static void writeCmd(uint8_t cmd) __attribute__((always_inline));
    inline static void writeData(uint8_t data) __attribute__((always_inline));
    inline static void copyinit(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h) __attribute__((always_inline));
    inline static void copycopy(uint16_t data, uint32_t clearMask) __attribute__((always_inline));
    inline static void copyend() __attribute__((always_inline));

    void blit(void);
    void copyColor(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t data);
    void copyData(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t* data);

   public:
    void            init() override;
    void            drawFrame(uint16_t frameColor) override;
    void            drawBitmap(uint16_t* bitmap) override;
    const uint16_t* getC64Colors() const override;
};
