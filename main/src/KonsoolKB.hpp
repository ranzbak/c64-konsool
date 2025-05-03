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
#include "konsoolled.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuDataStore.hpp"

class C64Emu;
class ExternalCmds;

class KonsoolKB {
   private:
    C64Emu*         c64emu;
    KonsoleLED*     konsoleled;
    DisplayDriver*  display;
    MenuController* menuController;
    MenuDataStore*  menuDataStore = MenuDataStore::getInstance();

    uint8_t sentdc01;
    uint8_t sentdc00;

    QueueHandle_t input_event_queue;

    uint16_t key_hold;

    bool    menu_overlay_active = false;
    uint8_t audio_volume        = 60;

   public:
    bool     deviceConnected;
    uint8_t* buffer;
    // shiftctrlcode: bit 0 -> shift
    //                bit 1 -> ctrl
    //                bit 2 -> commodore
    //                bit 7 -> external command (cmd, x, 128)
    uint8_t  shiftctrlcode;
    uint8_t  keypresseddowncnt;
    uint8_t  virtjoystickvalue;
    bool     keypresseddown;
    bool     detectreleasekey;

    KonsoolKB();
    void    init(C64Emu* c64emu);
    void    handleKeyPress();
    uint8_t getdc01(uint8_t dc00, bool xchgports);
    uint8_t getKBJoyValue(bool port2);
    void    setKbcodes(uint8_t sentdc01, uint8_t sentdc00);
};
