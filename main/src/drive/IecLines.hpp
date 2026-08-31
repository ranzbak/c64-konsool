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

// The three signal lines of the serial bus.
//
// Every device drives them open collector: a line is low when *anyone* pulls
// it low, and only floats high when nobody is. That wired-and behaviour is
// the whole protocol, so it is modelled directly here rather than as a value
// one side writes and the other reads.
//
// The booleans below are "is this participant pulling the line low", so false
// means released.
class IecLines {
   public:
    // What the C64's CIA 2 is driving.
    bool c64Atn  = false;
    bool c64Clk  = false;
    bool c64Data = false;

    // What the drive's VIA 1 is driving.
    bool driveClk  = false;
    bool driveData = false;

    void reset()
    {
        c64Atn    = false;
        c64Clk    = false;
        c64Data   = false;
        driveClk  = false;
        driveData = false;
    }

    // Resulting line states. True means the line is pulled low.
    bool atnLow() const
    {
        return c64Atn;
    }
    bool clkLow() const
    {
        return c64Clk || driveClk;
    }
    bool dataLow() const
    {
        return c64Data || driveData;
    }
};
