#pragma once

#include <cstdint>

class KonsoleLED {

private:
    uint8_t led_buffer[6*3] = {0};

public:
    void init();
    void set_led_color(uint8_t led, uint32_t color);
    void show_led_colors();
};