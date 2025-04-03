#include "T_HMI.hpp"

void T_HMI::init() {
    gpio_config_t io_conf;
    io_conf.intr_type    = (gpio_int_type_t)GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = (1ULL << Config::PWR_ON) | (1ULL << Config::PWR_EN);
    esp_err_t err        = gpio_config(&io_conf);
    if (err != ESP_OK) {
        throw HardwareInitializationException(std::string("init. of BoardDriver failed: ") + esp_err_to_name(err));
    }
    gpio_set_level(Config::PWR_ON, 1);
    gpio_set_level(Config::PWR_EN, 1);
}