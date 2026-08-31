#include "SDCard.hpp"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <strings.h>
#include <algorithm>
#include "Config.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "hal/ldo_types.h"

static const char* TAG = "SDCard";

SDCard::SDCard() : initialized(false)
{
}

SDCard::~SDCard()
{
    if (initialized) {
    }
}

bool SDCard::init()
{
    if (initialized) {
        return true;
    }

    initialized = true;
    return true;
}

void getPath(char* path, uint8_t* ram)
{
    uint8_t  cury      = ram[0xd6];
    uint8_t  curx      = ram[0xd3];
    uint8_t* cursorpos = ram + 0x0400 + cury * 40 + curx;
    cursorpos--;  // char may be 160
    while (*cursorpos == 32) {
        cursorpos--;
    }
    while ((*cursorpos != 32) && (cursorpos >= ram + 0x0400)) {
        cursorpos--;
    }
    cursorpos++;
    path[0]   = '/';
    uint8_t i = 1;
    uint8_t p;
    while (((p = *cursorpos++) != 32) && (p != 160) && (i < 17)) {
        if ((p >= 1) && (p <= 26)) {
            path[i] = p + 96;
        } else if ((p >= 33) && (p <= 63)) {
            path[i] = p;
        }
        i++;
    }
    path[i++] = '.';
    path[i++] = 'p';
    path[i++] = 'r';
    path[i++] = 'g';
    path[i]   = '\0';
}

std::string SDCard::fullPath(const char* filename) {
    std::string path = SD_CARD_PRG_PATH;
    if (filename == nullptr || filename[0] == '\0') return path;
    if (filename[0] != '/') path += '/';
    path += filename;
    return path;
}

// Reads a .prg into RAM: two byte load address followed by the data. Returns
// the address one past the last byte written, or 0 if nothing was loaded.
uint16_t SDCard::readPrg(const char* full_path, uint8_t* ram)
{
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) return 0;

    uint8_t hdr[2];
    if (read(fd, hdr, 2) != 2) {
        close(fd);
        return 0;
    }
    uint16_t addr = hdr[0] | (hdr[1] << 8);

    // A program longer than the space above its load address would otherwise
    // run straight off the end of the 64K RAM buffer, so stop at $FFFF.
    size_t pos  = addr;
    size_t room = (C64_RAM_SIZE - 1) - addr;
    while (room > 0) {
        ssize_t got = read(fd, ram + pos, room);
        if (got <= 0) break;
        pos  += static_cast<size_t>(got);
        room -= static_cast<size_t>(got);
    }
    close(fd);

    if (pos == addr) return 0;
    return static_cast<uint16_t>(pos);
}

uint16_t SDCard::load(const char* path, uint8_t* ram, size_t len) {
    (void)len;
    return readPrg(fullPath(path).c_str(), ram);
}

uint16_t SDCard::load_auto(const char* path, uint8_t* ram, size_t len)
{
    (void)path;
    (void)len;
    char file_path[64] = {0};
    if (!initialized) return 0;
    // The file name comes from the BASIC LOAD command still on screen.
    getPath(file_path, ram);
    ESP_LOGI(TAG, "load file %s", file_path);

    return readPrg(fullPath(file_path).c_str(), ram);
}

bool SDCard::save(const char* path, const uint8_t* ram, size_t len)
{
    (void)path;
    (void)len;
    if (!initialized) return false;

    // getPath() writes the name it scrapes off the screen into the buffer it
    // is given, so it needs somewhere writable of its own.
    char file_path[64] = {0};
    getPath(file_path, const_cast<uint8_t*>(ram));

    uint16_t startaddr = ram[43] + ram[44] * 256;
    uint16_t endaddr   = ram[45] + ram[46] * 256;
    if (endaddr <= startaddr) {
        ESP_LOGI(TAG, "nothing to save, start $%04x end $%04x", startaddr, endaddr);
        return false;
    }

    std::string full_path = fullPath(file_path);
    ESP_LOGI(TAG, "save file %s", full_path.c_str());

    int fd = open(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;

    write(fd, &ram[43], 2);
    write(fd, &ram[startaddr], endaddr - startaddr);
    close(fd);
    return true;
}

std::vector<std::string> SDCard::listLoadableFiles(const char* path)
{
    std::vector<std::string> result;
    bool                     truncated = false;

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        ESP_LOGI(TAG, "cannot open %s", path);
        return result;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (imageFormatFromName(name) == ImageFormat::UNKNOWN) continue;

        if (result.size() >= MAX_LISTED_FILES) {
            truncated = true;
            break;
        }
        result.push_back(name);
    }
    closedir(dir);

    if (truncated) {
        ESP_LOGW(TAG, "%s holds more than %u loadable files, the rest are not listed", path,
                 static_cast<unsigned>(MAX_LISTED_FILES));
    }

    // readdir hands entries back in whatever order the filesystem stored them,
    // so a file sits wherever it was written rather than where it is looked
    // for. Sorting also keeps the pages steady from one refresh to the next.
    std::sort(result.begin(), result.end(), [](const std::string& a, const std::string& b) {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    });

    ESP_LOGI(TAG, "%zu loadable files in %s", result.size(), path);
    return result;
}

bool SDCard::listNextEntry(uint8_t* nextentry, size_t entrySize, bool start)
{
    static DIR*           dir = nullptr;
    static struct dirent* ent;

    if (!initialized) return false;

    if (start) {
        if (dir) {
            closedir(dir);
            dir = nullptr;
        }

        dir = opendir(SD_CARD_PRG_PATH);
        if (!dir) {
            ESP_LOGI(TAG, "cannot open root dir");
            return false;
        }
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char* name = ent->d_name;
        size_t      len  = strlen(name);
        ESP_LOGI(TAG, "found file: %s", name);
        if (len > 4 && strcmp(name + len - 4, ".prg") == 0) {
            char   fname[17] = {};
            size_t copy_len  = len - 4;
            if (copy_len >= sizeof(fname)) copy_len = sizeof(fname) - 1;
            memcpy(nextentry, name, copy_len);
            nextentry[copy_len] = '\0';
            nextentry[16]       = '\0';
            return true;
        }
    }

    if (dir) {
        closedir(dir);
        dir = nullptr;
    }
    nextentry[0] = '\0';
    return true;
}
