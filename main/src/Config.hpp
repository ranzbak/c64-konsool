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

#include <cstddef>
#include <cstdint>
#include "soc/gpio_num.h"
extern "C" {
  #include <esp_adc/adc_oneshot.h>
}


// #define BOARD_T_HMI
//#define BOARD_T_DISPLAY_S3
#define BOARD_KONSOOL

// #ifdef BOARD_KONSOOL
// extern size_t display_h_res;
// extern size_t display_v_res;
// #endif

struct Config {

#if defined(BOARD_T_HMI)
// TODO: Replace with Pax setup
// #define USE_ST7789V
#define USE_SDCARD
#define USE_JOYSTICK

  static const gpio_num_t PWR_EN = GPIO_NUM_10;
  static const gpio_num_t PWR_ON = GPIO_NUM_14;
  static const adc_channel_t BAT_ADC = ADC_CHANNEL_4; // GPIO5

  // ST7789V
  static const gpio_num_t BL = GPIO_NUM_38;
  static const gpio_num_t CS = GPIO_NUM_6;
  static const gpio_num_t DC = GPIO_NUM_7;
  static const gpio_num_t WR = GPIO_NUM_8;
  static const gpio_num_t D0 = GPIO_NUM_48;
  static const gpio_num_t D1 = GPIO_NUM_47;
  static const gpio_num_t D2 = GPIO_NUM_39;
  static const gpio_num_t D3 = GPIO_NUM_40;
  static const gpio_num_t D4 = GPIO_NUM_41;
  static const gpio_num_t D5 = GPIO_NUM_42;
  static const gpio_num_t D6 = GPIO_NUM_45;
  static const gpio_num_t D7 = GPIO_NUM_46;

  // DisplayDriver (considering a possible rotation)
  static const uint16_t LCDWIDTH = 320;
  static const uint16_t LCDHEIGHT = 240;
  static const gpio_num_t REFRESHDELAY = GPIO_NUM_1;

  // SDCard
  static const gpio_num_t SD_MISO_PIN = GPIO_NUM_13;
  static const gpio_num_t SD_MOSI_PIN = GPIO_NUM_11;
  static const gpio_num_t SD_SCLK_PIN = GPIO_NUM_12;

  // Joystick
  static const adc_channel_t ADC_JOYSTICK_X = ADC_CHANNEL_4;
  static const adc_channel_t ADC_JOYSTICK_Y = ADC_CHANNEL_5;
  static const gpio_num_t JOYSTICK_FIRE_PIN = GPIO_NUM_18;
  static const gpio_num_t JOYSTICK_FIRE2_PIN = GPIO_NUM_17;

#elif defined(BOARD_T_DISPLAY_S3)
#define USE_RM67162

  static const adc_channel_t BAT_ADC = ADC_CHANNEL_3; // GPIO4

  // DisplayDriver (considering a possible rotation)
  static const uint16_t LCDWIDTH = 536;
  static const uint16_t LCDHEIGHT = 240;
  static const uint8_t REFRESHDELAY = 13;
#endif
#if defined(BOARD_KONSOOL)
// TODO: Replace with pax setup
#define USE_SDCARD
#define USE_JOYSTICK
#define USE_PAX

  static const gpio_num_t PWR_EN = GPIO_NUM_10;
  static const gpio_num_t PWR_ON = GPIO_NUM_14;
  // static const adc_channel_t BAT_ADC = ADC_CHANNEL_4; // GPIO5

  // DisplayDriver (considering a possible rotation)
  static const uint16_t LCDWIDTH = 0;
  static const uint16_t LCDHEIGHT = 0;
  static const gpio_num_t REFRESHDELAY = GPIO_NUM_1;

  // SDCard
  static const gpio_num_t SD_MISO_PIN = GPIO_NUM_13;
  static const gpio_num_t SD_MOSI_PIN = GPIO_NUM_11;
  static const gpio_num_t SD_SCLK_PIN = GPIO_NUM_12;

  // Joystick
  static const adc_channel_t ADC_JOYSTICK_X = ADC_CHANNEL_4;
  static const adc_channel_t ADC_JOYSTICK_Y = ADC_CHANNEL_5;
  static const gpio_num_t JOYSTICK_FIRE_PIN = GPIO_NUM_18;
  static const gpio_num_t JOYSTICK_FIRE2_PIN = GPIO_NUM_17;


#elif defined(BOARD_T_DISPLAY_S3)
#define USE_RM67162

  static const adc_channel_t BAT_ADC = ADC_CHANNEL_3; // GPIO4

  // DisplayDriver (considering a possible rotation)
  static const uint16_t LCDWIDTH = 536;
  static const uint16_t LCDHEIGHT = 240;
  static const uint8_t REFRESHDELAY = 13;
#endif

  // BLEKB
  static constexpr const char *SERVICE_UUID =
      "695ba701-a48c-43f6-9028-3c885771f19f";
  static constexpr const char *CHARACTERISTIC_UUID =
      "3b05e9bf-086f-4b56-9c37-7b7eeb30b28b";

  // resolution of system timer (throttling 6502 CPU, get BLE KB codes)
  static const uint16_t INTERRUPTSYSTEMRESOLUTION = 1000;

  // number of "steps" to average throttling
  static const uint8_t THROTTELINGNUMSTEPS = 50;
}; // namespace Config
