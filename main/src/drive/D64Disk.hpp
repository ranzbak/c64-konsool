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

// A .d64 file on the card, presented as sectors.
//
// Sizes from 35 to 42 tracks are recognised, with or without the trailing
// error info byte per block, by matching the file size against each candidate
// geometry the way VICE probes them.
class D64Disk : public DiskImage {
   private:
    int          fd         = -1;
    unsigned int trackCount = 35;
    bool         readOnly   = true;

   public:
    D64Disk()
    {
    }
    ~D64Disk() override;

    // Opens for reading and writing when the card allows it, falling back to
    // read only. writable() says which happened.
    bool open(const char* path);
    void close();
    bool isOpen() const
    {
        return fd >= 0;
    }

    bool readSector(unsigned int track, unsigned int sector, uint8_t* buf) override;
    bool writeSector(unsigned int track, unsigned int sector, const uint8_t* buf) override;

    bool writable() const override
    {
        return false;  // writes land in a later change
    }
    unsigned int tracks() const override
    {
        return trackCount;
    }
    unsigned int sectorsPerTrack(unsigned int track) const override;

    // Sectors on `track` of a 1541 disk, 0 if out of range. Static so callers
    // that have no image open can still work out a layout.
    static unsigned int sectorsOnTrack(unsigned int track);
};
