// USB HID gamepad support: keeps the latest gamepad state around as a C64 joystick byte.
//
// Working out which way a pad is being pushed is the badgeteam/hid-host component's job, since
// the launcher needs exactly the same answer. What is left here is what those directions and
// buttons mean to a C64, which has one stick and one fire button and no opinions beyond that.

#include "hid_gamepad_c64.h"
#include "esp_log.h"
#include "hid_gamepad.h"

static const char* TAG = "hid_gamepad";

// Written by the HID host task, read by the emulator task
static volatile uint8_t joy_value = HID_GAMEPAD_C64_IDLE;

static hid_gamepad_t gamepad;

bool hid_gamepad_connected(void) {
    return hid_gamepad_is_open(&gamepad);
}

uint8_t hid_gamepad_get_c64_joy(void) {
    return joy_value;
}

void hid_gamepad_disconnect(void) {
    hid_gamepad_close(&gamepad);
    // Never leave a direction stuck when the gamepad is unplugged
    joy_value = HID_GAMEPAD_C64_IDLE;
}

bool hid_gamepad_connect(const uint8_t* report_descriptor, size_t length, uint16_t vid, uint16_t pid) {
    hid_gamepad_disconnect();
    return hid_gamepad_open(&gamepad, report_descriptor, length, vid, pid);
}

void hid_gamepad_handle_report(const uint8_t* data, int length) {
    hid_gamepad_state_t report;
    if (!hid_gamepad_decode(&gamepad, data, length, &report)) {
        return;
    }

    uint8_t value = HID_GAMEPAD_C64_IDLE;
    if (report.up) {
        value &= ~HID_GAMEPAD_C64_UP;
    }
    if (report.down) {
        value &= ~HID_GAMEPAD_C64_DOWN;
    }
    if (report.left) {
        value &= ~HID_GAMEPAD_C64_LEFT;
    }
    if (report.right) {
        value &= ~HID_GAMEPAD_C64_RIGHT;
    }
    // A C64 joystick has one button, so any of them fires. Gamepads disagree far too much about
    // which button is which for anything cleverer to be worth it here.
    if (report.buttons != 0) {
        value &= ~HID_GAMEPAD_C64_FIRE;
    }

    if (value != joy_value) {
        ESP_LOGD(TAG, "Joystick value %02X", value);
    }
    joy_value = value;
}
