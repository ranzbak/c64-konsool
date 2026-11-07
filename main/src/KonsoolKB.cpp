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
#include "hid_gamepad_c64.h"
}
#include <cstdint>
#include <cstring>
#include "C64Emu.hpp"
#include "ExternalCmds.hpp"
#include "Joystick.hpp"
#include "KonsoolKB.hpp"
#include "kbmatrix.hpp"

static const char* TAG = "KonsoolKB";

static bool fn_pressed = false;

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
    this->menuController = &c64emu->menuController;

    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    // init buffer
    buffer = new uint8_t[256];
    for (int i = 0; i < 8; i++) {
        keyarr[i]     = 0xff;
        rev_keyarr[i] = 0xff;
    }

    // init div
    virtjoystickvalue = 0xff;
    detectreleasekey  = false;
}

void KonsoolKB::handleKeyPress()
{
    bsp_input_event_t event;
    uint8_t           key_code;
    static bool       keys_pressed[128];

    // Reset C64 key matrix
    for (int i = 0; i < 8; i++) {
        keyarr[i]     = 0xff;
        rev_keyarr[i] = 0xff;
    }

    if (this->display == nullptr) {
        this->display = c64emu->cpu.vic->getDriver();
    }
    // Sync menu state with menu draw routine
    display->enableMenuOverlay(menuController->getVisible());

    while (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1))) {
        switch (event.type) {
            case INPUT_EVENT_TYPE_SCANCODE: {
                // use Keycodes to keep track of pressed keys
                key_code = event.args_scancode.scancode;
                if (key_code == BSP_INPUT_SCANCODE_ESCAPED_VOLUME_UP ||
                    key_code == BSP_INPUT_SCANCODE_ESCAPED_VOLUME_DOWN) {
                    continue;  // Ignore keys
                }
                keys_pressed[key_code & 0x7f] = (key_code & 0x80) ? false : true;
                if (key_code == BSP_INPUT_SCANCODE_F6) {
                    menuController->toggle();
                }
                if ((key_code & 0x7f) == BSP_INPUT_SCANCODE_FN) {
                    fn_pressed = (key_code & 0x80) ? false : true;
                    printf("FN changed: %u\r\n", fn_pressed);
                }
                if (key_code == BSP_INPUT_SCANCODE_F5) {  // Switch between joystick port 1 & 2
                    if (fn_pressed) {
                        uint8_t brightness = 0;
                        bsp_input_get_backlight_brightness(&brightness);
                        bsp_input_set_backlight_brightness(brightness > 0 ? 0 : 100);
                    } else {
                        int cur_port = menuDataStore->getInt("kb_joystick_port", 1);
                        menuDataStore->set("kb_joystick_port", cur_port == 1 ? 2 : 1);
                        // TODO: Remove me later
                        cur_port = menuDataStore->getInt("kb_joystick_port", 1);
                        ESP_LOGI(TAG, "Switched to joystick port %d", cur_port);
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_NONE);
                    }
                }
                if (key_code == BSP_INPUT_SCANCODE_TAB) {
                    c64emu->cpu.restorenmi = true;
                }
                // Handle C64 keyboard matrix based on pressed keys
                if (menuController->getVisible()) {
                    if (keys_pressed[BSP_INPUT_SCANCODE_KP8]) {  // UP key code
                        ESP_LOGD(TAG, "Handling UP key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_UP);
                    } else if (keys_pressed[BSP_INPUT_SCANCODE_KP2]) {  // DOWN key code
                        ESP_LOGD(TAG, "Handling DOWN key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_DOWN);
                    } else if (keys_pressed[BSP_INPUT_SCANCODE_KP4]) {  // LEFT key code
                        ESP_LOGD(TAG, "Handling LEFT key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_LEFT);
                    } else if (keys_pressed[BSP_INPUT_SCANCODE_KP6]) {  // RIGHT key code
                        ESP_LOGD(TAG, "Handling RIGHT key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_RIGHT);
                    } else if (keys_pressed[BSP_INPUT_SCANCODE_ESC]) {
                        ESP_LOGD(TAG, "Handling ESC key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_LAST);
                    } else if (keys_pressed[BSP_INPUT_SCANCODE_ENTER]) {
                        ESP_LOGD(TAG, "Handling ENTER key press");
                        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_SELECT);
                    }
                } else if (menuDataStore->getBool("kb_joystick_emu")) {
                    // TODO: Handle joystick input
                    virtjoystickvalue = 0xff;
                    // Allow UP, DOWN, LEFT, RIGHT, space for fire button
                    if (keys_pressed[BSP_INPUT_SCANCODE_KP8]) {  // UP key code
                        virtjoystickvalue = ~(1 << Joystick::C64JOYUP);
                    }
                    if (keys_pressed[BSP_INPUT_SCANCODE_KP2]) {  // DOWN key code
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYDOWN);
                    }
                    if (keys_pressed[BSP_INPUT_SCANCODE_KP4]) {  // LEFT key code
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYLEFT);
                    }
                    if (keys_pressed[BSP_INPUT_SCANCODE_KP6]) {  // RIGHT key code
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYRIGHT);
                    }
                    if (keys_pressed[BSP_INPUT_SCANCODE_LEFTSHIFT]) {
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYFIRE);
                    }
                    // extra keys to make playing platform games easier
                    // Right shift is up + right
                    if (keys_pressed[BSP_INPUT_SCANCODE_RIGHTSHIFT]) {
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYUP);
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYRIGHT);
                    }
                    // The '/' key is up + left
                    if (keys_pressed[BSP_INPUT_SCANCODE_SLASH]) {
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYUP);
                        virtjoystickvalue &= ~(1 << Joystick::C64JOYLEFT);
                    }
                }

                if (!menuController->getVisible() && virtjoystickvalue == 0xff) {
                    bool physical_left  = keys_pressed[BSP_INPUT_SCANCODE_LEFTSHIFT];
                    bool physical_right = keys_pressed[BSP_INPUT_SCANCODE_RIGHTSHIFT];
                    bool shift_pressed  = physical_left || physical_right;

                    bool virtual_shift   = false;
                    bool virtual_deshift = false;

                    for (int i = 0; i < 128; i++) {
                        if (!keys_pressed[i]) {
                            continue;
                        }
                        KbMatrixEntry ent = shift_pressed ? kb_matrix_shift[i] : kb_matrix[i];
                        if (ent.row < 0) {
                            continue;  // scancode has no C64 matrix mapping
                        }
                        keyarr[ent.row]     &= ~(1 << ent.col);
                        rev_keyarr[ent.col] &= ~(1 << ent.row);
                        if (ent.shift & 0x01) virtual_shift = true;    // VIRTUAL_SHIFT
                        if (ent.shift & 0x10) virtual_deshift = true;  // DESHIFT_SHIFT
                    }
                    if (virtual_deshift) {
                        virtual_shift = false;
                    }

                    bool lshift_bit =
                        (physical_left && !virtual_deshift) || (virtual_shift && !VSHIFT_IS_RSHIFT && !physical_right);
                    bool rshift_bit =
                        (physical_right && !virtual_deshift) || (virtual_shift && VSHIFT_IS_RSHIFT && !physical_left);
                    if (lshift_bit) {
                        keyarr[LSHIFT_ROW]     &= ~(1 << LSHIFT_COL);
                        rev_keyarr[LSHIFT_COL] &= ~(1 << LSHIFT_ROW);
                    } else {
                        keyarr[LSHIFT_ROW]     |= (1 << LSHIFT_COL);
                        rev_keyarr[LSHIFT_COL] |= (1 << LSHIFT_ROW);
                    }
                    if (rshift_bit) {
                        keyarr[RSHIFT_ROW]     &= ~(1 << RSHIFT_COL);
                        rev_keyarr[RSHIFT_COL] &= ~(1 << RSHIFT_ROW);
                    } else {
                        keyarr[RSHIFT_ROW]     |= (1 << RSHIFT_COL);
                        rev_keyarr[RSHIFT_COL] |= (1 << RSHIFT_ROW);
                    }
                }
                break;
            }
            case INPUT_EVENT_TYPE_NAVIGATION: {
                break;
            }
            default:
                break;
        }
    }

    handleGamepadMenuInput();
}

void KonsoolKB::handleGamepadMenuInput()
{
    // Gamepads report their state continuously, so only newly pressed directions count
    static uint8_t prev_joy = HID_GAMEPAD_C64_IDLE;

    uint8_t joy     = hid_gamepad_get_c64_joy();
    uint8_t pressed = (uint8_t)(prev_joy & ~joy);
    prev_joy        = joy;

    if (!menuController->getVisible()) {
        return;
    }

    if (pressed & HID_GAMEPAD_C64_UP) {
        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_UP);
    } else if (pressed & HID_GAMEPAD_C64_DOWN) {
        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_DOWN);
    } else if (pressed & HID_GAMEPAD_C64_LEFT) {
        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_LEFT);
    } else if (pressed & HID_GAMEPAD_C64_RIGHT) {
        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_RIGHT);
    } else if (pressed & HID_GAMEPAD_C64_FIRE) {
        menuController->handleInput(MENU_OVERLAY_INPUT_TYPE_SELECT);
    }
}

uint8_t KonsoolKB::getdc01(uint8_t querydc00, bool xchgports)
{
    const uint8_t* arr    = xchgports ? rev_keyarr : keyarr;
    uint8_t        result = 0xff;
    for (int row = 0; row < 8; row++) {
        if (!(querydc00 & (1 << row))) {
            result &= arr[row];
        }
    }
    return result;
}

uint8_t KonsoolKB::getKBJoyValue()
{
    return virtjoystickvalue;
}

uint8_t KonsoolKB::getGamepadJoyValue()
{
    return hid_gamepad_get_c64_joy();
}

void KonsoolKB::setKbcodes(uint8_t colmask, uint8_t rowmask)
{
    (void)rowmask;
    for (int row = 0; row < 8; row++) {
        keyarr[row] &= colmask;
    }
    for (int col = 0; col < 8; col++) {
        if (!(colmask & (1 << col))) {
            rev_keyarr[col] = 0x00;
        }
    }
}
