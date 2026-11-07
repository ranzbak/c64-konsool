#pragma once
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

#include <cstddef>
#include <string>
#include "pax_gfx.h"

// Width in pixels of one character drawn at `scale`.
static const int PETSCII_CELL = 8;

// Draws a PETSCII string with the C64 character ROM, `scale` screen pixels per
// ROM pixel, so a character is 8*scale wide and 8*scale tall. The glyphs come
// from the unshifted charset, which is the one a directory is listed in.
// Returns the x one past the last column, so a caller can put something after
// it. Nothing is drawn for a character that has no glyph.
float pax_draw_petscii(pax_buf_t* buf, pax_col_t color, float x, float y, const std::string& petscii, int scale);
