#pragma once
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

#include <cstdint>
#include "soc/gpio_num.h"

#define C64_PAL_CPUCLK     985248.0
#define SID_CHANNEL_AMOUNT 3
#define LINES_PER_FRAME    312
#define PAL_FRAMERATE      50.1245419
#define PAL_TO_NTSC_RATIO  PAL_FRAMERATE / 60.0

#define BOARD_KONSOOL


struct Config {

#if defined(BOARD_KONSOOL)
#define USE_SDCARD
#define SD_CARD_MOUNT_POINT "/sdcard"
#define SD_CARD_PRG_PATH SD_CARD_MOUNT_POINT "/c64prg"
#define USE_JOYSTICK
#define USE_GFXP4
#define NEW_COMBINED_WAVEFORMS

    static const gpio_num_t PWR_EN = GPIO_NUM_10;
    static const gpio_num_t PWR_ON = GPIO_NUM_14;

    // LCD Display IO
    static const gpio_num_t LCDTE = GPIO_NUM_11;

    // SDCard
    static const gpio_num_t SD_MISO_PIN = GPIO_NUM_39;
    static const gpio_num_t SD_MOSI_PIN = GPIO_NUM_44;
    static const gpio_num_t SD_SCLK_PIN = GPIO_NUM_43;
    static const gpio_num_t SD_CS_PIN   = GPIO_NUM_42;

    // Joystick
    static const gpio_num_t JOYSTICK_LEFT     = GPIO_NUM_2;   // MTCK GPIO_2
    static const gpio_num_t JOYSTICK_RIGHT    = GPIO_NUM_3;   // MTDI GPIO_3
    static const gpio_num_t JOYSTICK_UP       = GPIO_NUM_4;   // MTMS GPIO_4
    static const gpio_num_t JOYSTICK_DOWN     = GPIO_NUM_15;  // SAO_IO1 GPIO_15
    static const gpio_num_t JOYSTICK_FIRE_PIN = GPIO_NUM_13;  // SAO_SCL GPIO_13
#endif

    // resolution of system timer (throttling 6502 CPU, get BLE KB codes)
    static const uint16_t INTERRUPTSYSTEMRESOLUTION = 1000;

    // number of "steps" to average throttling
    static const uint8_t THROTTELINGNUMSTEPS = 50;
};  // namespace Config
