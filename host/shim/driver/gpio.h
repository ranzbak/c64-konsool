#pragma once
// Nothing is wired to a PC. Enough of the GPIO API for the joystick to compile;
// the host runner never turns it on.
#include <cstdint>
#include "soc/gpio_num.h"
#include "esp_err.h"

typedef enum { GPIO_INTR_DISABLE = 0 } gpio_int_type_t;
typedef enum { GPIO_MODE_INPUT = 1, GPIO_MODE_OUTPUT = 2 } gpio_mode_t;
typedef enum { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE = 1 } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE = 1 } gpio_pulldown_t;

typedef struct {
    uint64_t        pin_bit_mask;
    gpio_mode_t     mode;
    gpio_pullup_t   pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

static inline esp_err_t gpio_config(const gpio_config_t*) { return ESP_OK; }
static inline int       gpio_get_level(gpio_num_t) { return 1; }
static inline esp_err_t gpio_set_level(gpio_num_t, uint32_t) { return ESP_OK; }
