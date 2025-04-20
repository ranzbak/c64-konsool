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
#include "menuoverlay/MenuTypes.hpp"
extern "C" {
#include <esp_log.h>
#include "bsp/audio.h"
#include "bsp/input.h"
}
#include <cstdint>
#include <cstring>
#include <konsoolled.hpp>
#include "C64Emu.hpp"
#include "ExternalCmds.hpp"
#include "Joystick.hpp"
#include "KonsoolKB.hpp"
#include "kbmatrix.hpp"

static const char* TAG = "KonsoolKB";

KonsoolKB::KonsoolKB()
{
    buffer = nullptr;
}

QueueHandle_t input_event_queue = NULL;

void KonsoolKB::init(C64Emu* c64emu)
{
    if (buffer != nullptr) {
        // init method must be called only once
        return;
    }

    this->c64emu         = c64emu;
    this->konsoleled     = new KonsoleLED();
    this->menuController = &c64emu->menuController;

    konsoleled->init();

    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    // init buffer
    buffer   = new uint8_t[256];
    sentdc01 = 0xff;
    sentdc00 = 0xff;

    // init div
    virtjoystickvalue = 0xff;
    detectreleasekey  = false;
}

void KonsoolKB::handleKeyPress()
{
    bsp_input_event_t event;
    uint8_t           key_code;
    static bool       keys_pressed[128];
    static uint16_t   repeat_delay = 0;

    // Reset C64 key matrix
    sentdc00 = 0xff;
    sentdc01 = 0xff;

    if (this->display == nullptr) {
        this->display = c64emu->cpu.vic->getDriver();
    }
    // Sync menu state with menu draw routine
    display->enableMenuOverlay(menuController->getVisible());

    while (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1))) {
        // use Keycodes to keep track of pressed keys
        key_code = event.args_scancode.scancode;
        switch (event.type) {
            case INPUT_EVENT_TYPE_SCANCODE: {
                keys_pressed[key_code & 0x7f] = (key_code & 0x80) ? false : true;
                if (key_code == 0x40) {
                    menuController->toggle();
                }
                if (key_code == 0x3f) {  // Switch between joystick port 1 & 2
                    int cur_port = menuDataStore->getInt("kb_joystick_port", 1);
                    menuDataStore->set("kb_joystick_port", cur_port == 1 ? 2 : 1);
                    // TODO: Remove me later
                    cur_port = menuDataStore->getInt("kb_joystick_port", 1);
                    ESP_LOGI(TAG, "Switched to joystick port %d", cur_port);
                }
                break;
            }
            default:
                break;
        }
    }

    // Handle C64 keyboard matrix based on pressed keys
    if (menuController->getVisible()) {
        if (repeat_delay < 2) {
            repeat_delay++;
            return;
        }
        if (keys_pressed[0x48]) {  // UP key code
            ESP_LOGD(TAG, "Handling UP key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_UP);
            repeat_delay = 0;
        } else if (keys_pressed[0x50]) {  // DOWN key code
            ESP_LOGD(TAG, "Handling DOWN key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_DOWN);
            repeat_delay = 0;
        } else if (keys_pressed[0x4b]) {  // LEFT key code
            ESP_LOGD(TAG, "Handling LEFT key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_LEFT);
            repeat_delay = 0;
        } else if (keys_pressed[0x4d]) {  // RIGHT key code
            ESP_LOGD(TAG, "Handling RIGHT key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_RIGHT);
            repeat_delay = 0;
        } else if (keys_pressed[0x01]) {  // ESC key code
            ESP_LOGD(TAG, "Handling ESC key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_LAST);
            repeat_delay = 0;
        } else if (keys_pressed[0x1c]) {  // ENTER key code
            ESP_LOGD(TAG, "Handling ENTER key press");
            menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_SELECT);
            repeat_delay = 0;
        }
    } else if (menuDataStore->getBool("kb_joystick_emu")) {
        // TODO: Handle joystick input
        virtjoystickvalue = 0xff;
        // Allow UP, DOWN, LEFT, RIGHT, space for fire button
        if (keys_pressed[0x48]) {  // UP key code
            virtjoystickvalue = ~(1 << Joystick::C64JOYUP);
        }
        if (keys_pressed[0x50]) {  // DOWN key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYDOWN);
        }
        if (keys_pressed[0x4b]) {  // LEFT key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYLEFT);
        }
        if (keys_pressed[0x4d]) {  // RIGHT key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYRIGHT);
        }
        if (keys_pressed[0x2a] || keys_pressed[0x1d]) {  // SHIFT key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYFIRE);
        }
        // extra keys to make playing platform games easier
        // Right shift is up + right
        if (keys_pressed[0x36]) {  // RIGHT SHIFT key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYUP);
            virtjoystickvalue &= ~(1 << Joystick::C64JOYRIGHT);
        }
        // The '/' key is up + lift
        if (keys_pressed[0x35]) {  // '/' key code
            virtjoystickvalue &= ~(1 << Joystick::C64JOYUP);
            virtjoystickvalue &= ~(1 << Joystick::C64JOYLEFT);
        }
    }

    if (!menuController->getVisible() && virtjoystickvalue == 0xff) {
        shiftctrlcode = 0;

        for (int i = 0; i < 128; i++) {
            // shiftctrlcode = second byte bit 0 -> left shift, bit 1 -> ctrl, bit 2 -> commodore, bit 7 -> external
            // command
            if (i == 0x42 || i == 0x2a || i == 0x1d || i == 0x5d) {
                continue;
            }
            if (keys_pressed[i]) {
                // Translate C64 keyboard matrix to KonsoleLED layout
                // or it with the previous values
                KbMatrixEntry ent = kb_matrix[i];
                sentdc00          = sentdc00 & ent.sentdc00;
                sentdc01          = sentdc01 & ent.sentdc01;
            }
        }
        if (keys_pressed[0x42] || keys_pressed[0x2a]) {
            shiftctrlcode = 1;
        }
        if (keys_pressed[0x1d]) {
            shiftctrlcode |= 2;
        }
        if (keys_pressed[0x5d]) {
            shiftctrlcode |= 4;
        }
    }

    switch (event.type) {
        case INPUT_EVENT_TYPE_NAVIGATION: {
            konsoleled->set_led_color(0, 0xffff0000);
            konsoleled->show_led_colors();
            if (event.args_navigation.state == false) {
                break;
            }

            switch (event.args_navigation.key) {
                break;
                case BSP_INPUT_NAVIGATION_KEY_VOLUME_DOWN:
                    if (audio_volume > 0) {
                        audio_volume -= 5;
                        bsp_audio_set_volume(audio_volume);
                    };
                    break;
                case BSP_INPUT_NAVIGATION_KEY_VOLUME_UP:
                    if (audio_volume < 100) {
                        audio_volume += 5;
                        bsp_audio_set_volume(audio_volume);
                    };
                    break;
                default:
                    break;
            }
            break;
        }
        case INPUT_EVENT_TYPE_ACTION: {
            konsoleled->set_led_color(0, 0xff0000ff);
            konsoleled->show_led_colors();
            break;
        }
        default:
            break;
    }
    // Handle modifier keys
    // if (event.args_keyboard.modifiers & BSP_INPUT_MODIFIER_SHIFT_L) {
    //     shiftctrlcode = 1;
    // }
    if (event.args_keyboard.modifiers & BSP_INPUT_MODIFIER_CTRL) {
        shiftctrlcode |= 2;
    }
    if (event.args_keyboard.modifiers & BSP_INPUT_NAVIGATION_KEY_SUPER) {
        shiftctrlcode |= 4;
    }
}

uint8_t KonsoolKB::getdc01(uint8_t querydc00, bool xchgports)
{
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

uint8_t KonsoolKB::getKBJoyValue(bool port2)
{
    return virtjoystickvalue;
}

void KonsoolKB::setKbcodes(uint8_t sentdc01, uint8_t sentdc00)
{
    this->sentdc01 = sentdc01;
    this->sentdc00 = sentdc00;
}
