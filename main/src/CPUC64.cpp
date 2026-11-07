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
#include "CPUC64.hpp"
#include <esp_log.h>
#include <esp_random.h>
#include <cstdint>
#if defined(HOST_BUILD)
#include "HostC64Emu.hpp"
#else
#include "C64Emu.hpp"
#endif
#include "JoystickInitializationException.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "roms/basic.h"
#include "roms/kernal.h"

static const uint8_t NUMCIACHECKS = 2;

static const char* TAG = "CPUC64";

// read dc00 / dc01:
// from
// https://retrocomputing.stackexchange.com/questions/6421/reading-both-keyboard-and-joystick-with-non-kernal-code-on-c64
// There may be multiple connections to ground, making this a "wired-OR" system:
// any one of these connections, if made to ground, will force the line low
// regardless of how any other connection is set. In other words, it's not
// possible for the line to be high if anything on that line wants to bring it
// low.
// => use "(cia1.ciareg[0x00] | ~ddra) & input;" instead of "(cia1.ciareg[0x00]
// & ddra) | (input & ~ddra);"

uint8_t CPUC64::getMem(uint16_t addr) {
    if ((!bankARAM) && ((addr >= 0xa000) && (addr <= 0xbfff))) {
        //    basic rom
        return basic_rom[addr - 0xa000];
    } else if ((!bankERAM) && (addr >= 0xe000)) {
        // kernal rom
        return kernal_rom[addr - 0xe000];
    } else if (bankDIO && (addr >= 0xd000) && (addr <= 0xdfff)) {
        // ** VIC **
        if (addr <= 0xd3ff) {
            uint8_t vicidx = (addr - 0xd000) % 0x40;
            if ((vicidx == 0x1e) || (vicidx == 0x1f)) {
                uint8_t val         = vic->vicreg[vicidx];
                vic->vicreg[vicidx] = 0;
                return val;
            } else if (vicidx == 0x11) {
                uint8_t raster8 = (vic->rasterline >= 256) ? 0x80 : 0;
                return (vic->vicreg[0x11] & 0x7f) | raster8;
            } else {
                return vic->vicreg[vicidx];
            }
        }
        // ** SID resp RNG **
        else if (addr <= 0xd7ff) {
            uint8_t sididx = (addr - 0xd400) % 0x100;
            if (sididx == 0x1b) {
                uint32_t rand = esp_random();
                return (uint8_t)(rand & 0xff);
            } else if (sididx == 0x1c) {
                return 0;
            } else {
                return sidreg[sididx];
            }
        }
        // ** Colorram **
        else if (addr <= 0xdbff) {
            return vic->colormap[addr - 0xd800];
        }
        // ** CIA 1 **
        else if (addr <= 0xdcff) {
            uint8_t ciaidx = (addr - 0xdc00) % 0x10;
            if (ciaidx == 0x00) {
                // port 2
                uint8_t ddra = cia1.ciaReg[0x02];
                // check for keyboard input first, a pressed key wins over the joystick
                uint8_t input = c64emu->konsoolkb.getdc01(cia1.ciaReg[0x01], true);
                if (input == 0xff) {
                    // no key pressed -> return the value of whatever drives port 2
                    input = (joystickmode == 2) ? joystick.getValue() : getVirtJoyValue(true);
                }
                return (cia1.ciaReg[0x00] | ~ddra) & input;
            } else if (ciaidx == 0x01) {
                // port 1
                uint8_t ddrb = cia1.ciaReg[0x03];
                if (joystickmode == 2) {
                    // special case: handle fire2 button -> space key
                    if ((cia1.ciaReg[0x00] == 0x7f) && joystick.getFire2()) {
                        return 0xef;
                    }
                }
                // check for keyboard input first, a pressed key wins over the joystick
                uint8_t input = c64emu->konsoolkb.getdc01(cia1.ciaReg[0x00], false);
                if (input == 0xff) {
                    // no key pressed -> return the value of whatever drives port 1
                    input = (joystickmode == 1) ? joystick.getValue() : getVirtJoyValue(false);
                }
                return (cia1.ciaReg[0x01] | ~ddrb) & input;
            }
            return cia1.getCommonCIAReg(ciaidx);
        }
        // ** CIA 2 **
        else if (addr <= 0xddff) {
            uint8_t ciaidx = (addr - 0xdd00) % 0x10;
            if (ciaidx == 0x00) {
                uint8_t ddra  = cia2.ciaReg[0x02];
                // Bits 0 to 5 read back the port; bits 6 and 7 are the serial
                // bus, and are set when the line is released. Following
                // Frodo's MOS6526_2::ReadRegister.
                uint8_t value = static_cast<uint8_t>((cia2.ciaReg[0x00] | ~ddra) & 0x3f);
                if (!iecLines.clkLow()) value |= 0x40;
                if (!iecLines.dataLow()) value |= 0x80;
                return value;
            } else if (ciaidx == 0x01) {
                uint8_t ddrb = cia2.ciaReg[0x03];
                return cia2.ciaReg[0x01] | ~ddrb;
            } else if (ciaidx == 0x0d) {
                nmiAck = true;
            }
            return cia2.getCommonCIAReg(ciaidx);
        }
    } else if ((!bankDRAM) && (addr >= 0xd000) && (addr <= 0xdfff)) {
        // dxxx character rom
        return charrom[addr - 0xd000];
    } else if (addr == 0x0001) {
        return register1;
    }
    // ram
    return ram[addr];
}

void CPUC64::decodeRegister1(uint8_t val) {
    switch (val) {
        case 0:
        case 4:
            bankARAM = true;
            bankDRAM = true;
            bankERAM = true;
            bankDIO  = false;
            break;
        case 1:
            bankARAM = true;
            bankDRAM = false;
            bankERAM = true;
            bankDIO  = false;
            break;
        case 2:
            bankARAM = true;
            bankDRAM = false;
            bankERAM = false;
            bankDIO  = false;
            break;
        case 3:
            bankARAM = false;
            bankDRAM = false;
            bankERAM = false;
            bankDIO  = false;
            break;
        case 5:
            bankARAM = true;
            bankDRAM = true;
            bankERAM = true;
            bankDIO  = true;
            break;
        case 6:
            bankARAM = true;
            bankDRAM = true;
            bankERAM = false;
            bankDIO  = true;
            break;
        case 7:
            bankARAM = false;
            bankDRAM = true;
            bankERAM = false;
            bankDIO  = true;
            break;
    }
}

void CPUC64::adaptVICBaseAddrs(bool fromcia) {
    uint8_t  val        = vic->vicreg[0x18];
    uint16_t val1       = (val & 0xf0) << 6;
    uint16_t vicmem     = vic->vicmem;
    // screenmem is used for text mode and bitmap mode
    vic->screenmemstart = vicmem + val1;
    bool bmm            = vic->vicreg[0x11] & 32;
    if ((bmm) || fromcia) {
        if ((val & 8) == 0) {
            vic->bitmapstart = vicmem;
        } else {
            vic->bitmapstart = vicmem + 0x2000;
        }
    }
    uint16_t charmemstart;
    if ((!bmm) || fromcia) {
        // charactermem
        val1         = (val & 0x0e) << 10;
        charmemstart = vicmem + val1;
        if ((charmemstart == 0x1800) || (charmemstart == 0x9800)) {
            vic->charset = vic->chrom + 0x0800;
        } else if ((charmemstart == 0x1000) || (charmemstart == 0x9000)) {
            vic->charset = vic->chrom;
        } else {
            vic->charset = ram + charmemstart;
        }
    }
}

void CPUC64::applyCia2PortA() {
    uint8_t val  = cia2.ciaReg[0x00];
    uint8_t ddra = cia2.ciaReg[0x02];

    // Bits 0 and 1 pick the VIC bank, and they follow the port pins rather
    // than the latch: a bit left as an input floats high and reads as one
    // whatever was written to it. A fast loader bit-bangs this register
    // thousands of times a second and does not preserve those bits, precisely
    // because it has left them as inputs. Spindle sets the data direction to
    // $3c, so on real hardware the bank never moves; taking it from the
    // written value alone put the VIC on a different 16K on every handshake,
    // and it drew its bitmap, screen and charset out of whatever was there.
    uint8_t bank = static_cast<uint8_t>(val | ~ddra) & 3;
    switch (bank) {
        case 0:
            vic->vicmem = 0xc000;
            break;
        case 1:
            vic->vicmem = 0x8000;
            break;
        case 2:
            vic->vicmem = 0x4000;
            break;
        case 3:
            vic->vicmem = 0x0000;
            break;
    }
    adaptVICBaseAddrs(true);

    // The same register drives the serial bus, through inverting buffers. A
    // line is released only by driving its bit with a zero: a one pulls the
    // line low, and so does leaving the bit as an input, because the pin then
    // floats high through its pull up and the inverter turns that into a
    // pulled down line. Frodo builds the same value as "~pra & ddra", a set
    // bit meaning the line is high (src/CIA.cpp, write_pa).
    uint8_t released = static_cast<uint8_t>(~val & ddra);
    bool    atnWas   = iecLines.atnLow();
    iecLines.c64Atn  = (released & 0x08) == 0;
    iecLines.c64Clk  = (released & 0x10) == 0;
    iecLines.c64Data = (released & 0x20) == 0;
    if (iecLines.atnLow() != atnWas) {
        onAtnChanged();
    }
}

void CPUC64::setMem(uint16_t addr, uint8_t val) {
    if (bankDIO && (addr >= 0xd000) && (addr <= 0xdfff)) {
        // ** VIC **
        if (addr <= 0xd3ff) {
            uint8_t vicidx = (addr - 0xd000) % 0x40;
            if (vicidx == 0x11) {
                // only bit 7 of latch register d011 is used
                vic->latchd011      = val;
                vic->vicreg[vicidx] = val & 0x7f;
                adaptVICBaseAddrs(false);
            } else if (vicidx == 0x12) {
                vic->latchd012 = val;
            } else if (vicidx == 0x16) {
                vic->vicreg[vicidx] = val;
                adaptVICBaseAddrs(false);
            } else if (vicidx == 0x18) {
                vic->vicreg[vicidx] = val;
                adaptVICBaseAddrs(false);
            } else if (vicidx == 0x19) {
                // clear given bits
                /*
                // does not work for all games, e.g. bubble bobble
                uint8_t act = vic->vicreg[vicidx];
                act &= ~val;
                if ((act & 7) == 0) {
                  act &= ~0x80;
                }
                vic->vicreg[vicidx] = val;
                */
                vic->vicreg[vicidx] = 0;
            } else if ((vicidx == 0x1e) || (vicidx == 0x1f)) {
                vic->vicreg[vicidx] = 0;
            } else {
                vic->vicreg[vicidx] = val;
            }
        }
        // ** SID **
        else if (addr <= 0xd7ff) {
            uint8_t sididx = (addr - 0xd400) % 0x100;

            sidreg[sididx] = val;
        }
        // ** Colorram **
        else if (addr <= 0xdbff) {
            vic->colormap[addr - 0xd800] = val;
        }
        // ** CIA 1 **
        else if (addr <= 0xdcff) {
            uint8_t ciaidx = (addr - 0xdc00) % 0x10;
            if (ciaidx == 0x00) {
                uint8_t ddra        = cia1.ciaReg[0x02];
                cia1.ciaReg[ciaidx] = (cia1.ciaReg[ciaidx] & ~ddra) | (val & ddra);
            } else if (ciaidx == 0x01) {
                uint8_t ddrb        = cia1.ciaReg[0x03];
                cia1.ciaReg[ciaidx] = (cia1.ciaReg[ciaidx] & ~ddrb) | (val & ddrb);
            } else {
                cia1.setCommonCIAReg(ciaidx, val);
            }
        }
        // ** CIA 2 **
        else if (addr <= 0xddff) {
            uint8_t ciaidx = (addr - 0xdd00) % 0x10;
            if (ciaidx == 0x00) {
                cia2.ciaReg[ciaidx] = val;
                applyCia2PortA();
            } else if (ciaidx == 0x02) {
                // The data direction decides which of the port's bits actually
                // drive the serial bus, and which of the VIC bank bits float
                // high, so both have to be recomputed when it changes and not
                // only when the port itself is written. A loader that turns ATN
                // into an output while the latch bit is already set asserts ATN
                // by writing here alone, and without this the drive never sees
                // the edge it is waiting for.
                cia2.setCommonCIAReg(ciaidx, val);
                applyCia2PortA();
            } else {
                cia2.setCommonCIAReg(ciaidx, val);
            }
        }
    }
    // ** register 1 **
    else if (addr == 0x0001) {
        register1 = val;
        decodeRegister1(register1 & 7);
    }
    // ** ram **
    else {
        ram[addr] = val;
    }
}

uint8_t *CPUC64::getSidRegs() {
    return sidreg;
}

// Kernal jump table entries for the serial routines. Each holds a JMP to the
// real routine, so the address to trap is read from the ROM rather than being
// hardcoded to a particular Kernal revision.
struct IecTrapVector {
    uint16_t    jumpTableAddr;
    const char* name;
};

static const IecTrapVector kIecTrapVectors[CPUC64::IEC_TRAP_COUNT] = {
    {0xFFB1, "LISTEN"},  // IEC_TRAP_LISTEN
    {0xFFB4, "TALK"},    // IEC_TRAP_TALK
    {0xFF93, "SECOND"},  // IEC_TRAP_SECOND
    {0xFF96, "TKSA"},    // IEC_TRAP_TKSA
    {0xFFA8, "CIOUT"},   // IEC_TRAP_CIOUT
    {0xFFA5, "ACPTR"},   // IEC_TRAP_ACPTR
    {0xFFAB, "UNTLK"},   // IEC_TRAP_UNTLK
    {0xFFAE, "UNLSN"},   // IEC_TRAP_UNLSN
};

// Kernal status byte, and the values the serial routines report through it.
// Taken from Frodo's IEC.h, which is the reference for what these routines
// hand back: a write with nobody listening reports $03 rather than just the
// write timeout bit, and a read past the end reports $02 on its own.
//
// Frodo (C) 1994-1997, 2002 Christian Bauer, GPL version 2 or later.
static const uint16_t KERNAL_STATUS       = 0x90;
static const uint8_t  STATUS_READ_TIMEOUT = 0x02;  // Frodo ST_READ_TIMEOUT
static const uint8_t  STATUS_TIMEOUT      = 0x03;  // Frodo ST_TIMEOUT
static const uint8_t  STATUS_EOI          = 0x40;  // Frodo ST_EOF
static const uint8_t  STATUS_NOT_PRESENT  = 0x80;  // Frodo ST_NOTPRESENT

static const uint8_t OPCODE_JAM = 0x02;
static const uint8_t OPCODE_JMP = 0x4C;
static const uint8_t OPCODE_RTS = 0x60;

bool CPUC64::installIecTraps() {
    if (iecTrapsActive) return true;

    // Resolve every routine first, so a Kernal that does not look the way we
    // expect leaves the ROM untouched rather than half patched.
    uint16_t resolved[IEC_TRAP_COUNT];
    for (int i = 0; i < IEC_TRAP_COUNT; i++) {
        uint16_t entry = kIecTrapVectors[i].jumpTableAddr;
        uint16_t off   = entry - 0xE000;
        if (kernal_rom[off] != OPCODE_JMP) {
            ESP_LOGE(TAG, "kernal jump table entry for %s is not a JMP", kIecTrapVectors[i].name);
            return false;
        }
        resolved[i] = static_cast<uint16_t>(kernal_rom[off + 1] | (kernal_rom[off + 2] << 8));
        if (resolved[i] < 0xE000) {
            ESP_LOGE(TAG, "%s points outside the kernal at $%04x", kIecTrapVectors[i].name, resolved[i]);
            return false;
        }
    }

    for (int i = 0; i < IEC_TRAP_COUNT; i++) {
        iecTrapAddr[i]      = resolved[i];
        uint16_t off        = resolved[i] - 0xE000;
        iecTrapSavedByte[i] = kernal_rom[off];
        kernal_rom[off]     = OPCODE_JAM;
        ESP_LOGI(TAG, "trapped %s at $%04x", kIecTrapVectors[i].name, resolved[i]);
    }
    iecTrapsActive = true;
    return true;
}

void CPUC64::removeIecTraps() {
    if (!iecTrapsActive) return;
    for (int i = 0; i < IEC_TRAP_COUNT; i++) {
        kernal_rom[iecTrapAddr[i] - 0xE000] = iecTrapSavedByte[i];
    }
    iecTrapsActive = false;
}

// Services one trapped Kernal call. Returns false when the address is not one
// of ours, which means a real JAM.
bool CPUC64::handleIecTrap(uint16_t addr) {
    if (!iecTrapsActive) return false;

    int which = -1;
    for (int i = 0; i < IEC_TRAP_COUNT; i++) {
        if (iecTrapAddr[i] == addr) {
            which = i;
            break;
        }
    }
    if (which < 0) return false;

    switch (which) {
        case IEC_TRAP_LISTEN:
            if (!iecbus.listen(a)) ram[KERNAL_STATUS] |= STATUS_NOT_PRESENT;
            break;
        case IEC_TRAP_TALK:
            if (!iecbus.talk(a)) ram[KERNAL_STATUS] |= STATUS_NOT_PRESENT;
            break;
        case IEC_TRAP_SECOND:
            if (!iecbus.second(a)) ram[KERNAL_STATUS] |= STATUS_NOT_PRESENT;
            break;
        case IEC_TRAP_TKSA:
            if (!iecbus.tksa(a)) ram[KERNAL_STATUS] |= STATUS_NOT_PRESENT;
            break;
        case IEC_TRAP_CIOUT:
            if (!iecbus.ciout(a)) ram[KERNAL_STATUS] |= STATUS_TIMEOUT;
            break;
        case IEC_TRAP_ACPTR: {
            uint8_t value = 0;
            bool    eoi   = false;
            if (iecbus.acptr(&value, &eoi)) {
                a = value;
                if (eoi) ram[KERNAL_STATUS] |= STATUS_EOI;
            } else {
                a                   = 0;
                ram[KERNAL_STATUS] |= STATUS_READ_TIMEOUT;
            }
            // ACPTR leaves the byte in the accumulator, so mirror what a load
            // would have done to the flags.
            zflag = (a == 0);
            nflag = (a & 0x80) != 0;
            break;
        }
        case IEC_TRAP_UNTLK:
            iecbus.untalk();
            break;
        case IEC_TRAP_UNLSN:
            iecbus.unlisten();
            break;
        default:
            return false;
    }
    return true;
}

// ATN going low is the drive's cue to look at the bus, so it has to be woken
// even if it had parked itself.
bool CPUC64::enableTrueDrive(uint8_t* romImage, DiskImage* image) {
    if (romImage == nullptr) return false;

    // The traps and the real drive cannot both answer device 8.
    removeIecTraps();

    drive.setLines(&iecLines);
    drive.setRom(romImage);
    drive.setDisk(image);
    if (!drive.ready()) return false;

    iecLines.reset();
    drive.reset();
    driveCycleDebt   = 0;
    trueDriveEnabled = true;
    ESP_LOGI(TAG, "true drive emulation on");
    return true;
}

void CPUC64::disableTrueDrive() {
    if (!trueDriveEnabled) return;
    trueDriveEnabled = false;
    drive.setDisk(nullptr);
    iecLines.reset();
    driveCycleDebt = 0;
    ESP_LOGI(TAG, "true drive emulation off");
}

void CPUC64::onAtnChanged() {
    if (!trueDriveEnabled) return;
    // The acknowledge gate depends on ATN, so the drive's outputs change the
    // moment ATN does, without the drive CPU running.
    drive.refreshIecOutputs();
}

// Runs the C64 up to `untilCycles` of this rasterline, giving the drive its
// turn along the way.
//
// The 1541 runs at very nearly the same speed as the C64, so it earns a cycle
// for every cycle the C64 spends. Following Frodo, the two are alternated an
// instruction at a time rather than a cycle at a time: whichever has fallen
// behind runs next. That is close enough for loaders that hand bytes back and
// forth over the bus, and far cheaper than stepping both cycle by cycle.
void CPUC64::executeUntilCycle(uint8_t untilCycles) {
    bool withDrive = trueDriveEnabled && drive.ready();

    while (numofcycles < untilCycles) {
        if (cpuhalted) {
            break;
        }

        if (withDrive && driveCycleDebt > 0) {
            driveCycleDebt -= static_cast<int>(drive.stepInstruction());
            continue;
        }

        logDebugInfo();
        uint8_t before = numofcycles;
        execute(getMem(pc++));

        if (withDrive) {
            // The drive owes itself whatever the C64 just spent.
            driveCycleDebt += static_cast<int>(static_cast<uint8_t>(numofcycles - before));
        }
    }
}

void CPUC64::emulateDriveCycles(unsigned int c64Cycles) {
    if (!trueDriveEnabled || !drive.ready()) return;

    // Timers keep running even while the drive is parked, so an interrupt it
    // is waiting on still arrives.
    drive.countTimers(c64Cycles);
}

void CPUC64::cmd6502halt() {
    // The JAM opcode has already been fetched, so the trapped routine starts
    // one byte back.
    if (handleIecTrap(pc - 1)) {
        execute(OPCODE_RTS);
        return;
    }
    cpuhalted = true;
    ESP_LOGE(TAG, "illegal code, cpu halted, pc = %x", pc - 1);
}

/*
void CPUC64::cmd6502nop1a() {
  ESP_LOGI(TAG, "nop: %d", micros());
  numofcycles += 2;
}
*/

uint8_t CPUC64::getA() {
    return a;
}

uint8_t CPUC64::getX() {
    return x;
}

uint8_t CPUC64::getY() {
    return y;
}

uint8_t CPUC64::getSP() {
    return sp;
}

uint8_t CPUC64::getSR() {
    return sr;
}

uint16_t CPUC64::getPC() {
    return pc;
}

void CPUC64::checkciatimers(uint8_t cycles) {
    // CIA 1 TOD alarm
    cia1.checkAlarm();
    // check for CIA 1 TOD alarm interrupt
    if (((cia1.latchDC0D & 0x84) == 0x84) && (!iflag)) {
        setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
    }
    // CIA 1 Timer A
    cia1.checkTimerA(cycles);
    // check for CIA 1 Timer A interrupt
    if (((cia1.latchDC0D & 0x81) == 0x81) && (!iflag)) {
        setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
    }
    // CIA 1 Timer B
    cia1.checkTimerB(cycles);
    // check for CIA 1 Timer B interrupt
    if (((cia1.latchDC0D & 0x82) == 0x82) && (!iflag)) {
        setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
    }
    // check for CIA 1 SDR interrupt
    if (((cia1.latchDC0D & 0x88) == 0x88) && (!iflag)) {
        setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
    }
    if (!deactivateCIA2) {
        // CIA 2 TOD alarm
        cia2.checkAlarm();
        // check for CIA 2 TOD alarm interrupt
        if (((cia2.latchDC0D & 0x84) == 0x84) && nmiAck) {
            nmiAck = false;
            setPCToIntVec(getMem(0xfffa) + (getMem(0xfffb) << 8), false);
        }
        // CIA 2 Timer A
        cia2.checkTimerA(cycles);
        // check for CIA 2 Timer A interrupt
        if (((cia2.latchDC0D & 0x81) == 0x81) && nmiAck) {
            nmiAck = false;
            setPCToIntVec(getMem(0xfffa) + (getMem(0xfffb) << 8), false);
        }
        // CIA 2 Timer B
        cia2.checkTimerB(cycles);
        // check for CIA 2 Timer B interrupt
        if (((cia2.latchDC0D & 0x82) == 0x82) && nmiAck) {
            nmiAck = false;
            setPCToIntVec(getMem(0xfffa) + (getMem(0xfffb) << 8), false);
        }
        // check for CIA 2 SDR interrupt
        if (((cia2.latchDC0D & 0x88) == 0x88) && nmiAck) {
            nmiAck = false;
            setPCToIntVec(getMem(0xfffa) + (getMem(0xfffb) << 8), false);
        }
    }
}

void CPUC64::logDebugInfo() {
    if (debug && (debuggingstarted || (debugstartaddr == 0) || (pc == debugstartaddr))) {
        debuggingstarted = true;
        // debug (use LOGE because LOGI doesn't work here...)
        ESP_LOGE(TAG, "pc: %2x, cmd: %s, a: %x, x: %x, y: %x, sp: %x, sr: %x", pc, cmdName[getMem(pc)], a, x, y, sp,
                 sr);
    }
}

void IRAM_ATTR CPUC64::run() {
    // pc *must* be set externally!
    cpuhalted             = false;
    debug                 = false;
    debugstartaddr        = 0;
    debuggingstarted      = false;
    numofcycles           = 0;
    static uint8_t badlinecycles = 0;
    static uint8_t spritecycles = 0;
    static int8_t cycles_extra  = 0;
    while (true) {
        if (cpuhalted) {
            continue;
        }

        // prepare next rasterline
        badlinecycles = vic->nextRasterline();
        spritecycles = vic->spriteDmaCycles();

        if (badlinecycles) {
            vic->screenHeight();
        }
        if (vic->screenblank) {
            badlinecycles = 0;
            spritecycles = 0;
        }
        // raster line interrupt?
        if ((vic->vicreg[0x19] & 0x81) && (vic->vicreg[0x1a] & 1) && (!iflag)) {
            setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
        }
        // execute CPU cycles and check CIA timers
        // (4 = average number of cycles for an instruction)
        // numofcycles is *not* cleared here: the counter is reset at the end of
        // the previous line, so the seven cycles of any interrupt entered since
        // then - the raster IRQ just above included - are charged against this
        // line's budget instead of being thrown away.
        uint8_t numofcyclestoexe = 63 - badlinecycles - spritecycles - cycles_extra;
        uint8_t n                = 1;
        if (badlinecycles == 0) {
            n = NUMCIACHECKS;
        }
        uint8_t tmp    = numofcyclestoexe / n;
        uint8_t sumtmp = tmp > 0 ? tmp : n;
        // Do CIA checks half way through the rasterline
        // But not during bad lines
        for (uint8_t i = 0; i < n - 1; i++) {
            executeUntilCycle(sumtmp);
            checkciatimers(tmp);
            sumtmp += tmp;
        }
        // Finish the raster line
        tmp = numofcyclestoexe - numofcycles;
        executeUntilCycle(numofcyclestoexe);

        emulateDriveCycles(numofcycles);

        // Make sure 63 cycles per rasterline on average
        cycles_extra = numofcycles - numofcyclestoexe;
        // Reset the counter here rather than at the top of the next line, so
        // that everything charged from now on - the sprite collision IRQ and
        // the restore NMI below, the CIA interrupts in checkciatimers, and the
        // raster IRQ at the top of the next line - lands in the next line's
        // count instead of being wiped out by the reset.
        numofcycles = 0;
        // TODO Add cycles access, > 63 gets subtracted from next rasterline
        checkciatimers(tmp);
        // draw rasterline
        vic->drawRasterline();
        // sprite collision interrupt?
        if ((vic->vicreg[0x19] & 0x86) && (vic->vicreg[0x1a] & 6) && (!iflag)) {
            setPCToIntVec(getMem(0xfffe) + (getMem(0xffff) << 8), false);
        }
        if (restorenmi && nmiAck) {
            nmiAck     = false;
            restorenmi = false;
            setPCToIntVec(getMem(0xfffa) + (getMem(0xfffb) << 8), false);
        }

        // throttle CPU at end of frame, and wait for the frame to be displayed
        if (vic->rasterline == 311) {
            xSemaphoreTake(frameRateMutex, 1000);
            // the menu can change the ports at any time, so pick the change up here
            // instead of hitting the data store on every single CIA 1 read
            updateJoystickPorts();
        }
    }
}

void CPUC64::initMemAndRegs() {
    ESP_LOGI(TAG, "CPUC64::initMemAndRegs");
    setMem(0, 0x2f);
    setMem(1, 0x37);
    setMem(0x8004, 0);
    sp            = 0xFF;
    iflag         = true;
    dflag         = false;
    bflag         = false;
    nmiAck        = true;
    restorenmi    = false;
    uint16_t addr = 0xfffc - 0xe000;
    pc            = kernal_rom[addr] + (kernal_rom[addr + 1] << 8);
}

void CPUC64::init(uint8_t* ram, uint8_t* charrom, VIC* vic, C64Emu* c64emu) {
    ESP_LOGI(TAG, "CPUC64::init");
    this->ram     = ram;
    this->charrom = charrom;
    this->vic     = vic;
    this->c64emu  = c64emu;
    measuredcycles.store(0, std::memory_order_release);
    adjustcycles.store(0, std::memory_order_release);
    joystickmode         = 0;
    kbjoystickmode       = 0;
    padjoystickmode      = 0;
    deactivateCIA2       = false;
    numofcycles          = 0;
    numofcyclespersecond = 0;
    try {
        joystick.init();
    } catch (const JoystickInitializationException& e) {
        ESP_LOGI(TAG, "error in init. of joystick: %s - continue anyway", e.what());
    }

    // Setup refresh rate semaphore
    frameRateMutex = xSemaphoreCreateBinary();

    // Setup Memory for first boot
    initMemAndRegs();
}

uint8_t CPUC64::getVirtJoyValue(bool port2) {
    // Virtual joystick sources: the emulated keyboard joystick and a USB gamepad. In
    // two player mode they sit on different ports, otherwise both drive the same one.
    // All values are active low, so anding them wires the sources together.
    uint8_t port = port2 ? 2 : 1;
    uint8_t val  = 0xff;
    if (kbjoystickmode == port) {
        val &= c64emu->konsoolkb.getKBJoyValue();
    }
    if (padjoystickmode == port) {
        val &= c64emu->konsoolkb.getGamepadJoyValue();
    }
    return val;
}

void CPUC64::updateJoystickPorts() {
    uint8_t port   = (uint8_t)menuDataStore->getInt("kb_joystick_port", 0);
    kbjoystickmode = port;
    // in two player mode the gamepad takes the other port, so the keyboard joystick
    // and the gamepad are two independent players
    padjoystickmode =
        (port != 0 && menuDataStore->getBool("two_player")) ? (uint8_t)(3 - port) : port;
}

void CPUC64::setPC(uint16_t newPC) {
    std::lock_guard<std::mutex> lock(pcMutex);
    pc = newPC;
}

void CPUC64::exeSubroutine(uint16_t addr, uint8_t rega, uint8_t regx, uint8_t regy) {
    bool     tcflag = cflag;
    bool     tzflag = zflag;
    bool     tdflag = dflag;
    bool     tbflag = bflag;
    bool     tvflag = vflag;
    bool     tnflag = nflag;
    bool     tiflag = iflag;
    uint8_t  ta     = a;
    uint8_t  tx     = x;
    uint8_t  ty     = y;
    uint8_t  tsp    = sp;
    uint8_t  tsr    = sr;
    uint16_t tpc    = pc;
    dflag           = false;
    bflag           = false;
    a               = rega;
    x               = regx;
    y               = regy;
    ram[0x033c]     = 0x20;  // jsr
    ram[0x033d]     = addr & 0xff;
    ram[0x033e]     = addr >> 8;
    pc              = 0x033c;
    while (true) {
        uint8_t nextopc = getMem(pc++);
        execute(nextopc);
        if ((sp == tsp) && (nextopc == 0x60)) {  // rts
            break;
        }
    }
    cflag = tcflag;
    zflag = tzflag;
    dflag = tdflag;
    bflag = tbflag;
    vflag = tvflag;
    nflag = tnflag;
    iflag = tiflag;
    a     = ta;
    x     = tx;
    y     = ty;
    sp    = tsp;
    sr    = tsr;
    pc    = tpc;
}

void CPUC64::setKeycodes(uint8_t keycode1, uint8_t keycode2) {
    c64emu->konsoolkb.setKbcodes(keycode1, keycode2);
}
