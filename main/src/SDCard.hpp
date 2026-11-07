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

#include <sys/stat.h>
#include <cstdint>
#include <string>
#include <vector>
#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#include "images/CbmImage.hpp"

class SDCard {
   private:
    bool initialized;

   public:
    SDCard();
    ~SDCard();

    bool                     init();
    //   uint16_t load(const char *path, uint8_t *ram, size_t len);
    uint16_t                 load(const char* path, uint8_t* ram, size_t len = 0);
    uint16_t                 load_auto(const char* path, uint8_t* ram, size_t len = 0);
    bool                     save(const char* path, const uint8_t* ram, size_t len = 0);
    // Lists the loadable files in `path` one page at a time. Names keep their
    // extension so the caller can tell a program from a container.
    // Lists the loadable files in `path`, sorted, with their extensions kept
    // so the caller can tell a program from a container. Static so it serves
    // a USB disk as well as the card. A directory holding more than
    // MAX_LISTED_FILES of them is truncated, with a warning.
    static const size_t             MAX_LISTED_FILES = 512;
    static std::vector<std::string> listLoadableFiles(const char* path);
    bool                     listNextEntry(uint8_t* nextEntry, size_t entrySize, bool start);
    // Builds the full path of a file in the program directory.
    static std::string       fullPath(const char* filename);
    // Reads a .prg at `full_path` into `ram`, wherever that path happens to
    // live. Returns the address one past the last byte written, or 0.
    static uint16_t          readPrg(const char* full_path, uint8_t* ram);
};
