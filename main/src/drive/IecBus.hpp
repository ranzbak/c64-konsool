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
#include "IecDevice.hpp"

// The serial bus, as far as the C64 side is concerned.
//
// LISTEN and TALK arrive before the secondary address does, so the bus holds
// the selected device until the secondary turns up and only then tells the
// device a transaction has started.
class IecBus {
   private:
    static const uint8_t MAX_DEVICES = 16;

    IecDevice* devices[MAX_DEVICES] = {};
    IecDevice* pendingListener      = nullptr;
    IecDevice* pendingTalker        = nullptr;
    IecDevice* listener             = nullptr;
    IecDevice* talker               = nullptr;

   public:
    void attach(IecDevice* device);
    void detach(uint8_t deviceNumber);
    bool hasDevices() const;

    // Each returns false when nothing on the bus answered, which the caller
    // turns into DEVICE NOT PRESENT.
    bool listen(uint8_t deviceNumber);
    bool talk(uint8_t deviceNumber);
    bool second(uint8_t secondary);
    bool tksa(uint8_t secondary);
    bool ciout(uint8_t value);
    bool acptr(uint8_t* value, bool* eoi);
    void unlisten();
    void untalk();
};
