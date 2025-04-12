#pragma once 
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"

class I2S {
    private:
    int16_t i2s_stereo_out[256 * 2]; // stereo output buffer
    i2s_std_config_t i2s_config;
    i2s_chan_config_t chan_cfg;
    i2s_chan_handle_t i2s_handle;

    public:
    I2S() {}

    esp_err_t init();

    esp_err_t write(const int16_t* data, size_t size);
};

