#include "i2s.hpp"
#include <cstdint>
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/i2s_types.h"
#include "sid/sid.hpp"

extern "C" {
#include "bsp/audio.h"
}

static const char* TAG = "I2S";

esp_err_t I2S::init()
{
    // TODO: Move to a better place.
    ESP_LOGI(TAG, "Initializing BSP audio interface");
    bsp_audio_set_volume(0);
    esp_err_t res = bsp_audio_initialize();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing audio failed");
        return res;
    }

    // TODO: Add enable to menu
    ESP_LOGI(TAG, "Enable aplifier for audio output");
    bsp_audio_set_volume(80);
    bsp_audio_set_amplifier(true);

    ESP_LOGI(TAG, "Initializing I2S audio interface");
    // I2S audio
    chan_cfg = (i2s_chan_config_t)I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    res = i2s_new_channel(&chan_cfg, &i2s_handle, NULL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing I2S channel failed");
        return res;
    }


    i2s_std_config_t i2s_config = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)DEFAULT_SAMPLERATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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

// Union for left and right audio channel to uint32_t
union MonoToStereo {
    uint32_t val;
    struct {
        int16_t l;
        int16_t r;
    };
};

esp_err_t I2S::write(const int16_t* data, size_t size)
{
    size_t          bytes_written;
    static uint32_t stereo_sample;
    assert(sizeof(i2s_stereo_out) >= size * 2 * 2);  // 2 channels * 2 bytes per sample
    static int16_t swapped;

    for (size_t i = 0; i < size; i++) {
        // Convert the union to use int16_t to match the data type
        // swapped = ((uint16_t(data[i]) << 8) & 0xFF00) | ((uint16_t(data[i]) >> 8) & 0x00FF);
        swapped = data[i];

        stereo_sample = MonoToStereo{
            .l = swapped,
            .r = swapped
        }.val;

        reinterpret_cast<uint32_t*>(i2s_stereo_out)[i] = stereo_sample;
    }

    // size * 2 * 2 because of 2 channels (left and right) and 2 bytes per sample (int16_t)
    i2s_channel_write(i2s_handle, (uint8_t const*)i2s_stereo_out, size*2*2, &bytes_written, 12);
    if (bytes_written < size * 2 * 2) {
        ESP_LOGE(TAG, "Failed to write to I2S buffer %d != %d", bytes_written, size * 2 * 2);
        return ESP_FAIL;
    }
    return ESP_OK;
}