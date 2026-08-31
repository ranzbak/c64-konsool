// Runs recorded gamepads through the joystick mapping and checks the C64 joystick byte.

#include <assert.h>
#include <stdio.h>
#include "esp_log.h"
#include "hid_gamepad.h"
#include "hid_gamepad_c64.h"
#include "test_descriptors.h"
#include "test_reports.h"

static const char TAG[] = "test";

#define SONY_VID       0x054c
#define DUALSHOCK3_PID 0x0268

// A gamepad nothing is known about beyond its report descriptor
#define UNKNOWN_VID 0x0000
#define UNKNOWN_PID 0x0000

static uint8_t feed(const uint8_t* report, int length) {
    hid_gamepad_handle_report(report, length);
    return hid_gamepad_get_c64_joy();
}

/// Stadia controller: directions from its hat switch, everything else fires
static void test_stadia(void) {
    assert(hid_gamepad_connect(gamepad1_desc, gamepad1_len, UNKNOWN_VID, UNKNOWN_PID));
    assert(hid_gamepad_connected());

    // Hat at 6 is left, sticks centered
    assert(feed(pad1_reports[0], 11) == (uint8_t)~HID_GAMEPAD_C64_LEFT);

    // Hat at 8 means centered, but the left stick is pushed down
    assert(feed(pad1_reports[1], 11) == (uint8_t)~HID_GAMEPAD_C64_DOWN);

    // Hat at 9 is out of range, so centered, and the sticks are at rest
    assert(feed(pad1_reports[2], 11) == HID_GAMEPAD_C64_IDLE);

    // Hat up, stick pushed down and left, and two buttons held: reported as it comes in
    assert(feed(pad1_reports[3], 11) == (uint8_t)~(HID_GAMEPAD_C64_UP | HID_GAMEPAD_C64_DOWN |
                                                   HID_GAMEPAD_C64_LEFT | HID_GAMEPAD_C64_FIRE));

    // Hat at 15 means centered, every button held
    assert(feed(pad1_reports[4], 11) == (uint8_t)~HID_GAMEPAD_C64_FIRE);

    hid_gamepad_disconnect();
    assert(!hid_gamepad_connected());
    assert(hid_gamepad_get_c64_joy() == HID_GAMEPAD_C64_IDLE);

    ESP_LOGI(TAG, "Stadia controller");
}

/// DualShock 4 clone: axes before the hat switch, and a hat that this parser has to find
static void test_dualshock4_clone(void) {
    assert(hid_gamepad_connect(gamepad2_desc, gamepad2_len, UNKNOWN_VID, UNKNOWN_PID));

    // Sticks centered, hat at 8 meaning centered
    assert(feed(pad2_reports[0], 64) == HID_GAMEPAD_C64_IDLE);

    // Same, with a button held
    assert(feed(pad2_reports[1], 64) == (uint8_t)~HID_GAMEPAD_C64_FIRE);

    // Left stick hard up and to the left
    assert(feed(pad2_reports[2], 64) == (uint8_t)~(HID_GAMEPAD_C64_UP | HID_GAMEPAD_C64_LEFT));

    hid_gamepad_disconnect();
    ESP_LOGI(TAG, "DualShock 4 clone");
}

/// DualShock 3: silent without its quirk, no hat switch, d-pad in its buttons
static void test_dualshock3(void) {
    assert(hid_gamepad_connect(gamepad3_desc, gamepad3_len, SONY_VID, DUALSHOCK3_PID));

    // Centered and nothing pressed
    assert(feed(pad3_reports[0], 11) == HID_GAMEPAD_C64_IDLE);

    // Cross is button fifteen, past the d-pad, so it fires rather than steering
    assert(feed(pad3_reports[1], 11) == (uint8_t)~HID_GAMEPAD_C64_FIRE);

    // Its quirk carries the feature report that gets it talking at all
    const hid_gamepad_quirk_t* quirk = hid_gamepad_find_quirk(SONY_VID, DUALSHOCK3_PID);
    assert(quirk != NULL);
    assert(quirk->enable_report != NULL);
    assert(quirk->enable_report_id == 0xf4);
    assert(quirk->dpad_first_button == 4);

    // Without the quirk its d-pad would fire instead of steering
    assert(hid_gamepad_find_quirk(UNKNOWN_VID, UNKNOWN_PID) == NULL);

    hid_gamepad_disconnect();
    ESP_LOGI(TAG, "DualShock 3");
}

/// Competition Pro: no report ID, and a stick that reports through X and Y rather than the hat
static void test_competition_pro(void) {
    assert(hid_gamepad_connect(gamepad4_desc, gamepad4_len, UNKNOWN_VID, UNKNOWN_PID));

    assert(feed(pad4_reports[0], 9) == HID_GAMEPAD_C64_IDLE);
    assert(feed(pad4_reports[1], 9) == (uint8_t)~HID_GAMEPAD_C64_LEFT);
    assert(feed(pad4_reports[2], 9) == (uint8_t)~(HID_GAMEPAD_C64_RIGHT | HID_GAMEPAD_C64_FIRE));

    hid_gamepad_disconnect();
    ESP_LOGI(TAG, "Competition Pro");
}

/// A mouse is not a joystick: its axes report how far it moved, not where it is, so there is
/// no center to push away from and nothing sensible to map to a direction
static void test_mice_are_rejected(void) {
    assert(!hid_gamepad_connect(mouse1_desc, mouse1_len, UNKNOWN_VID, UNKNOWN_PID));
    assert(!hid_gamepad_connect(mouse2_desc, mouse2_len, UNKNOWN_VID, UNKNOWN_PID));
    assert(!hid_gamepad_connect(mouse3_desc, mouse3_len, UNKNOWN_VID, UNKNOWN_PID));
    assert(!hid_gamepad_connected());
    assert(hid_gamepad_get_c64_joy() == HID_GAMEPAD_C64_IDLE);

    // Reports from a device that was never accepted do not move the joystick
    assert(feed(mouse1_reports[1], 8) == HID_GAMEPAD_C64_IDLE);

    ESP_LOGI(TAG, "Mice are left alone");
}

int main(void) {
    test_stadia();
    test_dualshock4_clone();
    test_dualshock3();
    test_competition_pro();
    test_mice_are_rejected();

    printf("All gamepad tests passed\n");
    return 0;
}
