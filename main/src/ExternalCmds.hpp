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

class C64Emu;

#include <cstdint>
#include <string>
#include "SDCard.hpp"
#include "drive/CbmDos.hpp"
#include "drive/D64Disk.hpp"

// notifications may be not larger than 20 bytes

struct BLENotificationStruct1 {
    uint8_t type;
    uint8_t joymode;
    uint8_t deactivateCIA2;
    uint8_t sendrawkeycodes;
    uint8_t switchdebug;
    uint8_t switchperf;
    uint8_t switchdetectreleasekey;
};

struct BLENotificationStruct2 {
    uint8_t  type;
    uint8_t  cpuRunning;
    uint16_t pc;
    uint8_t  a;
    uint8_t  x;
    uint8_t  y;
    uint8_t  sr;
    uint8_t  d011;
    uint8_t  d016;
    uint8_t  d018;
    uint8_t  d019;
    uint8_t  d01a;
    uint8_t  register1;
    uint8_t  dc0d;
    uint8_t  dc0e;
    uint8_t  dc0f;
    uint8_t  dd0d;
    uint8_t  dd0e;
    uint8_t  dd0f;
};

static const uint8_t BLENOTIFICATIONTYPE3NUMOFBYTES = 16;  // must be divisible by 8
struct BLENotificationStruct3 {
    uint8_t type;
    uint8_t mem[BLENOTIFICATIONTYPE3NUMOFBYTES];
};

struct BLENotificationStruct4 {
    uint8_t type;
};

struct BLENotificationStruct5 {
    uint8_t type;
    uint8_t batteryVolLow;
    uint8_t batteryVolHi;
};

class ExternalCmds {
   private:
    C64Emu*  c64emu;
    uint8_t* ram;
    bool     sendrawkeycodes;
    uint16_t actaddrreceivecmd;

    bool initialized = false;
    bool mounted     = false;
    bool trueDrive   = false;
    std::string mountedName;

    // The 1541 DOS ROM, read off the card. Kept here so it outlives the CPU
    // that points at it.
    uint8_t* driveRom = nullptr;
    bool loadDriveRom();

    void setVarTab(uint16_t addr);
    // Hands control back to the C64 after a load attempt, printing READY or an
    // error through the injected loadactions routine.
    void finishLoad(bool fileloaded, bool error);
    void setType1Notification();
    void setType2Notification();
    void setType3Notification(uint16_t addr);
    void setType4Notification();
    void setType5Notification(uint8_t batteryVolLow, uint8_t batteryVolHi);

   public:
    SDCard   sdcard;

    // Drive 8, backed by a mounted .d64.
    D64Disk  disk;
    CbmDos   dos;
    // TODO: Doesn't work need to look at later
    enum class ExtCmd;

    bool liststartflag;

    BLENotificationStruct1 type1notification;
    BLENotificationStruct2 type2notification;
    BLENotificationStruct3 type3notification;
    BLENotificationStruct4 type4notification;
    BLENotificationStruct5 type5notification;

    void    init(uint8_t* ram, C64Emu* c64emu);
    // Loads a bare .prg. `filename` carries no extension.
    bool    loadPrg(const char* filename);
    bool    loadPrgFromPath(const char* fullpath);
    // Loads any supported file from the program directory. `filename` includes
    // its extension; a .t64 or .d64 loads the first program it holds.
    bool    loadFile(const char* filename);
    // Loads one program out of a .t64 or .d64 by its index in entries().
    bool    loadImageEntry(const char* filename, uint16_t index);

    // The same three, addressed by full path rather than by a name in the
    // program directory, so a USB disk can be read as well as the card.
    bool    loadFileFromPath(const char* fullpath);
    bool    loadImageEntryFromPath(const char* fullpath, uint16_t index);
    bool    mountDiskFromPath(const char* fullpath);

    // Puts a different disk in the running drive without resetting anything.
    // The C64 keeps going and so does the drive, which matters because a
    // loader that has uploaded its own code into the drive would not survive a
    // reset. The change is announced on the write protect line, the way a real
    // drive announces it. Only useful with 1541 emulation on; with the kernal
    // traps it just swaps the image.
    bool    swapDisk(const char* fullpath);

    // Attaches a .d64 as drive 8 so the C64 can LOAD from it itself, rather
    // than having a program injected into memory. Returns false if the image
    // cannot be read or the Kernal traps could not be installed.
    bool    mountDisk(const char* filename);

    // Switches drive 8 between the Kernal traps and a real emulated 1541.
    // The 1541 needs its DOS ROM on the card; without it this returns false
    // and the traps stay in charge.
    bool    setTrueDriveEmulation(bool enabled);
    bool    trueDriveEmulation() const {
        return trueDrive;
    }
    void    unmountDisk();
    bool    diskMounted() const {
        return mounted;
    }
    const std::string& mountedDiskName() const {
        return mountedName;
    }
    void    reset();
    uint8_t executeExternalCmd(uint8_t* buffer);
};
