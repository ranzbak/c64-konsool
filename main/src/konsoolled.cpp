#include "konsoolled.hpp"

extern "C" {
    #include "bsp/led.h"
    #include "esp_err.h"
}


void KonsoleLED::init() {
    // initialize LEDs
    ESP_ERROR_CHECK(bsp_led_initialize());

    for (uint8_t i = 0; i < 6; i++) {
        set_led_color(i, 0x000000);  // Black
    }
    show_led_colors();
}


void KonsoleLED::set_led_color(uint8_t led, uint32_t color) {
    led_buffer[led * 3 + 0] = (color >> 8) & 0xFF;  // G
    led_buffer[led * 3 + 1] = (color >> 16) & 0xFF; // R
    led_buffer[led * 3 + 2] = (color >> 0) & 0xFF;  // B
}


void KonsoleLED::show_led_colors() {
    ESP_ERROR_CHECK(bsp_led_write(led_buffer, sizeof(led_buffer)));
}
