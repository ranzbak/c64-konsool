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

#include <cstddef>
#include <cstdint>

static const unsigned int CBM_SECTOR_SIZE = 256;

// Block level access to a disk image.
//
// This is the layer everything above it is built on: the DOS emulation reads
// its directory and files through here, and a future 1541 with its own CPU
// would read the same sectors through the same interface. Nothing above this
// line should know how a .d64 is laid out on the card.
class DiskImage {
   public:
    virtual ~DiskImage()
    {
    }

    // Reads one 256 byte sector. `buf` must have room for CBM_SECTOR_SIZE
    // bytes. Returns false for a track or sector outside the disk.
    virtual bool readSector(unsigned int track, unsigned int sector, uint8_t* buf) = 0;

    // Writes one sector back to the image. The default refuses, so a read only
    // image does not have to implement it.
    virtual bool writeSector(unsigned int track, unsigned int sector, const uint8_t* buf)
    {
        (void)track;
        (void)sector;
        (void)buf;
        return false;
    }

    virtual bool         writable() const = 0;
    virtual unsigned int tracks() const   = 0;

    // Sectors on `track`, or 0 when the track is out of range.
    virtual unsigned int sectorsPerTrack(unsigned int track) const = 0;

    // Where the directory starts. A 1541 always answers 18/1.
    virtual unsigned int dirTrack() const
    {
        return 18;
    }
    virtual unsigned int dirSector() const
    {
        return 1;
    }
};
