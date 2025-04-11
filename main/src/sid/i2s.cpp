#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2s_types.h"
#include "i2s.hpp"

static const char* TAG = "I2S";

esp_err_t I2S::init() {

    ESP_LOGI(TAG, "Initializing I2S audio interface");
    // I2S audio
    chan_cfg = (i2s_chan_config_t) I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    esp_err_t res = i2s_new_channel(&chan_cfg, &i2s_handle, NULL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing I2S channel failed");
        return res;
    }

    i2s_std_config_t i2s_config = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .mclk = GPIO_NUM_30,
                .bclk = GPIO_NUM_29,
                .ws   = GPIO_NUM_31,
                .dout = GPIO_NUM_28,
                .din  = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv   = false,
                    },
            },
    };

    res = i2s_channel_init_std_mode(i2s_handle, &i2s_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Configuring I2S channel failed");
        return res;
    }

    res = i2s_channel_enable(i2s_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Enabling I2S channel failed");
        return res;
    }

    return ESP_OK;
}

esp_err_t I2S::write(const int16_t* data, size_t size) {
    return i2s_channel_write(i2s_handle, data, size, NULL, 10);
}