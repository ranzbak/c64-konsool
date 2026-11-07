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
#ifndef CPUC64_H
#define CPUC64_H

#include <stdint.h>
#include "CIA.hpp"
#include "CPU6502.hpp"
#include "drive/Drive1541.hpp"
#include "drive/IecBus.hpp"
#include "drive/IecLines.hpp"
#include "Joystick.hpp"
#include "VIC.hpp"
#include <cstdint>
#include <mutex>
#include "freertos/idf_additions.h"
#include "menuoverlay/MenuDataStore.hpp"

class C64Emu;

class CPUC64 : public CPU6502 {
private:
  uint16_t iecTrapAddr[8] = {};
  uint8_t iecTrapSavedByte[8] = {};
  bool iecTrapsActive = false;
  int  driveCycleDebt = 0;
  bool handleIecTrap(uint16_t addr);
  void onAtnChanged();
  C64Emu *c64emu;
  uint8_t *ram;
  uint8_t *basicrom;
  uint8_t *kernalrom;
  uint8_t *charrom;
  Joystick joystick;
  MenuDataStore* menuDataStore = MenuDataStore::getInstance();

  uint8_t sidreg[0x100];

  // Limit frame rate to ~50Hz
  SemaphoreHandle_t frameRateMutex;

  bool bankARAM;
  bool bankDRAM;
  bool bankERAM;
  bool bankDIO;
  uint8_t register1;

  std::mutex pcMutex;

  bool nmiAck;

  inline void adaptVICBaseAddrs(bool fromcia) __attribute__((always_inline));
  void applyCia2PortA();
  inline void decodeRegister1(uint8_t val) __attribute__((always_inline));
  inline void checkciatimers(uint8_t cycles) __attribute__((always_inline));
  inline void logDebugInfo() __attribute__((always_inline));
  inline uint8_t getVirtJoyValue(bool port2) __attribute__((always_inline));

public:
  VIC *vic;
  CIA cia1;
  CIA cia2;

  CPUC64() : cia1(true), cia2(false) {}

  // public only for logging / debugging
  uint8_t getA();
  uint8_t getX();
  uint8_t getY();
  uint8_t getSP();
  uint8_t getSR();
  uint16_t getPC();

  uint32_t numofcyclespersecond;
  std::atomic<uint16_t> adjustcycles;
  std::atomic<uint16_t> measuredcycles;

  // set by class ExternalCmds
  uint8_t joystickmode;
  // C64 port (1 or 2, 0 for none) the emulated keyboard joystick is on, and the
  // port a USB gamepad is on. The two are equal unless two player mode is on.
  // Both are refreshed once per frame by updateJoystickPorts().
  uint8_t kbjoystickmode;
  uint8_t padjoystickmode;
  bool deactivateCIA2;
  bool debug;
  uint16_t debugstartaddr;
  bool debuggingstarted;
  uint64_t presleeptime;

  bool restorenmi;

  uint8_t getMem(uint16_t addr) override;
  void setMem(uint16_t addr, uint8_t val) override;
  SemaphoreHandle_t getFrameRateMutex() { return frameRateMutex; }

  uint8_t *getSidRegs();

  void cmd6502halt() override;
  void run() override;

  // Serial bus, and the Kernal routines we stand in for.
  //
  // There is no IEC hardware in this emulator, so instead the Kernal's own
  // serial routines are replaced: each one gets a JAM opcode patched over its
  // first byte, and cmd6502halt() picks the call up from there. The addresses
  // come from the Kernal jump table rather than being hardcoded.
  IecBus iecbus;

  // The real drive, and the bus lines it shares with CIA 2. When the drive is
  // not running these lines simply stay released, which is what an empty bus
  // looks like.
  IecLines  iecLines;
  Drive1541 drive;
  bool      trueDriveEnabled = false;

  // Steps the drive alongside the C64 for one rasterline's worth of cycles.
  void emulateDriveCycles(unsigned int c64Cycles);
  void executeUntilCycle(uint8_t untilCycles);
  enum IecTrap {
    IEC_TRAP_LISTEN = 0,
    IEC_TRAP_TALK,
    IEC_TRAP_SECOND,
    IEC_TRAP_TKSA,
    IEC_TRAP_CIOUT,
    IEC_TRAP_ACPTR,
    IEC_TRAP_UNTLK,
    IEC_TRAP_UNLSN,
    IEC_TRAP_COUNT
  };
  // Turns the real drive on. `romImage` must be Drive1541::ROM_SIZE bytes and
  // outlive the CPU. Returns false when there is no usable ROM.
  bool enableTrueDrive(uint8_t* romImage, DiskImage* image);
  void disableTrueDrive();
  bool trueDriveActive() const { return trueDriveEnabled && drive.ready(); }

  bool installIecTraps();
  void removeIecTraps();
  bool iecTrapsInstalled() const { return iecTrapsActive; }

  void initMemAndRegs();
  void init(uint8_t *ram, uint8_t *charrom, VIC *vic, C64Emu *c64emu);
  void setPC(uint16_t pc);
  void exeSubroutine(uint16_t addr, uint8_t rega, uint8_t regx, uint8_t regy);
  void setKeycodes(uint8_t keycode1, uint8_t keycode2);
  void updateJoystickPorts();
};

#endif // CPUC64_H
