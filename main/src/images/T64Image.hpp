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

// Reader for T64 tape containers.
//
// A T64 is a 64 byte header followed by a table of 32 byte records, each
// pointing at a blob of file data elsewhere in the container. Unlike a .prg
// the data blob does NOT start with a load address; the load address lives in
// the record.
//
// The record's end address is famously unreliable (the CONV64 tool wrote
// $C3C6 for every file), so the real length of each file is derived from the
// distance to the next data blob instead, exactly as t64fix does.
class T64Image : public CbmImage {
   private:
    struct Record {
        uint32_t offset;     // where the file data starts in the container
        uint16_t startAddr;  // load address
        uint16_t length;     // corrected length in bytes
    };

    int                     fd = -1;
    std::vector<Record>     records;
    std::vector<ImageEntry> imageEntries;

   public:
    T64Image()
    {
    }
    ~T64Image() override;

    bool open(const char* path) override;
    void close() override;

    const std::vector<ImageEntry>& entries() const override
    {
        return imageEntries;
    }

    bool extract(uint16_t index, uint8_t* ram, uint16_t* endAddr) override;
};
