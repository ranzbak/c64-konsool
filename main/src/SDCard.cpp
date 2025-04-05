#include "SDCard.hpp"
#include "Config.hpp"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <cstring>
#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_intr_types.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "hal/ldo_types.h"
#include "hal/spi_types.h"
#include "sd_protocol_types.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "soc/gpio_num.h"
#include "targets/tanmatsu/tanmatsu_hardware.h"

static const char* TAG = "SDCard";

SDCard::SDCard() : initialized(false) {
}

SDCard::~SDCard() {
    if (initialized) {
        sdspi_host_deinit();
    }
}

bool SDCard::init() {
    esp_err_t ret;

#if defined(USE_SDCARD)
    if (initialized) {
        return true;
    }

    // ESP_LOGI(TAG, "Initialize SDCard power");

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = LDO_UNIT_4, // SDCard powered by VO4
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return false;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Setup sdio slot");

    slot_config.clk = static_cast<gpio_num_t>(BSP_SDCARD_CLK);
    slot_config.cmd = static_cast<gpio_num_t>(BSP_SDCARD_CMD);
    slot_config.d0  = static_cast<gpio_num_t>(BSP_SDCARD_D0);
    slot_config.d1  = static_cast<gpio_num_t>(BSP_SDCARD_D1);
    slot_config.d2  = static_cast<gpio_num_t>(BSP_SDCARD_D2);
    slot_config.d3  = static_cast<gpio_num_t>(BSP_SDCARD_D3);
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting SDcard");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 5,
        .allocation_unit_size     = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    const char mount_point[] = SD_CARD_MOUNT_POINT;

    ret = esp_vfs_fat_sdmmc_mount(
        mount_point,
        &host,
        &slot_config,
        &mount_config,
        &mount_card
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return false;
    }

    // Get some info about the card
    sdmmc_card_print_info(stdout, mount_card);

    ESP_LOGI(TAG, "SDcard initialized");

    initialized = true;
    return true;
#else
    return false;
#endif
}

void getPath(char *path, uint8_t *ram) {
    uint8_t cury = ram[0xd6];
    uint8_t curx = ram[0xd3];
    uint8_t *cursorpos = ram + 0x0400 + cury * 40 + curx;
    cursorpos--; // char may be 160
    while (*cursorpos == 32) {
      cursorpos--;
    }
    while ((*cursorpos != 32) && (cursorpos >= ram + 0x0400)) {
      cursorpos--;
    }
    cursorpos++;
    path[0] = '/';
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
    path[i] = '\0';
  }

uint16_t SDCard::load(const char* path, uint8_t* ram, size_t len) {
    char file_path[64] = {0};
    if (!initialized) return 0;
    getPath(file_path, ram);
    ESP_LOGI(TAG, "load file %s", path);

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_CARD_MOUNT_POINT, file_path);
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) return 0;

    uint8_t hdr[2];
    if (read(fd, hdr, 2) != 2) {
        close(fd);
        return 0;
    }
    uint16_t addr = hdr[0] | (hdr[1] << 8);
    uint16_t pos  = addr;
    while (read(fd, &ram[pos], 1) == 1) pos++;
    close(fd);
    return pos;
}

bool SDCard::save(const char* path, const uint8_t* ram, size_t len) {
    if (!initialized) return false;
    getPath(const_cast<char*>(path), const_cast<uint8_t*>(ram));
    uint16_t startaddr = ram[43] + ram[44] * 256;
    uint16_t endaddr   = ram[45] + ram[46] * 256;
    ESP_LOGI(TAG, "save file %s", path);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;

    write(fd, &ram[43], 2);
    write(fd, &ram[startaddr], endaddr - startaddr);
    close(fd);
    return true;
}

bool SDCard::listNextEntry(uint8_t* nextentry, size_t entrySize, bool start) {
    static DIR*           dir = nullptr;
    static struct dirent* ent;

    if (!initialized) return false;

    if (start) {
        if (dir) {
            closedir(dir);
            dir = nullptr;
        }
        dir = opendir(SD_CARD_MOUNT_POINT);
        if (!dir) {
            ESP_LOGI(TAG, "cannot open root dir");
            return false;
        }
    }

    while ((ent = readdir(dir)) != nullptr) {
        const char* name = ent->d_name;
        size_t      len  = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".prg") == 0) {
            char fname[17] = {};
            size_t copy_len = len - 4;
            if (copy_len >= sizeof(fname)) copy_len = sizeof(fname) - 1;
            memcpy(fname, name, copy_len);
            for (uint8_t i = 0; i < 16; i++) {
                uint8_t p = fname[i];
                if ((p >= 97) && (p <= 122))
                    nextentry[i] = p - 32;
                else
                    nextentry[i] = p;
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
