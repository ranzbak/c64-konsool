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
extern "C" {
#include <esp_log.h>
#include "bsp/input.h"
}
#include "C64Emu.hpp"
#include "Config.hpp"
#include "KonsoolKB.hpp"
// #include "ExternalCmds.h"
// #include "Joystick.h"
#include <cstdint>
#include <cstring>

// static const char *TAG = "BLEKB";

static const uint8_t NUMOFCYCLES_KEYPRESSEDDOWN = 3;

// static const uint8_t VIRTUALJOYSTICKLEFT_ACTIVATED    = 0x01;
// static const uint8_t VIRTUALJOYSTICKLEFT_DEACTIVATED  = 0x81;
// static const uint8_t VIRTUALJOYSTICKRIGHT_ACTIVATED   = 0x02;
// static const uint8_t VIRTUALJOYSTICKRIGHT_DEACTIVATED = 0x82;
// static const uint8_t VIRTUALJOYSTICKUP_ACTIVATED      = 0x04;
// static const uint8_t VIRTUALJOYSTICKUP_DEACTIVATED    = 0x84;
// static const uint8_t VIRTUALJOYSTICKDOWN_ACTIVATED    = 0x08;
// static const uint8_t VIRTUALJOYSTICKDOWN_DEACTIVATED  = 0x88;

KonsoolKB::KonsoolKB() {
    buffer = nullptr;
}

QueueHandle_t input_event_queue = NULL;

void KonsoolKB::init(C64Emu* c64emu) {
    if (buffer != nullptr) {
        // init method must be called only once
        return;
    }

    this->c64emu = c64emu;

    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    // init buffer
    buffer            = new uint8_t[256];
    keypresseddowncnt = NUMOFCYCLES_KEYPRESSEDDOWN;
    keypresseddown    = false;
    sentdc01          = 0xff;
    sentdc00          = 0xff;

    // init div
    virtjoystickvalue = 0xff;
    detectreleasekey  = false;
}

void KonsoolKB::handleKeyPress() {
    bsp_input_event_t event;
    // shiftctrlcode = second byte bit 0 -> left shift, bit 1 -> ctrl, bit 2 -> commodore, bit 7 -> external command
    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
        key_hold = 128;
        switch (event.type) {
            case INPUT_EVENT_TYPE_KEYBOARD: {
                sentdc00 = event.args_keyboard.ascii;
                sentdc01 = event.args_keyboard.modifiers;
                break;
            }
            case INPUT_EVENT_TYPE_NAVIGATION: {
                break;
            }
            case INPUT_EVENT_TYPE_ACTION: {
                break;
            }
            default:
                break;
        }
    } else {
        if (key_hold == 0) {
            sentdc00 = 0xff;
            sentdc01 = 0xff;
        } else {
            key_hold--;
        }
    }
}

// TODO ??
uint8_t KonsoolKB::getdc01(uint8_t querydc00, bool xchgports) {
    uint8_t kbcode1;
    uint8_t kbcode2;
    if (xchgports) {
        kbcode1 = sentdc01;
        kbcode2 = sentdc00;
    } else {
        kbcode1 = sentdc00;
        kbcode2 = sentdc01;
    }
    if (querydc00 == 0) {
        return kbcode2;
    }
    // special case "shift" + "commodore"
    if ((shiftctrlcode & 5) == 5) {
        if (querydc00 == kbcode1) {
            return kbcode2;
        } else {
            return 0xff;
        }
    }
    // key combined with a "special key" (shift, ctrl, commodore)?
    if ((~querydc00 & 2) && (shiftctrlcode & 1)) {  // *query* left shift key?
        if (kbcode1 == 0xfd) {
            // handle scan of key codes in the same "row"
            return kbcode2 & 0x7f;
        } else {
            return 0x7f;
        }
    } else if ((~querydc00 & 0x40) && (shiftctrlcode & 1)) {  // *query* right shift key?
        if (kbcode1 == 0xbf) {
            // handle scan of key codes in the same "row"
            return kbcode2 & 0xef;
        } else {
            return 0xef;
        }
    } else if ((~querydc00 & 0x80) && (shiftctrlcode & 2)) {  // *query* ctrl key?
        if (kbcode1 == 0x7f) {
            // handle scan of key codes in the same "row"
            return kbcode2 & 0xfb;
        } else {
            return 0xfb;
        }
    } else if ((~querydc00 & 0x80) && (shiftctrlcode & 4)) {  // *query* commodore key?
        if (kbcode1 == 0x7f) {
            // handle scan of key codes in the same "row"
            return kbcode2 & 0xdf;
        } else {
            return 0xdf;
        }
    }
    // query "main" key press
    if (querydc00 == kbcode1) {
        return kbcode2;
    } else {
        return 0xff;
    }
}

uint8_t KonsoolKB::getKBJoyValue(bool port2) {
    return virtjoystickvalue;
}

void KonsoolKB::setKbcodes(uint8_t sentdc01, uint8_t sentdc00) {
    this->sentdc01 = sentdc01;
    this->sentdc00 = sentdc00;
}
