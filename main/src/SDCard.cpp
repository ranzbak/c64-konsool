#include "SDCard.hpp"
#include "Config.hpp"
#include "driver/sdmmc_default_configs.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
// #include "sdmmc_cmd.h"
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>

static const char *TAG = "SDCard";

SDCard::SDCard() : initialized(false) {}

#include "driver/sdmmc_host.h"

sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

bool SDCard::init() {
#if defined(USE_SDCARD)
    if (initialized) {
        return true;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return false;
    }

    initialized = true;
    return true;
#else
    return false;
#endif
}

static void getPath(char *path, uint8_t *ram) {
    uint8_t cury = ram[0xd6];
    uint8_t curx = ram[0xd3];
    uint8_t *cursorpos = ram + 0x0400 + cury * 40 + curx;
    cursorpos--;
    while (*cursorpos == 32 && cursorpos >= ram + 0x0400) cursorpos--;
    while ((*cursorpos != 32) && (cursorpos >= ram + 0x0400)) cursorpos--;
    cursorpos++;
    path[0] = '/';
    uint8_t i = 1, p;
    while (((p = *cursorpos++) != 32) && (p != 160) && (i < 17)) {
        if ((p >= 1) && (p <= 26)) path[i] = p + 96;
        else if ((p >= 33) && (p <= 63)) path[i] = p;
        i++;
    }
    strcpy(&path[i], ".prg");
}

uint16_t SDCard::load(const char *path, uint8_t *ram, size_t len) {
    if (!initialized) return 0;
    getPath(const_cast<char *>(path), ram);
    ESP_LOGI(TAG, "load file %s", path);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    uint8_t hdr[2];
    if (read(fd, hdr, 2) != 2) {
        close(fd);
        return 0;
    }
    uint16_t addr = hdr[0] | (hdr[1] << 8);
    uint16_t pos = addr;
    while (read(fd, &ram[pos], 1) == 1) pos++;
    close(fd);
    return pos;
}

bool SDCard::save(const char *path, const uint8_t *ram, size_t len) {
    if (!initialized) return false;
    getPath(const_cast<char *>(path), const_cast<uint8_t *>(ram));
    uint16_t startaddr = ram[43] + ram[44] * 256;
    uint16_t endaddr = ram[45] + ram[46] * 256;
    ESP_LOGI(TAG, "save file %s", path);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;

    write(fd, &ram[43], 2);
    write(fd, &ram[startaddr], endaddr - startaddr);
    close(fd);
    return true;
}

bool SDCard::listNextEntry(char *nextentry, size_t entrySize, bool start) {
    static DIR *dir = nullptr;
    static struct dirent *ent;

    if (!initialized) return false;

    if (start) {
        if (dir) {
            closedir(dir);
            dir = nullptr;
        }
        dir = opendir("/sdcard");
        if (!dir) {
            ESP_LOGI(TAG, "cannot open root dir");
            return false;
        }
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".prg") == 0) {
            char fname[17] = {};
            strncpy(fname, name, len - 4);
            for (uint8_t i = 0; i < 16; i++) {
                uint8_t p = fname[i];
                if ((p >= 97) && (p <= 122)) nextentry[i] = p - 32;
                else nextentry[i] = p;
            }
            nextentry[16] = '\0';
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
