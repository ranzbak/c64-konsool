#pragma once
/*
 Copyright (C) 2025 Konsool 64 contributors

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

// A 6522 VIA, of which the 1541 has two: VIA 1 carries the serial bus, VIA 2
// drives the disk hardware.
//
// Only what the drive's ROM actually leans on is modelled: the two ports with
// their direction registers, timer 1 in free running and one shot mode, timer
// 2 counting down, and the interrupt flag and enable registers. The shift
// register is not used by the 1541 ROM.
class Via6522 {
   public:
    // Register indices, as seen at $1800 and $1C00 in the drive.
    enum Reg {
        REG_PRB    = 0x0,
        REG_PRA    = 0x1,
        REG_DDRB   = 0x2,
        REG_DDRA   = 0x3,
        REG_T1CL   = 0x4,
        REG_T1CH   = 0x5,
        REG_T1LL   = 0x6,
        REG_T1LH   = 0x7,
        REG_T2CL   = 0x8,
        REG_T2CH   = 0x9,
        REG_SR     = 0xA,
        REG_ACR    = 0xB,
        REG_PCR    = 0xC,
        REG_IFR    = 0xD,
        REG_IER    = 0xE,
        REG_PRA_NH = 0xF,  // port A without handshake
    };

    // Interrupt flag bits.
    static const uint8_t IRQ_CA2 = 0x01;
    static const uint8_t IRQ_CA1 = 0x02;
    static const uint8_t IRQ_SR  = 0x04;
    static const uint8_t IRQ_CB2 = 0x08;
    static const uint8_t IRQ_CB1 = 0x10;
    static const uint8_t IRQ_T2  = 0x20;
    static const uint8_t IRQ_T1  = 0x40;
    static const uint8_t IRQ_ANY = 0x80;

    uint8_t  pra  = 0;
    uint8_t  ddra = 0;
    uint8_t  prb  = 0;
    uint8_t  ddrb = 0;
    uint16_t t1c  = 0;
    uint16_t t1l  = 0;
    uint16_t t2c  = 0;
    uint8_t  t2ll = 0;
    uint8_t  sr   = 0;
    uint8_t  acr  = 0;
    uint8_t  pcr  = 0;
    uint8_t  ifr  = 0;
    uint8_t  ier  = 0;

    // In one shot mode timer 1 raises its flag once per load. The counter
    // keeps running and wraps, so without this it would flag again every time
    // it came round.
    bool t1Fired = false;

    // Last level seen on CA1, so only an edge raises the flag.
    bool ca1 = false;

    void reset();

    // CA1 is an input pin. The chip latches an interrupt on the edge PCR bit
    // 0 selects: clear for a falling edge, set for a rising one. On the 1541's
    // VIA 1 this pin is ATN coming in off the serial bus, which is how the
    // DOS learns it has been called.
    void setCa1(bool level);

    // Runs both timers for `cycles`, raising their interrupt flags. Timer 1
    // reloads by itself in free running mode.
    void countTimers(unsigned int cycles);

    // True when an enabled interrupt source is asserted.
    bool irqAsserted() const
    {
        return (ifr & ier & 0x7f) != 0;
    }

    // Reading a register. `portAInput` and `portBInput` are what the outside
    // world is presenting on the pins; bits configured as outputs come from
    // the port registers instead.
    uint8_t read(uint8_t reg, uint8_t portAInput, uint8_t portBInput);
    void    write(uint8_t reg, uint8_t value);

   private:
    void updateIrqFlag();
};
