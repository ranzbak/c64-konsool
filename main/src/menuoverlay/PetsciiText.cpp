/*
 Copyright (C) 2025 Konsool 64 contributors

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

#include "PetsciiText.hpp"
#include <cstdint>
#include "images/CbmImage.hpp"

// roms/charset.h defines the array rather than declaring it, and C64Emu.cpp
// already includes it, so including it here would be a second definition.
extern unsigned char charset_rom[];

// The unshifted charset is the first half of the ROM. That is the one the
// directory a disk carries was drawn for; the shifted set at $0800 turns its
// graphics into lower case.
static const unsigned int CHARSET_UNSHIFTED = 0x0000;

float pax_draw_petscii(pax_buf_t* buf, pax_col_t color, float x, float y, const std::string& petscii, int scale)
{
    const float cellWidth = static_cast<float>(PETSCII_CELL * scale);

    for (size_t i = 0; i < petscii.size(); i++) {
        uint8_t        code  = petsciiToScreenCode(static_cast<uint8_t>(petscii[i]));
        const uint8_t* glyph = charset_rom + CHARSET_UNSHIFTED + (code << 3);
        float          left  = x + static_cast<float>(i) * cellWidth;

        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            if (bits == 0) continue;

            // The ROM's most significant bit is the leftmost pixel, the way
            // the VIC reads it. Draw each run of set bits as one rectangle
            // rather than a rectangle per pixel.
            int col = 0;
            while (col < 8) {
                if (!(bits & (0x80 >> col))) {
                    col++;
                    continue;
                }
                int run = 0;
                while (col + run < 8 && (bits & (0x80 >> (col + run)))) {
                    run++;
                }
                pax_draw_rect(buf, color, left + static_cast<float>(col * scale),
                              y + static_cast<float>(row * scale), static_cast<float>(run * scale),
                              static_cast<float>(scale));
                col += run;
            }
        }
    }

    return x + static_cast<float>(petscii.size()) * cellWidth;
}
