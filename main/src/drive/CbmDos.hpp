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
#include <string>
#include <vector>
#include "DiskImage.hpp"
#include "IecDevice.hpp"

// CBM DOS on top of a disk image, standing in for the 1541's own ROM.
//
// Files are read by following their sector chain, the directory is
// synthesised as the BASIC listing a real drive returns, and channel 15
// carries the usual "00, OK,00,00" style status. Writing is not implemented
// yet: anything that would modify the disk answers WRITE PROTECT ON.
class CbmDos : public IecDevice {
   public:
    // Number of channels a 1541 offers. 15 is the command channel.
    static const uint8_t NUM_CHANNELS   = 16;
    static const uint8_t CMD_CHANNEL    = 15;
    static const uint8_t DIR_ENTRY_SIZE = 32;

   private:
    struct Channel {
        bool open = false;

        // A file being written. The sector under construction lives in
        // writeBuf until it fills up or the channel is closed.
        bool         writing = false;
        uint8_t      writeBuf[CBM_SECTOR_SIZE] = {};
        unsigned int writePos  = 2;  // link bytes come first
        uint8_t      curTrack  = 0;
        uint8_t      curSector = 0;
        uint16_t     blocks    = 0;
        uint8_t      dirTrack  = 0;  // where this file's directory slot lives
        uint8_t      dirSector = 0;
        uint8_t      dirSlot   = 0;

        // A file being streamed off the disk by following its sector chain.
        bool         streaming = false;
        unsigned int track     = 0;
        unsigned int sector    = 0;
        uint8_t      buf[CBM_SECTOR_SIZE] = {};
        unsigned int pos     = 0;  // next byte to hand out, within buf
        unsigned int used    = 0;  // one past the last valid byte in buf
        unsigned int visited = 0;  // sectors read, to bound a circular chain

        // A response held in memory: the directory, or a status message.
        std::vector<uint8_t> data;
        size_t               dataPos = 0;

        bool eof = false;
    };

    DiskImage*  disk     = nullptr;
    uint8_t     deviceNo = 8;
    std::string diskName;

    Channel channels[NUM_CHANNELS];

    // Bus state
    bool        listening     = false;
    bool        talking       = false;
    uint8_t     activeChannel = 0;
    uint8_t     pendingCmd    = 0;  // the secondary's command bits while listening
    std::string nameBuf;            // filename or command being received

    // Status shown on channel 15
    std::string status;

    // The BAM sector, kept in memory while a disk is attached so allocation
    // does not re-read it for every block.
    uint8_t bam[CBM_SECTOR_SIZE];
    bool    bamLoaded = false;
    bool    bamDirty  = false;

    bool         bamLoad();
    bool         bamFlush();
    bool         bamIsFree(unsigned int track, unsigned int sector) const;
    void         bamSet(unsigned int track, unsigned int sector, bool free);
    // Finds a free block, preferring one near `nearTrack` the way a real drive
    // does so a file's blocks stay together.
    bool         bamAllocate(unsigned int nearTrack, uint8_t* track, uint8_t* sector);
    void         bamFreeChain(unsigned int track, unsigned int sector);
    unsigned int bamFreeBlocks() const;

    // Directory slot helpers. `slot` is the 32 byte entry inside `buf`.
    bool findSlot(const std::string& pattern, uint8_t typeWanted, uint8_t* dirTrack, uint8_t* dirSector,
                  uint8_t* dirSlot, uint8_t* buf);
    bool findFreeSlot(uint8_t* dirTrack, uint8_t* dirSector, uint8_t* dirSlot, uint8_t* buf);
    bool updateSlot(uint8_t dirTrack, uint8_t dirSector, uint8_t dirSlot, const uint8_t* entry);

    void openWrite(uint8_t channel, const std::string& name, uint8_t type, bool replace);
    void closeChannel(uint8_t channel);
    void scratch(const std::string& pattern);
    void renameFile(const std::string& args);

    void setStatus(uint8_t code, uint8_t track = 0, uint8_t sector = 0);
    void openFile(uint8_t channel, const std::string& request);
    void openDirectory(uint8_t channel, const std::string& pattern);
    void executeCommand(const std::string& command);
    bool advanceSector(Channel& ch);

    // Finds a directory entry whose name matches `pattern`, which may contain
    // the usual * and ? wildcards. Returns false when nothing matches.
    bool findEntry(const std::string& pattern, uint8_t typeWanted, uint8_t* track, uint8_t* sector);

   public:
    CbmDos()
    {
    }
    ~CbmDos() override
    {
    }

    // Attaches an image. Passing nullptr detaches, after which the device
    // reports itself absent and the Kernal says DEVICE NOT PRESENT.
    void setDisk(DiskImage* image, const std::string& name = "");
    void setDeviceNumber(uint8_t number)
    {
        deviceNo = number;
    }

    void reset();

    uint8_t deviceNumber() const override
    {
        return deviceNo;
    }
    bool present() const override
    {
        return disk != nullptr;
    }

    void listen(uint8_t secondary) override;
    void talk(uint8_t secondary) override;
    void unlisten() override;
    void untalk() override;
    bool write(uint8_t value) override;
    bool read(uint8_t* value, bool* eoi) override;

    // Exposed for tests.
    const std::string& currentStatus() const
    {
        return status;
    }
    static bool nameMatches(const uint8_t* entryName, const std::string& pattern);
};
