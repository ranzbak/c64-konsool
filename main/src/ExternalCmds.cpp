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
#include "ExternalCmds.hpp"
#include <esp_log.h>
#include <fcntl.h>
#include <cstring>
#include <sys/unistd.h>
#include "C64Emu.hpp"
#include "menuoverlay/MenuDataStore.hpp"
#include "Config.hpp"
#include "listactions.h"
#include "loadactions.h"
#include "saveactions.hpp"
#include "string.h"
#include <string>
#include "images/CbmImage.hpp"
#include "images/D64Image.hpp"
#include "images/T64Image.hpp"
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include "drive/Drive1541.hpp"

static const char* TAG = "ExternalCmds";

enum class ExternalCmds::ExtCmd {
    NOEXTCMD                = 0,
    JOYSTICKMODE1           = 1,
    JOYSTICKMODE2           = 2,
    KBJOYSTICKMODE1         = 3,
    KBJOYSTICKMODE2         = 4,
    JOYSTICKMODEOFF         = 5,
    KBJOYSTICKMODEOFF       = 6,
    LOAD                    = 11,
    RECEIVEDATA             = 12,
    SHOWREG                 = 13,
    SHOWMEM                 = 14,
    RESTORE                 = 15,
    RESET                   = 20,
    GETSTATUS               = 21,
    SWITCHFRAMECOLORREFRESH = 22,
    SENDRAWKEYS             = 24,
    SWITCHDEBUG             = 25,
    SWITCHPERF              = 26,
    SWITCHDETECTRELEASEKEY  = 27,
    GETBATTERYVOLTAGE       = 29,
    POWEROFF                = 30,
    SAVE                    = 31,
    LIST                    = 32
};

void ExternalCmds::init(uint8_t* ram, C64Emu* c64emu) {
    if (initialized == true) {
        return;
    };
    this->ram       = ram;
    this->c64emu    = c64emu;
    sendrawkeycodes = false;
    liststartflag   = true;

    // Setup SDCard
    // TODO: implement detection of insert and remove SD card
    sdcard.init();

    // Switch the real drive on for the whole session when its ROM is on the
    // card. Waiting for a disk to go in was too late to be useful: the menu
    // toggle still read off until something had been mounted, and a program
    // started from a .prg had no drive 8 at all. A card without the ROM is
    // unchanged, and the toggle still switches it off by hand.
    if (loadDriveRom() && setTrueDriveEmulation(true)) {
        MenuDataStore::getInstance()->set("true_drive_ena", true);
        ESP_LOGI(TAG, "1541 emulation on, %s found", DRIVE_ROM_FILENAME);
    }
}

void ExternalCmds::setType1Notification() {
    type1notification.type                   = 1;
    type1notification.joymode                = c64emu->cpu.joystickmode;
    type1notification.deactivateCIA2         = c64emu->cpu.deactivateCIA2;
    type1notification.sendrawkeycodes        = sendrawkeycodes;
    type1notification.switchdebug            = c64emu->cpu.debug;
    type1notification.switchperf             = c64emu->perf;
    type1notification.switchdetectreleasekey = c64emu->konsoolkb.detectreleasekey;
}

void ExternalCmds::setType2Notification() {
    type2notification.type       = 2;
    type2notification.cpuRunning = !c64emu->cpu.cpuhalted;
    type2notification.pc         = c64emu->cpu.getPC();
    type2notification.a          = c64emu->cpu.getA();
    type2notification.x          = c64emu->cpu.getX();
    type2notification.y          = c64emu->cpu.getY();
    type2notification.sr         = c64emu->cpu.getSR();
    type2notification.d011       = c64emu->cpu.getMem(0xd011);
    type2notification.d016       = c64emu->cpu.getMem(0xd016);
    type2notification.d018       = c64emu->cpu.getMem(0xd018);
    type2notification.d019       = c64emu->cpu.getMem(0xd019);
    type2notification.d01a       = c64emu->cpu.getMem(0xd01a);
    type2notification.register1  = c64emu->cpu.getMem(1);
    type2notification.dc0d       = c64emu->cpu.getMem(0xdc0d);
    type2notification.dc0e       = c64emu->cpu.getMem(0xdc0e);
    type2notification.dc0f       = c64emu->cpu.getMem(0xdc0f);
    type2notification.dd0d       = c64emu->cpu.getMem(0xdd0d);
    type2notification.dd0e       = c64emu->cpu.getMem(0xdd0e);
    type2notification.dd0f       = c64emu->cpu.getMem(0xdd0f);
}

void ExternalCmds::setType3Notification(uint16_t addr) {
    type3notification.type = 3;
    for (uint8_t i = 0; i < BLENOTIFICATIONTYPE3NUMOFBYTES; i++) {
        type3notification.mem[i] = c64emu->cpu.getMem(addr + i);
    }
}

void ExternalCmds::setType4Notification() {
    type4notification.type = 4;
}

void ExternalCmds::setType5Notification(uint8_t batteryVolLow, uint8_t batteryVolHi) {
    type5notification.type          = 5;
    type5notification.batteryVolLow = batteryVolLow;
    type5notification.batteryVolHi  = batteryVolHi;
}

void ExternalCmds::setVarTab(uint16_t addr) {
    // set VARTAB
    ram[0x2d] = addr % 256;
    ram[0x2e] = addr / 256;
    // clr
    c64emu->cpu.setPC(0xa52a);
}

void ExternalCmds::finishLoad(bool fileloaded, bool error) {
    uint16_t addr = src_loadactions_prg[0] + (src_loadactions_prg[1] << 8);
    memcpy(ram + addr, src_loadactions_prg + 2, src_loadactions_prg_len - 2);
    if (fileloaded) {
        c64emu->cpu.exeSubroutine(addr, 1, 0, 0);
    } else if (error) {
        c64emu->cpu.exeSubroutine(addr, 0, 1, 0);
    } else {
        c64emu->cpu.exeSubroutine(addr, 0, 0, 0);
    }
    c64emu->cpu.cpuhalted = false;
}

bool ExternalCmds::loadPrg(const char* filename) {
    return loadFile((std::string(filename) + ".prg").c_str());
}

bool ExternalCmds::loadFile(const char* filename) {
    ESP_LOGI(TAG, "load %s from sdcard...", filename);
    if (!sdcard.init()) {
        ESP_LOGE(TAG, "error init sdcard");
        c64emu->cpu.cpuhalted = true;
        finishLoad(false, true);
        return false;
    }
    return loadFileFromPath(SDCard::fullPath(filename).c_str());
}

bool ExternalCmds::loadFileFromPath(const char* fullpath) {
    ImageFormat format = imageFormatFromName(fullpath);
    if (format == ImageFormat::T64 || format == ImageFormat::D64) {
        // No entry was picked, so load the first program in the container.
        return loadImageEntryFromPath(fullpath, 0);
    }
    if (format == ImageFormat::PRG) {
        return loadPrgFromPath(fullpath);
    }

    ESP_LOGE(TAG, "unsupported file type: %s", fullpath);
    c64emu->cpu.cpuhalted = true;
    finishLoad(false, true);
    return false;
}

bool ExternalCmds::loadImageEntry(const char* filename, uint16_t index) {
    if (!sdcard.init()) {
        ESP_LOGE(TAG, "error init sdcard");
        c64emu->cpu.cpuhalted = true;
        finishLoad(false, true);
        return false;
    }
    return loadImageEntryFromPath(SDCard::fullPath(filename).c_str(), index);
}

bool ExternalCmds::loadImageEntryFromPath(const char* fullpath, uint16_t index) {
    ESP_LOGI(TAG, "load entry %u of %s", static_cast<unsigned>(index), fullpath);

    // Taking a program off a disk is not the same as being handed a .prg: the
    // program usually expects that disk to still be in the drive, because the
    // next part comes off it. Leave it mounted, which also brings the 1541 on
    // when its ROM is on the card. Do this before the CPU is halted below,
    // since mounting halts and releases it in its own right.
    if (imageFormatFromName(fullpath) == ImageFormat::D64) {
        mountDiskFromPath(fullpath);
    }

    c64emu->cpu.cpuhalted = true;

    T64Image  t64;
    D64Image  d64;
    CbmImage* image = nullptr;
    switch (imageFormatFromName(fullpath)) {
        case ImageFormat::T64:
            image = &t64;
            break;
        case ImageFormat::D64:
            image = &d64;
            break;
        default:
            ESP_LOGE(TAG, "not a container: %s", fullpath);
            finishLoad(false, true);
            return false;
    }

    if (!image->open(fullpath)) {
        ESP_LOGE(TAG, "cannot read image %s", fullpath);
        finishLoad(false, true);
        return false;
    }

    uint16_t endAddr    = 0;
    bool     fileloaded = image->extract(index, ram, &endAddr);
    image->close();

    if (fileloaded) {
        setVarTab(endAddr);
    } else {
        ESP_LOGI(TAG, "cannot extract entry %u of %s", static_cast<unsigned>(index), fullpath);
    }

    finishLoad(fileloaded, false);
    return fileloaded;
}

// Reads the 1541 DOS ROM off the card. It is a copyrighted 16K binary so it
// is not shipped with the firmware; drop it next to the disk images as
// "1541.rom" to use the real drive.
bool ExternalCmds::loadDriveRom() {
    if (driveRom != nullptr) return true;
    if (!sdcard.init()) return false;

    std::string path = SDCard::fullPath(DRIVE_ROM_FILENAME);
    int         fd   = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        ESP_LOGW(TAG, "no %s on the card, true drive emulation unavailable", DRIVE_ROM_FILENAME);
        return false;
    }

    uint8_t* buffer    = new uint8_t[Drive1541::ROM_SIZE];
    size_t   remaining = Drive1541::ROM_SIZE;
    size_t   total     = 0;
    while (remaining > 0) {
        ssize_t got = read(fd, buffer + total, remaining);
        if (got <= 0) break;
        total     += static_cast<size_t>(got);
        remaining -= static_cast<size_t>(got);
    }
    close(fd);

    if (total != Drive1541::ROM_SIZE) {
        ESP_LOGE(TAG, "%s is %u bytes, expected %u", DRIVE_ROM_FILENAME, static_cast<unsigned>(total),
                 static_cast<unsigned>(Drive1541::ROM_SIZE));
        delete[] buffer;
        return false;
    }

    driveRom = buffer;
    ESP_LOGI(TAG, "loaded %s", DRIVE_ROM_FILENAME);
    return true;
}

bool ExternalCmds::setTrueDriveEmulation(bool enabled) {
    if (!enabled) {
        c64emu->cpu.disableTrueDrive();
        trueDrive = false;
        // Hand device 8 back to the traps if a disk is still mounted.
        if (mounted) {
            c64emu->cpu.cpuhalted = true;
            c64emu->cpu.installIecTraps();
            c64emu->cpu.cpuhalted = false;
        }
        return true;
    }

    if (!loadDriveRom()) {
        trueDrive = false;
        return false;
    }

    c64emu->cpu.cpuhalted = true;
    bool ok               = c64emu->cpu.enableTrueDrive(driveRom, mounted ? &disk : nullptr);
    c64emu->cpu.cpuhalted = false;

    trueDrive = ok;
    return ok;
}

bool ExternalCmds::mountDisk(const char* filename) {
    if (!sdcard.init()) {
        ESP_LOGE(TAG, "error init sdcard");
        return false;
    }
    return mountDiskFromPath(SDCard::fullPath(filename).c_str());
}

bool ExternalCmds::mountDiskFromPath(const char* fullpath) {
    unmountDisk();

    // Putting a disk in is the moment to switch the real drive on. Anything
    // that loads more than one part needs it, and having to remember a menu
    // toggle every time is a good way to conclude the disk is broken. This
    // only happens when the ROM is actually on the card, so a card without one
    // behaves exactly as before, and the toggle still switches it back off.
    if (!trueDrive && loadDriveRom()) {
        if (setTrueDriveEmulation(true)) {
            MenuDataStore::getInstance()->set("true_drive_ena", true);
            ESP_LOGI(TAG, "1541 emulation switched on for this disk");
        }
    }

    if (imageFormatFromName(fullpath) != ImageFormat::D64) {
        ESP_LOGE(TAG, "%s is not a disk image", fullpath);
        return false;
    }

    if (!disk.open(fullpath)) {
        ESP_LOGE(TAG, "cannot read disk image %s", fullpath);
        return false;
    }

    // Menus and logs want the file name, not the whole path it came from.
    const char* name = strrchr(fullpath, '/');
    name             = (name != nullptr) ? name + 1 : fullpath;

    // Set the DOS emulation up whichever drive is in charge. It only answers
    // while the traps are installed, and having it ready means switching the
    // 1541 off later leaves a working drive 8 rather than an empty bus.
    dos.setDeviceNumber(8);
    dos.setDisk(&disk);
    c64emu->cpu.iecbus.attach(&dos);

    if (trueDrive) {
        // The real drive reads the image itself, so the traps stay out of the
        // way entirely.
        c64emu->cpu.cpuhalted = true;
        bool ok               = c64emu->cpu.enableTrueDrive(driveRom, &disk);
        c64emu->cpu.cpuhalted = false;
        if (!ok) {
            dos.setDisk(nullptr);
            c64emu->cpu.iecbus.detach(8);
            disk.close();
            return false;
        }
        mounted     = true;
        mountedName = name;
        ESP_LOGI(TAG, "mounted %s in the emulated 1541", name);
        return true;
    }

    // Installing the traps rewrites bytes in the kernal image the running CPU
    // is fetching from, so stop it for the moment it takes.
    c64emu->cpu.cpuhalted = true;
    bool installed        = c64emu->cpu.installIecTraps();
    c64emu->cpu.cpuhalted = false;

    if (!installed) {
        ESP_LOGE(TAG, "could not install the kernal serial traps");
        c64emu->cpu.iecbus.detach(8);
        dos.setDisk(nullptr);
        disk.close();
        return false;
    }

    mounted     = true;
    mountedName = name;
    ESP_LOGI(TAG, "mounted %s as drive 8", name);
    return true;
}

bool ExternalCmds::swapDisk(const char* fullpath) {
    ESP_LOGI(TAG, "swapping in %s, true drive %s", fullpath, trueDrive ? "on" : "off");

    if (imageFormatFromName(fullpath) != ImageFormat::D64) {
        ESP_LOGE(TAG, "%s is not a disk image", fullpath);
        return false;
    }
    if (!mounted) {
        // Nothing to swap for; this is an ordinary mount.
        return mountDiskFromPath(fullpath);
    }

    // Read the new image before letting go of the old one, so a bad path
    // leaves the drive with the disk it already had.
    D64Disk next;
    if (!next.open(fullpath)) {
        ESP_LOGE(TAG, "cannot read disk image %s", fullpath);
        return false;
    }
    next.close();

    disk.close();
    if (!disk.open(fullpath)) {
        ESP_LOGE(TAG, "cannot reopen %s", fullpath);
        mounted = false;
        return false;
    }

    // The kernal traps read through the same image, so they need nothing else.
    dos.setDisk(&disk);

    if (trueDrive) {
        // Deliberately not enableTrueDrive(): that resets the drive CPU, which
        // would throw away a loader's uploaded code. Just change the disk under
        // the head and let the drive notice.
        c64emu->cpu.drive.swapDisk(&disk);
    }

    const char* name = strrchr(fullpath, '/');
    name             = (name != nullptr) ? name + 1 : fullpath;
    mountedName      = name;
    ESP_LOGI(TAG, "swapped in %s", name);
    return true;
}

void ExternalCmds::unmountDisk() {
    if (!mounted) return;

    if (trueDrive) {
        // Taking the disk out does not switch the drive off, on a real 1541 or
        // here. Switching it off left this object still believing it was on,
        // and the menu toggle still reading "On" over a drive that was not
        // running, so the setting lied about the machine. Re-enable it with no
        // disk instead, which is what an empty drive is.
        c64emu->cpu.cpuhalted = true;
        c64emu->cpu.enableTrueDrive(driveRom, nullptr);
        c64emu->cpu.cpuhalted = false;
        dos.setDisk(nullptr);
        c64emu->cpu.iecbus.detach(8);
        disk.close();
        mounted = false;
        mountedName.clear();
        ESP_LOGI(TAG, "took the disk out of the emulated 1541");
        return;
    }

    // Take the traps back out so the Kernal behaves exactly as it did before
    // anything was mounted.
    c64emu->cpu.cpuhalted = true;
    c64emu->cpu.removeIecTraps();
    c64emu->cpu.cpuhalted = false;
    c64emu->cpu.iecbus.detach(8);
    dos.setDisk(nullptr);
    disk.close();

    mounted = false;
    mountedName.clear();
    ESP_LOGI(TAG, "unmounted drive 8");
}

bool ExternalCmds::loadPrgFromPath(const char* fullpath) {
    ESP_LOGI(TAG, "load from %s", fullpath);
    c64emu->cpu.cpuhalted = true;
    bool fileloaded = false;

    uint16_t addr = SDCard::readPrg(fullpath, ram);
    if (addr != 0) {
        setVarTab(addr);
        fileloaded = true;
    } else {
        ESP_LOGE(TAG, "failed to load %s", fullpath);
    }

    uint16_t action_addr = src_loadactions_prg[0] + (src_loadactions_prg[1] << 8);
    memcpy(ram + action_addr, src_loadactions_prg + 2, src_loadactions_prg_len - 2);
    c64emu->cpu.exeSubroutine(action_addr, fileloaded ? 1 : 0, 0, 0);
    c64emu->cpu.cpuhalted = false;
    return fileloaded;
}

void ExternalCmds::reset() {
    if (c64emu != nullptr) {
            c64emu->cpu.cpuhalted = true;
            c64emu->cpu.initMemAndRegs();
            c64emu->cpu.vic->initVarsAndRegs();
            c64emu->cpu.cia1.init(true);
            c64emu->cpu.cia2.init(false);
            c64emu->cpu.cpuhalted = false;
    }
}

uint8_t ExternalCmds::executeExternalCmd(uint8_t* buffer) {
    ExtCmd cmd = static_cast<ExtCmd>(buffer[0]);
    switch (cmd) {
        case ExtCmd::NOEXTCMD:
            return 0;
        case ExtCmd::LOAD: {
            ESP_LOGI(TAG, "load from sdcard...");
            c64emu->cpu.cpuhalted = true;
            bool     fileloaded   = false;
            bool     error        = false;
            uint16_t addr;
            if (sdcard.init()) {
                addr = sdcard.load_auto(SD_CARD_PRG_PATH, ram);
                if (addr == 0) {
                    ESP_LOGI(TAG, "file not found");
                } else {
                    setVarTab(addr);
                    fileloaded = true;
                }
            } else {
                error = true;
                ESP_LOGI(TAG, "error init sdcard");
            }
            addr = src_loadactions_prg[0] + (src_loadactions_prg[1] << 8);
            memcpy(ram + addr, src_loadactions_prg + 2, src_loadactions_prg_len - 2);
            if (fileloaded) {
                c64emu->cpu.exeSubroutine(addr, 1, 0, 0);
            } else if (error) {
                c64emu->cpu.exeSubroutine(addr, 0, 1, 0);
            } else {
                c64emu->cpu.exeSubroutine(addr, 0, 0, 0);
            }
            c64emu->cpu.cpuhalted = false;
            return 0;
        }
        case ExtCmd::SAVE: {
            ESP_LOGI(TAG, "save to sdcard...");
            c64emu->cpu.cpuhalted = true;
            bool filesaved        = false;
            if (sdcard.init()) {
                filesaved = sdcard.save(SD_CARD_PRG_PATH, const_cast<uint8_t*>(ram));
                if (!filesaved) {
                    ESP_LOGI(TAG, "error saving file");
                }
            } else {
                ESP_LOGI(TAG, "error init sdcard");
            }
            uint16_t addr = src_saveactions_prg[0] + (src_saveactions_prg[1] << 8);
            memcpy(ram + addr, src_saveactions_prg + 2, src_saveactions_prg_len - 2);
            if (filesaved) {
                c64emu->cpu.exeSubroutine(addr, 1, 0, 0);
            } else {
                c64emu->cpu.exeSubroutine(addr, 0, 0, 0);
            }
            c64emu->cpu.cpuhalted = false;
            return 0;
        }
        case ExtCmd::LIST: {
            ESP_LOGI(TAG, "list sdcard...");
            c64emu->cpu.cpuhalted = true;
            if (sdcard.init()) {
                uint16_t addr = src_listactions_prg[0] + (src_listactions_prg[1] << 8);
                memcpy(ram + addr, src_listactions_prg + 2, src_listactions_prg_len - 2);
                if (liststartflag) {
                    c64emu->cpu.exeSubroutine(addr, 0, 1, 0);
                } else {
                    c64emu->cpu.exeSubroutine(addr, 0, 2, 0);
                }
                uint8_t filename[17];
                int     cnt = 0;
                while (cnt < 23) {
                    bool success  = sdcard.listNextEntry(filename, sizeof(filename), liststartflag);
                    liststartflag = false;
                    if (success && (filename[0] != '\0')) {
                        // copy filename to c64 ram (0x0342)
                        for (uint8_t i = 0; i < 17; i++) {
                            ram[0x342 + i] = filename[i];
                        }
                        // print it
                        c64emu->cpu.exeSubroutine(addr, 0, 0, 0);
                    } else {
                        if (!success) {
                            ESP_LOGI(TAG, "error reading entry");
                        }
                        liststartflag = true;
                        break;
                    }
                    cnt++;
                }
            } else {
                ESP_LOGI(TAG, "error init sdcard");
            }
            c64emu->cpu.cpuhalted = false;
            return 0;
        }
        case ExtCmd::RECEIVEDATA: {
            ESP_LOGI(TAG, "enter receivedata");
            c64emu->cpu.cpuhalted = true;
            // simple "protocol":
            // - byte 0: cmd (as usual)
            // - byte 1: cmd detail: first block (1), next block (0), last block (2)
            // - byte 2: cmd flag (as usual)
            // - first block: byte 3 - 4: start address, 5 - 252: data
            // - next block: byte 3 - 252: data
            // - last block: byte 3: length of last block, byte 4 - (length+4-1): data
            uint8_t cmddetail     = buffer[1];
            if (cmddetail == 0) {
                // next block
                ESP_LOGI(TAG, "next block: %x", actaddrreceivecmd);
                for (uint8_t i = 3; i < 253; i++) {
                    ram[actaddrreceivecmd + i - 3] = buffer[i];
                }
                actaddrreceivecmd += 250;
            } else if (cmddetail == 1) {
                // first block
                uint16_t addr     = buffer[3] + (buffer[4] << 8);
                actaddrreceivecmd = addr;
                ESP_LOGI(TAG, "first block: %x", actaddrreceivecmd);
                for (uint8_t i = 5; i < 253; i++) {
                    ram[actaddrreceivecmd + i - 5] = buffer[i];
                }
                actaddrreceivecmd += 253 - 5;
            } else if (cmddetail == 2) {
                // last block
                uint8_t len = buffer[3];
                ESP_LOGI(TAG, "last block: %x", actaddrreceivecmd);
                for (uint8_t i = 4; i < (len + 4); i++) {
                    ram[actaddrreceivecmd + i - 4] = buffer[i];
                }
                actaddrreceivecmd += len;
                setVarTab(actaddrreceivecmd);
            }
            c64emu->cpu.cpuhalted = false;
            ESP_LOGI(TAG, "leave receivedata");
            setType4Notification();
            return 4;
        }
        case ExtCmd::RESTORE:
            if (buffer[1] == 1) {
                // restore + run/stop
                c64emu->cpu.setMem(0xdc00, 0);
                c64emu->cpu.setKeycodes(0x7f, 0);
            }
            c64emu->cpu.restorenmi = true;
            return 0;
        case ExtCmd::SHOWREG:
            setType2Notification();
            ESP_LOGI(TAG, "cpuRunning %s", type2notification.cpuRunning ? "true" : "false");
            ESP_LOGI(TAG, "pc = %x, a = %x, x = %x, y = %x, sr = %x", type2notification.pc, type2notification.a,
                     type2notification.x, type2notification.y, type2notification.sr);
            ESP_LOGI(TAG, "d011 = %x, d016 = %x, d018 = %x", type2notification.d011, type2notification.d016,
                     type2notification.d018);
            ESP_LOGI(TAG, "d019 = %x, d01a = %x, register1 = %x", type2notification.d019, type2notification.d01a,
                     type2notification.register1);
            ESP_LOGI(TAG, "dc0d = %x, dc0e = %x, dc0f = %x", type2notification.dc0d, type2notification.dc0e,
                     type2notification.dc0f);
            ESP_LOGI(TAG, "dd0d = %x, dd0e = %x, dd0f = %x", type2notification.dd0d, type2notification.dd0e,
                     type2notification.dd0f);
            return 2;
        case ExtCmd::SHOWMEM: {
            uint16_t addr              = buffer[3] + (buffer[4] << 8);
            // use addr also as debugging start address
            c64emu->cpu.debugstartaddr = addr;
            ESP_LOGI(TAG, "addr: %x", addr);
            setType3Notification(addr);
            for (uint8_t i = 0; i < BLENOTIFICATIONTYPE3NUMOFBYTES / 8; i++) {
                uint8_t j = i * 8;
                ESP_LOGI(TAG, "mem[%d]: %d %d %d %d %d %d %d %d", j, type3notification.mem[j],
                         type3notification.mem[j + 1], type3notification.mem[j + 2], type3notification.mem[j + 3],
                         type3notification.mem[j + 4], type3notification.mem[j + 5], type3notification.mem[j + 6],
                         type3notification.mem[j + 7]);
            }
            return 3;
        }
        case ExtCmd::RESET:
            c64emu->cpu.cpuhalted = true;
            c64emu->cpu.initMemAndRegs();
            c64emu->cpu.vic->initVarsAndRegs();
            c64emu->cpu.cia1.init(true);
            c64emu->cpu.cia2.init(false);
            c64emu->cpu.cpuhalted = false;
            return 0;
        case ExtCmd::JOYSTICKMODE1:
            c64emu->cpu.joystickmode   = 1;
            c64emu->cpu.kbjoystickmode = 0;
            ESP_LOGI(TAG, "joystickmode = %x", c64emu->cpu.joystickmode);
            setType1Notification();
            return 1;
        case ExtCmd::JOYSTICKMODE2:
            c64emu->cpu.joystickmode   = 2;
            c64emu->cpu.kbjoystickmode = 0;
            ESP_LOGI(TAG, "joystickmode = %x", c64emu->cpu.joystickmode);
            setType1Notification();
            return 1;
        case ExtCmd::JOYSTICKMODEOFF:
            c64emu->cpu.joystickmode = 0;
            ESP_LOGI(TAG, "joystickmode = %x", c64emu->cpu.joystickmode);
            setType1Notification();
            return 1;
        case ExtCmd::KBJOYSTICKMODE1:
            c64emu->cpu.kbjoystickmode = 1;
            c64emu->cpu.joystickmode   = 0;
            ESP_LOGI(TAG, "kbjoystickmode = %x", c64emu->cpu.kbjoystickmode);
            return 0;
        case ExtCmd::KBJOYSTICKMODE2:
            c64emu->cpu.kbjoystickmode = 2;
            c64emu->cpu.joystickmode   = 0;
            ESP_LOGI(TAG, "kbjoystickmode = %x", c64emu->cpu.kbjoystickmode);
            return 0;
        case ExtCmd::KBJOYSTICKMODEOFF:
            c64emu->cpu.kbjoystickmode = 0;
            ESP_LOGI(TAG, "kbjoystickmode = %x", c64emu->cpu.kbjoystickmode);
            return 0;
        case ExtCmd::GETSTATUS:
            // just send type 1 notification
            ESP_LOGI(TAG, "send status to BLE client");
            setType1Notification();
            return 1;
        case ExtCmd::SWITCHFRAMECOLORREFRESH:
            c64emu->cpu.deactivateCIA2 = !c64emu->cpu.deactivateCIA2;
            ESP_LOGI(TAG, "deactivateCIA2 = %x", c64emu->cpu.deactivateCIA2);
            setType1Notification();
            return 1;
        case ExtCmd::SENDRAWKEYS:
            sendrawkeycodes = !sendrawkeycodes;
            ESP_LOGI(TAG, "sendrawkeycodes = %x", sendrawkeycodes);
            setType1Notification();
            return 1;
        case ExtCmd::SWITCHDEBUG:
            c64emu->cpu.debug            = !c64emu->cpu.debug;
            c64emu->cpu.debuggingstarted = false;
            ESP_LOGI(TAG, "debug = %x", c64emu->cpu.debug);
            setType1Notification();
            return 1;
        case ExtCmd::SWITCHPERF:
            c64emu->perf = !c64emu->perf;
            ESP_LOGI(TAG, "perf = %x", c64emu->perf);
            setType1Notification();
            return 1;
        case ExtCmd::SWITCHDETECTRELEASEKEY:
            c64emu->konsoolkb.detectreleasekey = !c64emu->konsoolkb.detectreleasekey;
            ESP_LOGI(TAG, "detectreleasekey = %x", c64emu->konsoolkb.detectreleasekey);
            setType1Notification();
            return 1;
        case ExtCmd::GETBATTERYVOLTAGE: {
            uint32_t voltage = c64emu->batteryVoltage;
            setType5Notification(voltage & 0xff, (voltage >> 8) & 0xff);
            return 5;
        }
        case ExtCmd::POWEROFF: {
            ESP_LOGI(TAG, "power off");
            c64emu->powerOff();
            return 0;
        }
#ifdef BOARD_T_HMI
        case ExtCmd::POWEROFF:
            c64emu->powerOff();
            return 0;
#endif
    }
    return 0;
}
