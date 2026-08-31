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
#include "DiskImage.hpp"
#include "Gcr.hpp"

// What the read head sees, modelled at the byte level.
//
// Following Frodo's Job1541: the head advances one byte each time the drive
// reads port A of VIA 2, rather than on a bit clock. That is enough for the
// drive's own read routines and for loaders that drive them, and it costs a
// fraction of a rotation model.
//
// Frodo (C) 1994-1997, 2002 Christian Bauer, GPL version 2 or later.
class DiskController {
   private:
    DiskImage*   disk        = nullptr;
    unsigned int halfTrack   = 2 * 18;  // the head starts on the directory track
    unsigned int headPos     = 0;
    // How much of gcrTrack is actually on the surface of this track. Outer
    // tracks hold more bytes per revolution than inner ones, so this is the
    // point the head wraps at, not the size of the buffer.
    unsigned int trackLen    = GCR_TRACK_SIZE;
    bool         trackLoaded = false;
    uint8_t      id1         = 0;
    uint8_t      id2         = 0;

    // A disk being swapped is announced on the write protect line, which is
    // the only way the drive can tell. Taking one out uncovers the photocell,
    // putting one in covers it again, so the line moves whatever the two disks
    // are. The DOS and every loader that cares watch for exactly that.
    unsigned int changeCycles = 0;

    uint8_t gcrTrack[GCR_TRACK_SIZE];
    bool    trackDirty = false;

    void loadTrack();
    // Writes any changed sectors of the current track back to the image.
    void flushTrack();

   public:
    DiskController();

    void setDisk(DiskImage* image);
    bool hasDisk() const
    {
        return disk != nullptr;
    }

    // The head sits on a half track, since the stepper moves in half steps.
    unsigned int currentTrack() const
    {
        return halfTrack / 2;
    }
    unsigned int currentHalfTrack() const
    {
        return halfTrack;
    }

    void moveHeadOut();  // towards track 1
    void moveHeadIn();   // towards track 35

    // Reads the byte under the head and moves on. Returns gap bytes when
    // there is no disk, which is what an empty drive sounds like.
    uint8_t readGcrByte();

    // Writes the byte under the head and moves on, as the head does when the
    // drive has port A driving. The track is written back to the image when
    // the head leaves it.
    void writeGcrByte(uint8_t value);

    // Pushes any pending changes out. Called when the head steps away, when
    // the disk is taken out, and on reset.
    void flush();

    // Where the head is sitting in the encoded track, for tests and probes.
    unsigned int headPosition() const
    {
        return headPos;
    }

    // How many bytes go past the head in one revolution of this track.
    unsigned int trackLength() const
    {
        return trackLen;
    }

    // Moves the head on by one byte without reading it. The disk keeps turning
    // whether or not the drive is taking bytes off it, and the DOS leans on
    // that: it hunts for a sync mark by watching the sync line alone, without
    // touching port A, so a head that only moves when it is read never
    // arrives at one.
    void rotate();

    // True when the head is sitting on a sync mark.
    bool syncFound() const;

    // Port B bit 4 of VIA 2: low means write protected. A read only image, or
    // no image at all, reports protected. While a disk is being swapped this
    // reports the sequence a real drive produces instead.
    uint8_t writeProtectBit() const;

    // Puts a different disk in without disturbing the drive. The head stays
    // where it is and the drive CPU keeps running, so a loader that has
    // uploaded its own code survives the swap, which is the whole point of
    // being able to turn a disk over in the middle of a demo.
    void swapDisk(DiskImage* image);

    // Runs the swap sequence. Fed from the same byte clock that turns the disk.
    void countChange(unsigned int cycles);

    bool diskChanging() const
    {
        return changeCycles != 0;
    }

    // Speed zone for the current track, as the drive would select it.
    uint8_t speedZone() const;

    void reset();
};
