#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Quirks, the report descriptor parser and the decoder that works out which way a pad is being
// pushed live in the badgeteam/hid-host component, since the launcher needs the same answer.
// Include its hid_gamepad.h for hid_gamepad_quirk_t and hid_gamepad_find_quirk(). What is left
// here is what a gamepad means to a C64.

// Bits of the C64 joystick byte, all active low
#define HID_GAMEPAD_C64_UP    (1 << 0)
#define HID_GAMEPAD_C64_DOWN  (1 << 1)
#define HID_GAMEPAD_C64_LEFT  (1 << 2)
#define HID_GAMEPAD_C64_RIGHT (1 << 3)
#define HID_GAMEPAD_C64_FIRE  (1 << 4)

// Idle value of the C64 joystick byte
#define HID_GAMEPAD_C64_IDLE 0xff

/// @brief Whether a usable USB gamepad is connected
bool hid_gamepad_connected(void);

/// @brief Latest gamepad state as a C64 joystick byte, active low
uint8_t hid_gamepad_get_c64_joy(void);

/// @brief Learn the report layout of a connected gamepad from its report descriptor
///
/// @return true when the descriptor describes something usable as a joystick
bool hid_gamepad_connect(const uint8_t *report_descriptor, size_t length, uint16_t vid, uint16_t pid);

/// @brief Forget the connected gamepad and release the joystick
void hid_gamepad_disconnect(void);

/// @brief Feed a raw HID input report from a gamepad, called by the HID host
void hid_gamepad_handle_report(const uint8_t *data, int length);

#ifdef __cplusplus
}
#endif
