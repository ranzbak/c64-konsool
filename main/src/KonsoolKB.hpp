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
#include <string>
#include "DisplayDriver.hpp"
#include "bsp/input.h"
#include "freertos/idf_additions.h"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuDataStore.hpp"

class C64Emu;
class ExternalCmds;

class KonsoolKB {
   private:
    C64Emu*         c64emu;
    DisplayDriver*  display;
    MenuController* menuController;
    MenuDataStore*  menuDataStore = MenuDataStore::getInstance();

    // C64 keyboard matrix, one bit per column per row (0 = pressed), plus
    // a column->row mirror for the reverse-direction CIA port read
    uint8_t keyarr[8];
    uint8_t rev_keyarr[8];

    QueueHandle_t input_event_queue;

    uint16_t key_hold;

    bool    menu_overlay_active = false;
    uint8_t audio_volume        = 60;

   public:
    bool     deviceConnected;
    uint8_t* buffer;
    uint8_t  keypresseddowncnt;
    uint8_t  virtjoystickvalue;
    bool     keypresseddown;
    bool     detectreleasekey;

    KonsoolKB();
    void    init(C64Emu* c64emu);
    void    handleKeyPress();
    void    handleGamepadMenuInput();
    uint8_t getdc01(uint8_t dc00, bool xchgports);
    uint8_t getKBJoyValue();
    uint8_t getGamepadJoyValue();
    void    setKbcodes(uint8_t colmask, uint8_t rowmask);
};
