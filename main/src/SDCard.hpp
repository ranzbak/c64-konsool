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

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <cstdint>

class SDCard {
  private:
      bool initialized;
      sdmmc_card_t *card;
  
  public:
      SDCard();
      ~SDCard();
  
      bool init();
    //   uint16_t load(const char *path, uint8_t *ram, size_t len);
      uint16_t load(const char *path, uint8_t *ram, size_t len);
      bool save(const char *path, const uint8_t *ram, size_t len);
      bool listNextEntry(char *nextEntry, size_t entrySize, bool start);
  };

