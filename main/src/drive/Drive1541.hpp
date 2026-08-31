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
#include "CPU6502.hpp"
#include "DiskController.hpp"
#include "DiskImage.hpp"
#include "IecLines.hpp"
#include "Via6522.hpp"

// A real 1541: its own 6502, 2K of RAM, the DOS ROM, and the two VIAs.
//
// This is what makes fast loaders work. They never call the Kernal's serial
// routines; they bit-bang the bus and upload their own code into the drive,
// so the only way to run them is to actually have a drive to upload into.
//
// The ROM is not shipped. It is a 16K copyrighted binary, so it is read off
// the SD card at start up and the drive stays disabled when it is missing.
class Drive1541 : public CPU6502 {
   public:
    static const unsigned int RAM_SIZE = 0x0800;  // 2K, mirrored up to $17FF
    static const unsigned int ROM_SIZE = 0x4000;  // 16K at $C000

   private:
    uint8_t  ram[RAM_SIZE];
    uint8_t* rom = nullptr;  // not owned; lives as long as the drive does

    Via6522   via1;  // serial bus
    Via6522   via2;  // disk hardware
    IecLines* lines = nullptr;

    DiskController controller;

    // Stepper phase last seen on VIA 2 port B, to tell which way the head
    // moved when it changes.
    uint8_t lastStepperPhase = 0;
    // The head and the stepper agree on real hardware; here they have to be
    // lined up once, on the first write after a reset.
    bool    stepperAligned = false;
    uint8_t stepperSkew    = 0;

    // Cycles counted towards the next BYTE READY pulse from the head.
    unsigned int byteReadyCycles = 0;
    // Whether the DOS took the last byte off the head. Reading port A moves
    // the head on by itself, so the byte clock only turns the disk when the
    // byte went unread.
    bool         headReadThisByte = false;
    // The byte last taken off the head. It stays on port A until the next
    // BYTE READY, so reading twice inside one byte time reads it twice.
    uint8_t      headByte         = 0;

    bool romLoaded = false;

    void updateIecOutputs();
    void updateStepper(uint8_t portB);
    // Runs the head's byte clock and pulses BYTE READY when a byte is due.
    void countByteReady(unsigned int cycles);

    uint8_t readVia1(uint8_t reg);
    uint8_t readVia2(uint8_t reg);
    void    writeVia1(uint8_t reg, uint8_t value);
    void    writeVia2(uint8_t reg, uint8_t value);

   public:
    Drive1541();
    ~Drive1541()
    {
    }

    // True when the drive has a ROM and can actually run.
    bool ready() const
    {
        return romLoaded;
    }

    // Points the drive at a ROM image already in memory. The buffer must
    // outlive the drive and be ROM_SIZE bytes.
    void setRom(uint8_t* romImage);

    void setLines(IecLines* busLines)
    {
        lines = busLines;
    }
    // Puts a different disk in with the drive still running, so a loader that
    // has uploaded its own code into the drive survives it.
    void swapDisk(DiskImage* image)
    {
        controller.swapDisk(image);
    }

    void setDisk(DiskImage* image)
    {
        controller.setDisk(image);
    }
    DiskController& disk()
    {
        return controller;
    }

    void reset();

    // Runs timers even while the drive is parked, so a pending interrupt
    // still arrives on time.
    void countTimers(unsigned int cycles);

    // Executes instructions until `cycles` is used up. Returns the number of
    // cycles actually consumed, which can overshoot by an instruction.
    unsigned int emulateCycles(unsigned int cycles);

    // Which track the head is over, for tests and probes.
    unsigned int trackForProbe() const
    {
        return controller.currentTrack();
    }

    unsigned int headPosForProbe() const
    {
        return controller.headPosition();
    }

    // Executes exactly one instruction and returns the cycles it took, so the
    // caller can interleave it against the C64.
    unsigned int stepInstruction();

    // Recomputes what the drive is pulling low. Needed when ATN moves, since
    // the acknowledge gate depends on it without the drive CPU running.
    void refreshIecOutputs();

    // CPU6502 hooks.
    void    run() override;
    uint8_t getMem(uint16_t addr) override;
    void    setMem(uint16_t addr, uint8_t val) override;
};
