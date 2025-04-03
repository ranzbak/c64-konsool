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

// #include "BLEKB.h"
#include "KonsoolKB.hpp"
#include "esp_attr.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
extern "C" {
#include "esp_timer.h"
#include "stdio.h"
}
#include "CPUC64.hpp"
#include "ConfigBoard.hpp"
// #include "ExternalCmds.hpp"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"

class C64Emu {
   private:
    static C64Emu* instance;  // needed for wrapper methods
    static void    interruptTODFuncWrapper() {
        if (instance != nullptr) {
            instance->interruptTODFunc();
        }
    }

    static void interruptSystemFuncWrapper(void* parameter) {
        while (true) {
            if (instance != nullptr) {
                instance->interruptSystemFunc();
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
    }
    // static void interruptProfilingBatteryCheckFuncWrapper() {
    //   if (instance != nullptr) {
    //     instance->interruptProfilingBatteryCheckFunc();
    //   }
    // }
    static void cpuCodeWrapper(void* parameter) {
        if (instance != nullptr) {
            instance->cpuCode(parameter);
        }
    }

    static SemaphoreHandle_t lcdRefreshSem;

    IRAM_ATTR static void gpioLcdTEISR(void* parameter) {
        SemaphoreHandle_t *semaphore = static_cast<SemaphoreHandle_t*>(parameter);
        if (instance != nullptr) {
            xSemaphoreGiveFromISR(semaphore, NULL);
        }
    }

    uint8_t*    ram;
    ConfigBoard configBoard;
    VIC         vic;

    uint16_t checkForKeyboardCnt        = 0;
    uint8_t  throttlingCnt              = 0;
    uint32_t numofburnedcyclespersecond = 0;

    uint16_t cntSecondsForBatteryCheck;

    esp_timer_handle_t* interruptProfilingBatteryCheck = NULL;
    esp_timer_handle_t* interruptTOD                   = NULL;
    esp_timer_handle_t* interruptSystem                = NULL;
    TaskHandle_t        cpuTask;
    TaskHandle_t        interruptTask;

    void interruptTODFunc();
    void interruptSystemFunc();
    // void interruptProfilingBatteryCheckFunc();
    void cpuCode(void* parameter);
    bool updateTOD(CIA& cia);

    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t         adc_cali_handle;

   public:
    CPUC64    cpu;
    // BLEKB blekb;
    KonsoolKB konsoolkb;
    // ExternalCmds externalCmds;
    bool      perf           = false;
    uint32_t  batteryVoltage = 0;

    void powerOff();
    void setup();
    void loop();
};
