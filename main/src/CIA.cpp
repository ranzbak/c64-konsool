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
#include "CIA.hpp"

// bit 4 of ciareg[0x0e] and ciareg[0x0f] is handled in CPUC64::setMem

void CIA::checkAlarm() {
    if (isAlarm.load(std::memory_order_acquire)) {
        isAlarm.store(false, std::memory_order_release);
        latchDC0D |= 0x04;
        if (ciaReg[0x0d] & 4) {
            latchDC0D |= 0x80;
        }
    }
}

void CIA::checkTimerA(uint8_t deltaT) {
    uint8_t reg0e = ciaReg[0x0e];
    if (!(reg0e & 1)) {
        // timer stopped
        return;
    }
    if (reg0e & 0x20) {
        // timer clocked by CNT pin
        return;
    }
    int32_t tmp = timerA - deltaT;
    timerA      = (tmp < 0) ? 0 : tmp;
    if (timerA == 0) {
        underflowTimerA = true;
        if (reg0e & 0x02) {
            if (!(reg0e & 0x04)) {
                ciaReg[0x01] ^= 0x40;
            }
            // ignore "toggle bit for one cycle"
        }
        latchDC0D |= 0x01;
        if (!(reg0e & 8)) {
            timerA = (latchDC05 << 8) + latchDC04;
        } else {
            ciaReg[0x0e] &= 0xfe;
        }
        if (ciaReg[0x0d] & 1) {
            latchDC0D |= 0x80;
        }
        if ((ciaReg[0x0e] & 0x40) && (serBitNR != 0)) {
            serBitNR--;
            if (serBitNR == 0) {
                latchDC0D |= 0x08;
                if (ciaReg[0x0d] & 8) {
                    latchDC0D |= 0x80;
                }
                if (serBitNRNext != 0) {
                    serBitNR     = serBitNRNext;
                    serBitNRNext = 0;
                }
            }
        }
    }
}

void CIA::checkTimerB(uint8_t deltaT) {
    uint8_t reg0f = ciaReg[0x0f];
    if (!(reg0f & 1)) {
        // timer stopped
        return;
    }
    uint8_t bit56 = ciaReg[0x0f] & 0x60;
    if (bit56 == 0) {
        int32_t tmp = timerB - deltaT;
        timerB      = (tmp < 0) ? 0 : tmp;
    } else if (bit56 == 0x40) {
        if (underflowTimerA) {
            underflowTimerA = false;
            timerB--;
        }
    } else {
        return;
    }
    if (timerB == 0) {
        if (reg0f & 0x02) {
            if (!(reg0f & 0x04)) {
                ciaReg[0x01] ^= 0x80;
            }
            // ignore "toggle bit for one cycle"
        }
        latchDC0D |= 0x02;
        if (!(reg0f & 8)) {
            timerB = (latchDC07 << 8) + latchDC06;
        } else {
            ciaReg[0x0f] &= 0xfe;
        }
        if (ciaReg[0x0d] & 2) {
            latchDC0D |= 0x80;
        }
    }
}

void CIA::init(bool isCIA1) {
    for (uint8_t i = 0; i < 0x10; i++) {
        ciaReg[i] = 0;
    }

    underflowTimerA = false;
    serBitNR        = 0;
    serBitNRNext    = 0;
    latchDC04       = 0;
    latchDC05       = 0;
    latchDC06       = 0;
    latchDC07       = 0;
    latchDC0D       = 0;
    timerA          = 0;
    timerB          = 0;

    isTODRunning.store(false, std::memory_order_release);
    isTODFreezed = false;
    isAlarm.store(false, std::memory_order_release);
    latchRunDC08.store(0, std::memory_order_release);
    latchRunDC09.store(0, std::memory_order_release);
    latchRunDC0A.store(0, std::memory_order_release);
    latchRunDC0B.store(0, std::memory_order_release);
    latchAlarmDC08.store(0, std::memory_order_release);
    latchAlarmDC09.store(0, std::memory_order_release);
    latchAlarmDC0A.store(0, std::memory_order_release);
    latchAlarmDC0B.store(0, std::memory_order_release);

    if (isCIA1) {
        ciaReg[0] = 127;
        ciaReg[1] = 255;
        ciaReg[2] = 255;
    } else {
        ciaReg[0] = 151;
        ciaReg[1] = 255;
        ciaReg[2] = 63;
    }
}

CIA::CIA(bool isCIA1) {
    init(isCIA1);
}

uint8_t CIA::getCommonCIAReg(uint8_t ciaIdx) {
    if (ciaIdx == 0x04) {
        return timerA & 0xff;
    } else if (ciaIdx == 0x05) {
        return (timerA >> 8) & 0xff;
    } else if (ciaIdx == 0x06) {
        return timerB & 0xff;
    } else if (ciaIdx == 0x07) {
        return (timerB >> 8) & 0xff;
    } else if (ciaIdx == 0x08) {
        uint8_t val;
        if (isTODFreezed) {
            val = ciaReg[ciaIdx];
        } else {
            val = latchRunDC08.load(std::memory_order_acquire);
        }
        isTODFreezed = false;
        return val;
    } else if (ciaIdx == 0x09) {
        if (isTODFreezed) {
            return ciaReg[ciaIdx];
        } else {
            return latchRunDC09.load(std::memory_order_acquire);
        }
    } else if (ciaIdx == 0x0a) {
        if (isTODFreezed) {
            return ciaReg[ciaIdx];
        } else {
            return latchRunDC0A.load(std::memory_order_acquire);
        }
    } else if (ciaIdx == 0x0b) {
        isTODFreezed = true;
        ciaReg[0x08] = latchRunDC08.load(std::memory_order_acquire);
        ciaReg[0x09] = latchRunDC09.load(std::memory_order_acquire);
        ciaReg[0x0a] = latchRunDC0A.load(std::memory_order_acquire);
        ciaReg[0x0b] = latchRunDC0B.load(std::memory_order_acquire);
        return ciaReg[ciaIdx];
    } else if (ciaIdx == 0x0d) {
        uint8_t val = latchDC0D;
        latchDC0D   = 0;
        return val;
    } else {
        return ciaReg[ciaIdx];
    }
}

void CIA::setCommonCIAReg(uint8_t ciaIdx, uint8_t val) {
    if (ciaIdx == 0x04) {
        latchDC04 = val;
    } else if (ciaIdx == 0x05) {
        latchDC05 = val;
        // timerA stopped? if yes, write timerA
        if (!(ciaReg[0x0e] & 1)) {
            timerA = (latchDC05 << 8) + latchDC04;
        }
    } else if (ciaIdx == 0x06) {
        latchDC06 = val;
    } else if (ciaIdx == 0x07) {
        latchDC07 = val;
        // timerB stopped? if yes, write timerB
        if (!(ciaReg[0x0f] & 1)) {
            timerB = (latchDC07 << 8) + latchDC06;
        }
    } else if (ciaIdx == 0x08) {
        if (ciaReg[0x0f] & 128) {
            latchAlarmDC08.store(val, std::memory_order_release);
        } else {
            ciaReg[0x08] = val;
            latchRunDC08.store(val, std::memory_order_release);
            latchRunDC09.store(ciaReg[0x09], std::memory_order_release);
            latchRunDC0A.store(ciaReg[0x0a], std::memory_order_release);
            latchRunDC0B.store(ciaReg[0x0b], std::memory_order_release);
        }
        isTODRunning.store(true, std::memory_order_release);
    } else if (ciaIdx == 0x09) {
        if (ciaReg[0x0f] & 128) {
            latchAlarmDC09.store(val, std::memory_order_release);
        } else {
            ciaReg[0x09] = val;
        }
    } else if (ciaIdx == 0x0a) {
        if (ciaReg[0x0f] & 128) {
            latchAlarmDC0A.store(val, std::memory_order_release);
        } else {
            ciaReg[0x0a] = val;
        }
    } else if (ciaIdx == 0x0b) {
        isTODRunning.store(false, std::memory_order_release);
        if (ciaReg[0x0f] & 128) {
            latchAlarmDC0B.store(val, std::memory_order_release);
        } else {
            ciaReg[0x0b] = val;
        }
    } else if (ciaIdx == 0x0c) {
        if (serBitNR == 0) {
            serBitNR = 8;
        } else {
            serBitNRNext = 8;
        }
        ciaReg[ciaIdx] = val;
    } else if (ciaIdx == 0x0d) {
        if (val & 0x80) {
            ciaReg[ciaIdx] |= val;
        } else {
            ciaReg[ciaIdx] &= ~(val | 0x80);
        }
    } else if (ciaIdx == 0x0e) {
        ciaReg[ciaIdx] = val;
        if (val & 0x10) {
            timerA = (latchDC05 << 8) + latchDC04;
        }
    } else if (ciaIdx == 0x0f) {
        ciaReg[ciaIdx] = val;
        if (val & 0x10) {
            timerB = (latchDC07 << 8) + latchDC06;
        }
    } else {
        ciaReg[ciaIdx] = val;
    }
}
