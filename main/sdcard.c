#include "sdcard.h"
#include "bsp/storage.h"
#include "esp_err.h"
#include "esp_log.h"

static char const TAG[]         = "sdcard";
static const char mount_point[] = "/sd";

sd_status_t status = SD_STATUS_NOT_PRESENT;

// The BSP owns the SD card: it takes the LDO that powers it during
// bsp_device_initialize(), so nothing else can bring the card up.
esp_err_t sd_mount(void) {
    bool mounted = false;
    if (bsp_storage_get_mounted(BSP_STORAGE_TYPE_SDCARD, &mounted) == ESP_OK && mounted) {
        ESP_LOGI(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t res = bsp_storage_mount(BSP_STORAGE_TYPE_SDCARD, mount_point);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(res));
        status = SD_STATUS_ERROR;
        return res;
    }

    status = SD_STATUS_OK;
    return ESP_OK;
}

esp_err_t sd_unmount(void) {
    esp_err_t res = bsp_storage_unmount(BSP_STORAGE_TYPE_SDCARD);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(res));
        return res;
    }
    status = SD_STATUS_NOT_PRESENT;
    return ESP_OK;
}

sd_status_t sd_status(void) {
    return status;
}
