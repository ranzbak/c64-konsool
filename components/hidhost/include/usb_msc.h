#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback invoked when a USB MSC device is mounted or unmounted.
 *
 * @param mounted  true when the volume was just mounted at /usb,
 *                 false when it was unmounted.
 * @param arg      User-provided context pointer passed to usb_msc_init().
 */
typedef void (*usb_msc_event_cb_t)(bool mounted, void *arg);

/**
 * @brief Install the USB MSC class driver.
 *
 * Must be called after hid_kbd_init() (i.e., after the USB Host Library is
 * installed). Internally creates a background task that mounts the FAT volume
 * at /usb on connect and unmounts it on disconnect.
 *
 * @param cb   Callback invoked on mount/unmount from a background task. May be NULL.
 * @param arg  Passed verbatim to cb.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t usb_msc_init(usb_msc_event_cb_t cb, void *arg);

#ifdef __cplusplus
}
#endif
