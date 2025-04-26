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
#include "C64Emu.hpp"
#include <driver/gpio.h>
#include <string.h>
#include <cstdint>
// #include "Config.hpp"
#include "ExternalCmds.hpp"
// #include "HardwareInitializationException.h"
#include "VIC.hpp"
// #include "driver/timer_types_legacy.h"
// #include "esp_check.h"
#include "esp_err.h"
// #include "esp_log_level.h"
// #include "esp_rom_gpio.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
// #include "hal/gpio_types.h"
#include "portmacro.h"
#include "roms/charset.h"
#include "sid/sid.hpp"
extern "C" {
#include "bsp/battery.h"
// #include <esp_adc/adc_cali.h>
// #include <esp_adc/adc_cali_scheme.h>
// #include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
// #include <inttypes.h>
}

static const char* TAG = "C64Emu";

SemaphoreHandle_t C64Emu::lcdRefreshSem;

C64Emu* C64Emu::instance = nullptr;

void IRAM_ATTR C64Emu::interruptProfilingBatteryCheckFunc()
{
    // Retrieve the battery voltage
    bsp_battery_get_voltage((uint16_t*)&batteryVoltage);

    // profiling (if activated)
    if (!perf) {
        return;
    }
    // frames per second
    if (vic.cntRefreshs != 0) {
        ESP_LOGI(TAG, "fps: %d batv: %d", vic.cntRefreshs, (int)batteryVoltage);
    }
    vic.cntRefreshs            = 0;
    // number of cycles per second
    cpu.numofcyclespersecond   = 0;
    numofburnedcyclespersecond = 0;
}

bool C64Emu::updateTOD(CIA& cia)
{
    uint8_t dc08 = cia.latchRunDC08.load(std::memory_order_acquire);
    dc08++;
    if (dc08 > 9) {
        dc08            = 0;
        uint8_t dc09    = cia.latchRunDC09.load(std::memory_order_acquire);
        uint8_t dc09one = dc09 & 15;
        uint8_t dc09ten = dc09 >> 4;
        dc09one++;
        if (dc09one > 9) {
            dc09one = 0;
            dc09ten++;
            if (dc09ten > 5) {
                dc09ten         = 0;
                uint8_t dc0a    = cia.latchRunDC0A.load(std::memory_order_acquire);
                uint8_t dc0aone = dc0a & 15;
                uint8_t dc0aten = dc0a >> 4;
                dc0aone++;
                if (dc0aone > 9) {
                    dc0aone = 0;
                    dc0aten++;
                    if (dc0aten > 5) {
                        dc0aten         = 0;
                        uint8_t dc0b    = cia.latchRunDC0B.load(std::memory_order_acquire);
                        uint8_t dc0bone = dc0b & 15;
                        uint8_t dc0bten = dc0b >> 4;
                        bool    pm      = dc0b & 128;
                        dc0bone++;
                        if (((dc0bten == 0) && (dc0bone > 9)) || (dc0bten && (dc0bone > 1))) {
                            dc0bone = 0;
                            dc0bten++;
                            if (dc0bten > 1) {
                                dc0bten = 0;
                                pm      = !pm;
                            }
                        }
                        cia.latchRunDC0B.store(dc0bone | (dc0bten << 4) | (pm ? 127 : 0), std::memory_order_release);
                    }
                }
                cia.latchRunDC0A.store(dc0aone | (dc0aten << 4), std::memory_order_release);
            }
        }
        cia.latchRunDC09.store(dc09one | (dc09ten << 4), std::memory_order_release);
    }
    cia.latchRunDC08.store(dc08, std::memory_order_release);
    uint8_t alarmdc08 = cia.latchAlarmDC08.load(std::memory_order_acquire);
    if (dc08 == alarmdc08) {
        uint8_t dc09      = cia.latchRunDC09.load(std::memory_order_acquire);
        uint8_t alarmdc09 = cia.latchAlarmDC09.load(std::memory_order_acquire);
        if (dc09 == alarmdc09) {
            uint8_t dc0a      = cia.latchRunDC0A.load(std::memory_order_acquire);
            uint8_t alarmdc0a = cia.latchAlarmDC0A.load(std::memory_order_acquire);
            if (dc0a == alarmdc0a) {
                uint8_t dc0b      = cia.latchRunDC0B.load(std::memory_order_acquire);
                uint8_t alarmdc0b = cia.latchAlarmDC0B.load(std::memory_order_acquire);
                if (dc0b == alarmdc0b) {
                    return true;
                }
            }
        }
    }
    return false;
}

void IRAM_ATTR C64Emu::interruptTODFunc()
{
    if (cpu.cia1.isTODRunning.load(std::memory_order_acquire)) {
        if (updateTOD(cpu.cia1)) {
            cpu.cia1.isAlarm.store(true, std::memory_order_release);
        }
    }
    if (cpu.cia2.isTODRunning.load(std::memory_order_acquire)) {
        if (updateTOD(cpu.cia2)) {
            cpu.cia2.isAlarm.store(true, std::memory_order_release);
        }
    }
}

void C64Emu::handleKeyboardFunc()
{
    // check for keyboard inputs ca. each 8 ms
    checkForKeyboardCnt++;
    if (checkForKeyboardCnt == 8) {
        konsoolkb.handleKeyPress();
        checkForKeyboardCnt = 0;
    }
}

void C64Emu::cpuCode(void* parameter)
{
    ESP_LOGI(TAG, "cpuTask running on core %d", xPortGetCoreID());

    // init LCD driver
    vic.initLCDController();

    // interrupt each 100 ms to increment CIA real time clock (TOD)
    TimerHandle_t tod_timer = xTimerCreate("TOD Timer",
                                           pdMS_TO_TICKS(100),  // 100ms
                                           pdTRUE,              // auto-reload
                                           this,                // timer ID / context
                                           [](TimerHandle_t xTimer) {
                                               auto self = static_cast<C64Emu*>(pvTimerGetTimerID(xTimer));
                                               self->interruptTODFuncWrapper();
                                           });
    xTimerStart(tod_timer, 0);

    cpu.run();
    // cpu runs forever -> no vTaskDelete(NULL);
}

void C64Emu::powerOff()
{
#ifdef BOARD_T_HMI
    gpio_config_t io_conf;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << Config::PWR_ON);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)Config::PWR_ON, 0);
#endif
}

void C64Emu::setup()
{
    instance = this;
    ESP_LOGI(TAG, "Initializing C64 emulator");

    // allocate ram
    ram = new uint8_t[1 << 16];

    // Init I2S
    ESP_ERROR_CHECK(i2s.init());

    // init VIC
    vic.init(ram, charset_rom, &sid);

    // init ble keyboard
    konsoolkb.init(this);

    // init CPU
    cpu.init(ram, charset_rom, &vic, this);

    // init SID
    sid.init(cpu.getSidRegs(), [](int16_t* buf, size_t num) { instance->i2s.write(buf, num); }, 8580);

    // init Menu system
    menuController.init(this);

    // init ExternalCmds (must be initialized after cpu!)
    ESP_LOGI(TAG, "Initializing ExternalCmds");
    externalCmds.init(ram, this);

    // start cpu task
    xTaskCreatePinnedToCore(cpuCodeWrapper,  // Function to implement the task
                            "CPU",           // Name of the task
                            10000,           // Stack size in words
                            NULL,            // Task input parameter
                            19,              // Priority of the task
                            &cpuTask,        // Task handle
                            1);              // Core where the task should run

    // Interrupt handler for keyboard IO (keyboard)
    xTaskCreatePinnedToCore(handleKeyboardFuncWrapper,  // Keyboard task
                            "keyboardHandler",          //
                            4096,                       //
                            NULL,                       //
                            0,                          //
                            &interruptTask,             //
                            0);

    // Check FPS
    const esp_timer_create_args_t profiling_timer_args = {.callback        = &interruptProfilingBatteryCheckFuncWrapper,
                                                          .arg             = NULL,
                                                          .dispatch_method = ESP_TIMER_TASK,
                                                          .name            = "profiling_timer",
                                                          .skip_unhandled_events = true};
    esp_timer_create(&profiling_timer_args, &profiling_timer);
    esp_timer_start_periodic(profiling_timer, 1000000);  // in microseconds
}

void C64Emu::loop()
{
    vic.refresh(true);
}
