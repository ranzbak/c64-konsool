#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_vfs_fat.h"
#include "usb/msc_host_vfs.h"
#include "usb_msc.h"

static const char *TAG = "usb_msc";

#define USB_MSC_MOUNT_PATH "/usb"

/* ---- Internal state ---- */
static msc_host_device_handle_t s_msc_device = NULL;
static msc_host_vfs_handle_t    s_vfs_handle  = NULL;
static usb_msc_event_cb_t       s_user_cb     = NULL;
static void                    *s_user_cb_arg = NULL;

/* ---- Event queue ---- */
typedef enum {
    MSC_EVT_CONNECTED,
    MSC_EVT_DISCONNECTED,
} msc_queue_evt_id_t;

typedef struct {
    msc_queue_evt_id_t id;
    uint8_t usb_addr;
} msc_queue_msg_t;

static QueueHandle_t s_msc_queue = NULL;

/* Called from the MSC driver's internal task — must only enqueue, no blocking calls. */
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    ESP_LOGI(TAG, "MSC event: %s (addr=%d)",
             event->event == MSC_DEVICE_CONNECTED ? "CONNECTED" : "DISCONNECTED",
             (int)event->device.address);
    msc_queue_msg_t msg = {
        .usb_addr = event->device.address,
    };
    if (event->event == MSC_DEVICE_CONNECTED) {
        msg.id = MSC_EVT_CONNECTED;
        xQueueSend(s_msc_queue, &msg, 0);
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        msg.id = MSC_EVT_DISCONNECTED;
        xQueueSend(s_msc_queue, &msg, 0);
    }
}

static void msc_event_task(void *arg)
{
    msc_queue_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_msc_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (msg.id == MSC_EVT_CONNECTED) {
            ESP_LOGI(TAG, "MSC device connected (addr=%d)", msg.usb_addr);

            if (s_msc_device != NULL) {
                ESP_LOGW(TAG, "Already have a device mounted, ignoring");
                continue;
            }

            esp_err_t err = msc_host_install_device(msg.usb_addr, &s_msc_device);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "msc_host_install_device failed: %s", esp_err_to_name(err));
                s_msc_device = NULL;
                continue;
            }
            const esp_vfs_fat_mount_config_t mount_cfg = {
                .format_if_mount_failed   = false,
                .max_files                = 4,
                .allocation_unit_size     = 0,
                .disk_status_check_enable = false,
            };

            err = msc_host_vfs_register(s_msc_device, USB_MSC_MOUNT_PATH, &mount_cfg, &s_vfs_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "msc_host_vfs_register failed: %s", esp_err_to_name(err));
                msc_host_uninstall_device(s_msc_device);
                s_msc_device = NULL;
                continue;
            }

            ESP_LOGI(TAG, "USB drive mounted at %s", USB_MSC_MOUNT_PATH);
            if (s_user_cb) {
                s_user_cb(true, s_user_cb_arg);
            }

        } else if (msg.id == MSC_EVT_DISCONNECTED) {
            if (s_msc_device == NULL) {
                continue;
            }

            ESP_LOGI(TAG, "MSC device disconnected (addr=%d)", msg.usb_addr);

            if (s_vfs_handle) {
                msc_host_vfs_unregister(s_vfs_handle);
                s_vfs_handle = NULL;
            }
            msc_host_uninstall_device(s_msc_device);
            s_msc_device = NULL;

            if (s_user_cb) {
                s_user_cb(false, s_user_cb_arg);
            }
        }
    }
}

esp_err_t usb_msc_init(usb_msc_event_cb_t cb, void *arg)
{
    s_user_cb     = cb;
    s_user_cb_arg = arg;

    s_msc_queue = xQueueCreate(4, sizeof(msc_queue_msg_t));
    if (!s_msc_queue) {
        return ESP_ERR_NO_MEM;
    }

    const msc_host_driver_config_t msc_cfg = {
        .create_backround_task = true,  /* note: typo is in the SDK field name */
        .task_priority         = 5,
        .stack_size            = 4096,
        .core_id               = tskNO_AFFINITY,
        .callback              = msc_event_cb,
        .callback_arg          = NULL,
    };

    esp_err_t err = msc_host_install(&msc_cfg);
    if (err != ESP_OK) {
        vQueueDelete(s_msc_queue);
        s_msc_queue = NULL;
        return err;
    }

    BaseType_t created = xTaskCreate(msc_event_task, "msc_events", 4096, NULL, 4, NULL);
    if (created != pdTRUE) {
        msc_host_uninstall();
        vQueueDelete(s_msc_queue);
        s_msc_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "USB MSC driver installed, waiting for USB drive");
    return ESP_OK;
}
