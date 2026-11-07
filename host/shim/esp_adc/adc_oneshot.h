#pragma once
// The analogue joystick is not wired to anything on a PC. Only the handle type
// is needed to compile; the host runner never turns the joystick on.
#include "esp_err.h"
typedef void* adc_oneshot_unit_handle_t;
typedef int   adc_channel_t;
typedef int   adc_atten_t;
typedef int   adc_bitwidth_t;
typedef int   adc_unit_t;
#define ADC_UNIT_2        1
#define ADC_ATTEN_DB_12   3
#define ADC_ATTEN_DB_11   3
#define ADC_BITWIDTH_DEFAULT 0
typedef struct { adc_unit_t unit_id; int ulp_mode; } adc_oneshot_unit_init_cfg_t;
typedef struct { adc_atten_t atten; adc_bitwidth_t bitwidth; } adc_oneshot_chan_cfg_t;
static inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t*, adc_oneshot_unit_handle_t*) { return ESP_FAIL; }
static inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t, const adc_oneshot_chan_cfg_t*) { return ESP_FAIL; }
static inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int* out) { if (out) *out = 0; return ESP_FAIL; }
