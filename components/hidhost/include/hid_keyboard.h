#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USB host + HID host for boot keyboards
esp_err_t hid_kbd_init(void);
void handle_hid_events(void);

#ifdef __cplusplus
}
#endif
