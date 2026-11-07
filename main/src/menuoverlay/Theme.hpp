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

#pragma once

#include <cstdint>

// Every colour and measurement the menu draws with, in one place, so the look
// can be retuned without hunting literals through the renderer.
//
// The panel is 16 bit 565, so the steps between these greys are deliberately
// wide: neighbouring values that look distinct on a desktop band into one
// colour here.
namespace Theme {

// ---- colour -------------------------------------------------------------
// 0xAARRGGBB, the format pax takes.
static const uint32_t BACKGROUND    = 0xFF0F1115;  // the ground
static const uint32_t SURFACE       = 0xFF171A21;  // top bar and hint bar
static const uint32_t SURFACE_RAISE = 0xFF1F242E;  // the selected row
static const uint32_t HAIRLINE      = 0xFF2A303B;  // 1px separators, used sparingly
static const uint32_t TEXT_PRIMARY  = 0xFFE8EAED;  // names and titles
static const uint32_t TEXT_MUTED    = 0xFF98A2B3;  // block counts, hints, context
static const uint32_t ACCENT        = 0xFF5A8DEE;  // selection bar, active toggle
static const uint32_t DANGER        = 0xFFE5484D;  // errors only, never decoration

// ---- geometry -----------------------------------------------------------
static const int SCREEN_W  = 800;
static const int SCREEN_H  = 480;
static const int TOPBAR_H  = 56;
static const int HINTBAR_H = 40;
static const int SIDE_PAD  = 20;
static const int CONTENT_Y = TOPBAR_H;
static const int CONTENT_H = SCREEN_H - TOPBAR_H - HINTBAR_H;  // 384

// A short list of settings can afford to breathe; a disk directory holding a
// hundred and forty entries cannot. Each screen picks the one that suits it,
// so neither has to compromise for the other.
static const int ROW_H       = 34;  // menus: 11 rows
static const int ROW_H_DENSE = 16;  // directories: 24 rows

// A directory row is exactly two screen pixels per ROM pixel, which is the
// size a C64 draws its own characters at and the size the glyphs tile at with
// nothing left over. Three looked oversized and two in a taller row left a gap
// that broke the vertical bars of a title box.

// The selection is a bar down the left edge of a raised row, which reads at a
// glance without painting the whole row a different colour.
static const int SEL_BAR_W = 4;

// Type. saira_regular is a bitmap face whose natural size is 18, so 18 is
// drawn 1:1 and anything else is scaled.
static const int TITLE_SIZE = 22;
static const int BODY_SIZE  = 18;
// What fits beside a glyph in a dense row without overflowing it.
static const int BODY_SIZE_DENSE = 16;

// The scrollbar sits in the right margin rather than stealing a row for a
// "page 3 of 7" counter.
static const int SCROLL_W    = 4;
static const int SCROLL_INSET = 8;

// Where a toggle's value lines up. A fixed column rather than measured text,
// so the values form a column. A directory's block count is placed from the
// glyph width instead, since that depends on the row height.
static const int VALUE_COL = 620;

}  // namespace Theme
