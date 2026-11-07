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

// Secondary address command bits, as they travel on the bus after LISTEN or
// TALK. The low nibble is the channel number.
static const uint8_t IEC_SEC_MASK  = 0x0F;
static const uint8_t IEC_SEC_DATA  = 0x60;
static const uint8_t IEC_SEC_CLOSE = 0xE0;
static const uint8_t IEC_SEC_OPEN  = 0xF0;

// One device on the serial bus, expressed as the protocol primitives rather
// than as high level operations like "load a file".
//
// This is deliberate. The Kernal builds LOAD, SAVE, OPEN, directory listings
// and everything else out of exactly these calls, so implementing them once
// covers all of it. It is also the seam for real drive emulation later: a
// 1541 with its own CPU answers the same primitives by clocking them over
// emulated serial lines, and nothing above this interface has to change.
class IecDevice {
   public:
    virtual ~IecDevice()
    {
    }

    virtual uint8_t deviceNumber() const = 0;

    // False when nothing would answer on the bus, which makes the Kernal
    // report DEVICE NOT PRESENT.
    virtual bool present() const = 0;

    // Bus commands. `secondary` arrives as sent, command bits included.
    virtual void listen(uint8_t secondary) = 0;
    virtual void talk(uint8_t secondary)   = 0;
    virtual void unlisten()                = 0;
    virtual void untalk()                  = 0;

    // CIOUT: a byte travelling to the device. False signals a device error.
    virtual bool write(uint8_t value) = 0;

    // ACPTR: a byte travelling from the device. `eoi` marks the last byte of
    // the file. False means there was nothing to read.
    virtual bool read(uint8_t* value, bool* eoi) = 0;
};
