/*
 Copyright (C) 2024 retroelec <retroelec42@gmail.com>

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 3 of the License, or (at your
 option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 For the complete text of the GNU General Public License see
 http://www.gnu.org/licenses/.
*/

#include "Joystick.hpp"
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <soc/gpio_struct.h>
#include <cstdint>
#include "Config.hpp"
#include "JoystickInitializationException.h"
#include "esp_err.h"

void Joystick::init() {
#ifdef USE_JOYSTICK
    // init gpio 
    gpio_config_t io_conf;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (
        1ULL << Config::JOYSTICK_FIRE_PIN
        | 1ULL << Config::JOYSTICK_DOWN
        | 1ULL << Config::JOYSTICK_UP
        | 1ULL << Config::JOYSTICK_LEFT
        | 1ULL << Config::JOYSTICK_RIGHT
    );

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        throw JoystickInitializationException(esp_err_to_name(err));
    }
#endif
}

uint8_t Joystick::getValue() {
    int     valueL, valueR, valueU, valueD, valueFire = 0;
#ifdef USE_JOYSTICK
    valueL = gpio_get_level(Config::JOYSTICK_LEFT);
    valueR = gpio_get_level(Config::JOYSTICK_RIGHT);
    valueU = gpio_get_level(Config::JOYSTICK_UP);
    valueD = gpio_get_level(Config::JOYSTICK_DOWN);
    valueFire = gpio_get_level(Config::JOYSTICK_FIRE_PIN);
#endif
    // C64 register value
    uint8_t value = 0xff;
    if (!valueL) {
        value &= ~(1 << C64JOYLEFT);
    } else if (!valueR) {
        value &= ~(1 << C64JOYRIGHT);
    }
    if (!valueD) {
        value &= ~(1 << C64JOYDOWN);
    } else if (!valueU) {
        value &= ~(1 << C64JOYUP);
    }
    if (!valueFire) {
        value &= ~(1 << C64JOYFIRE);
    }
    return value;
}

bool Joystick::getFire2() {
#if defined USE_JOYSTICK
    return ((GPIO.in.val >> Config::JOYSTICK_FIRE_PIN) & 0x01) == 0;
#else
    return false;
#endif
}
