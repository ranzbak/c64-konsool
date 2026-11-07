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
#include <vector>
#include "CbmImage.hpp"
#include "drive/D64Disk.hpp"

// Read only reader for 1541 disk images.
//
// This walks the directory on track 18 and follows a file's sector chain to
// copy a program straight into RAM, which is the quick way to start something
// that loads in one go.
//
// For anything that loads more than one part, mount the disk as drive 8
// instead (see ExternalCmds::mountDisk) so the C64 does the loading itself.
class D64Image : public CbmImage {
   private:
    struct Entry {
        uint8_t track;  // first data sector
        uint8_t sector;
    };

    D64Disk                 disk;
    std::vector<Entry>      dirEntries;
    std::vector<ImageEntry> imageEntries;

   public:
    D64Image()
    {
    }
    ~D64Image() override;

    bool open(const char* path) override;
    void close() override;

    const std::vector<ImageEntry>& entries() const override
    {
        return imageEntries;
    }

    bool extract(uint16_t index, uint8_t* ram, uint16_t* endAddr) override;

    // Number of sectors on `track` of a 1541 disk, 0 if out of range.
    static unsigned int sectorsPerTrack(unsigned int track)
    {
        return D64Disk::sectorsOnTrack(track);
    }
};
