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
#include "Config.hpp"
#include "DisplayDriver.hpp"
#if defined(USE_ST7789V)
#include "ST7789V.hpp"
#elif defined(USE_RM67162)
#include "RM67162.h"
#elif defined(USE_GFXP4)
#include "GfxP4.hpp"
#endif

struct ConfigDisplay {
    DisplayDriver* displayDriver;
#if defined(USE_ST7789V)
    ConfigDisplay() {
        displayDriver = new ST7789V();
    }
#elif defined(USE_RM67162)
    ConfigDisplay() {
        displayDriver = new RM67162();
    }
#elif defined(USE_GFXP4)
    ConfigDisplay() {
        displayDriver = new GfxP4();
    }
#endif
};