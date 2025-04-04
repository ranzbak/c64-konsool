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
#include <konsoolled.hpp>

static const char* TAG = "KonsoolKB";

static const uint8_t NUMOFCYCLES_KEYPRESSEDDOWN = 16;

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

    this->c64emu     = c64emu;
    this->konsoleled = new KonsoleLED();

    konsoleled->init();

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
    uint8_t key_code;

    // shiftctrlcode = second byte bit 0 -> left shift, bit 1 -> ctrl, bit 2 -> commodore, bit 7 -> external command
    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
        shiftctrlcode  = 0;
        keypresseddown = NUMOFCYCLES_KEYPRESSEDDOWN;
        switch (event.type) {
            case INPUT_EVENT_TYPE_KEYBOARD: {
                ESP_LOGI(TAG, "Keyboard event %c (%02x) %s", event.args_keyboard.ascii,
                         (uint8_t)event.args_keyboard.ascii, event.args_keyboard.utf8);
                this->keypresseddown = true;
                this->key_hold       = true;
                
                key_code = event.args_keyboard.ascii;

                if (key_code >= 'A' && key_code <= 'Z') {
                    key_code += 'a' - 'A';
                    shiftctrlcode = 1;
                }

                // Translation table https://sta.c64.org/cbm64kbdlay.html
                switch (key_code) {
                    case 'a':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xfb;
                        break;
                    case 'b':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xef;
                        break;
                    case 'c':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xef;
                        break;
                    case 'd':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xfb;
                        break;
                    case 'e':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xbf;
                        break;
                    case 'f':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xdf;
                        break;
                    case 'g':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xfb;
                        break;
                    case 'h':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xdf;
                        break;
                    case 'i':
                        sentdc00 = 0xef;
                        sentdc01 = 0xfd;
                        break;
                    case 'j':
                        sentdc00 = 0xef;
                        sentdc01 = 0xfb;
                        break;
                    case 'k':
                        sentdc00 = 0xef;
                        sentdc01 = 0xdf;
                        break;
                    case 'l':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xfb;
                        break;
                    case 'm':
                        sentdc00 = 0xef;
                        sentdc01 = 0xef;
                        break;
                    case 'n':
                        sentdc00 = 0xef;
                        sentdc01 = 0x7f;
                        break;
                    case 'o':
                        sentdc00 = 0xef;
                        sentdc01 = 0xbf;
                        break;
                    case 'p':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xfd;
                        break;
                    case 'q':
                        sentdc00 = 0x7f;
                        sentdc01 = 0xbf;
                        break;
                    case 'r':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xfd;
                        break;
                    case 's':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xdf;
                        break;
                    case 't':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xbf;
                        break;
                    case 'u':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xbf;
                        break;
                    case 'v':
                        sentdc00 = 0xf7;
                        sentdc01 = 0x7f;
                        break;
                    case 'w':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xfd;
                        break;
                    case 'x':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xf7;
                        break;
                    case 'y':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xfd;
                        break;
                    case 'z':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xef;
                        break;
                    case '0':
                        sentdc00 = 0xef;
                        sentdc01 = 0xf7;
                        break;
                    case '!':
                        shiftctrlcode |= 0x01;
                    case '1':
                        sentdc00 = 0x7f;
                        sentdc01 = 0xfe;
                        break;
                    case '"':
                        shiftctrlcode |= 0x01;
                    case '2':
                        sentdc00 = 0x7f;
                        sentdc01 = 0xf7;
                        break;
                    case '#':
                        shiftctrlcode |= 0x01;
                    case '3':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xfe;
                        break;
                    case '$':
                        shiftctrlcode |= 0x01;
                    case '4':
                        sentdc00 = 0xfd;
                        sentdc01 = 0xf7;
                        break;
                    case '%':
                        shiftctrlcode |= 0x01;
                    case '5':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xfe;
                        break;
                    case '&':
                        shiftctrlcode |= 0x01;
                    case '6':
                        sentdc00 = 0xfb;
                        sentdc01 = 0xf7;
                        break;
                    case '`':
                        shiftctrlcode |= 0x01;
                    case '7':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xfe;
                        break;
                    case '(':
                        shiftctrlcode |= 0x01;
                    case '8':
                        sentdc00 = 0xf7;
                        sentdc01 = 0xf7;
                        break;
                    case ')':
                        shiftctrlcode |= 0x01;
                    case '9':
                        sentdc00 = 0xef;
                        sentdc01 = 0xfe;
                        break;
                    case ' ':
                        sentdc00 = 0x7f;
                        sentdc01 = 0xef;
                        break;
                    case '.':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xef;
                        break;
                    case '-':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xf7;
                        break;
                    case '+':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xfe;
                        break;
                    case '=':
                        sentdc00 = 0xbf;
                        sentdc01 = 0xdf;
                        break;
                    case ',':
                        sentdc00 = 0xdf;
                        sentdc01 = 0x7f;
                        break;
                    case '*':
                        sentdc00 = 0xbf;
                        sentdc01 = 0xfd;
                        break;
                    case '/':
                        sentdc00 = 0xbf;
                        sentdc01 = 0x7f;
                        break;
                    case ';':
                        sentdc00 = 0xbf;
                        sentdc01 = 0xfb;
                        break;
                    case ':':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xdf;
                        break;
                    case '@':
                        sentdc00 = 0xdf;
                        sentdc01 = 0xbf;
                        break;
                    default:
                        sentdc00 = 0xff;
                        sentdc01 = 0xff;
                        break;
                }
                // sentdc00             = event.args_keyboard.ascii;
                // sentdc01             = event.args_keyboard.modifiers;
                konsoleled->set_led_color(0, 0xff00ff00);
                konsoleled->show_led_colors();
                break;
            }
            case INPUT_EVENT_TYPE_NAVIGATION: {
                konsoleled->set_led_color(0, 0xffff0000);
                konsoleled->show_led_colors();

                switch (event.args_navigation.key) {
                    case BSP_INPUT_NAVIGATION_KEY_BACKSPACE:
                        sentdc00 = 0xfe;
                        sentdc01 = 0xfe;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RETURN:
                        sentdc00 = 0xfe;
                        sentdc01 = 0xfd;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_UP:
                        sentdc00 = 0xfe;
                        sentdc01 = 0x7f;
                        shiftctrlcode |= 1;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_DOWN:
                        sentdc00 = 0xfe;
                        sentdc01 = 0x7f;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_LEFT:
                        sentdc00 = 0xfe;
                        sentdc01 = 0xfb;
                        shiftctrlcode |= 1;
                        break;
                    case BSP_INPUT_NAVIGATION_KEY_RIGHT:
                        sentdc00 = 0xfe;
                        sentdc01 = 0xfb;
                        break;
                    default:
                        sentdc00 = 0xff;
                        sentdc01 = 0xff;
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
        if (event.args_keyboard.modifiers & BSP_INPUT_MODIFIER_SHIFT_L) {
            shiftctrlcode = 1;
        }
        if (event.args_keyboard.modifiers & BSP_INPUT_MODIFIER_CTRL) {
            shiftctrlcode |= 2;
        }
        if (event.args_keyboard.modifiers & BSP_INPUT_NAVIGATION_KEY_SUPER) {
            shiftctrlcode |= 4;
        }
    } else {
        if (keypresseddowncnt == 0) {
            this->keypresseddown = false;
            this->key_hold       = false;
            this->shiftctrlcode  = 0;
            sentdc00             = 0xff;
            sentdc01             = 0xff;
            konsoleled->set_led_color(0, 0xff000000);
            konsoleled->show_led_colors();
        } else {
            keypresseddowncnt--;
        }
    }
}

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
