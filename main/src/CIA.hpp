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
#ifndef CIA_H
#define CIA_H

#include <atomic>
#include <cstdint>

// register dc0d:
// - Interrupt Control Register when written to
// - is an Interrupt Latch Register when read from
// - read the clear-on-read register $dc0d will ACK all pending CIA 1 interrupts

class CIA {
public:
  uint8_t ciaReg[0x10];

  bool underflowTimerA;
  uint8_t serBitNR;
  uint8_t serBitNRNext;
  uint8_t latchDC04;
  uint8_t latchDC05;
  uint8_t latchDC06;
  uint8_t latchDC07;
  uint8_t latchDC0D; // read latch register
  uint16_t timerA;
  uint16_t timerB;

  std::atomic<bool> isTODRunning;
  bool isTODFreezed;
  std::atomic<bool> isAlarm;
  std::atomic<uint8_t> latchRunDC08; // TOD running
  std::atomic<uint8_t> latchRunDC09;
  std::atomic<uint8_t> latchRunDC0A;
  std::atomic<uint8_t> latchRunDC0B;
  std::atomic<uint8_t> latchAlarmDC08; // set alarm
  std::atomic<uint8_t> latchAlarmDC09;
  std::atomic<uint8_t> latchAlarmDC0A;
  std::atomic<uint8_t> latchAlarmDC0B;

  CIA(bool isCIA1);
  void init(bool isCIA1);
  void checkAlarm();
  void checkTimerA(uint8_t deltaT);
  void checkTimerB(uint8_t deltaT);
  uint8_t getCommonCIAReg(uint8_t ciaidx);
  void setCommonCIAReg(uint8_t ciaidx, uint8_t val);
};
#endif // CIA_H
