#pragma once 
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "VIC.hpp"

class I2S {
    private:
    uint8_t i2s_stereo_out[128 * 2 * 2]; // stereo output buffer
    i2s_chan_config_t chan_cfg;
    i2s_chan_handle_t i2s_handle;

    public:
    I2S() {}

    esp_err_t init();

    esp_err_t write(const int16_t* data, size_t size);
};

