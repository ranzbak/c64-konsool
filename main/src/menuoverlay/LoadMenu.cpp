#include "LoadMenu.hpp"
#include <dirent.h>
#include <cstdio>
#include <string.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "C64Emu.hpp"
#include "Config.hpp"
#include "ExternalCmds.hpp"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "SDCard.hpp"
#include "images/CbmImage.hpp"
#include "menuoverlay/ImageMenu.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "portmacro.h"

const static char* TAG = "LoadMenu";

LoadMenu::LoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController)
{
    c64emu = menuController->getC64Emu();
    sdcard = &c64emu->externalCmds.sdcard;
}

LoadMenu::~LoadMenu()
{
    delete imageMenu;
}

void LoadMenu::displayMenu()
{
    items.clear();

    DIR* dir = opendir(SD_CARD_PRG_PATH);
    if (dir == nullptr) {
        MenuItem item = MenuItem();
        item.id       = 0;
        item.title    = "(No SD card mounted)";
        item.type     = MenuItemType::SPACER;
        items.push_back(item);
        return;
    }
    closedir(dir);

    if (entries.empty()) {
        MenuItem item = MenuItem();
        item.id       = 0;
        item.title    = "(no files found)";
        item.type     = MenuItemType::SPACER;
        items.push_back(item);
        return;
    }

    // Every file goes in the list; the menu overlay scrolls it. Paging was
    // only ever there because the overlay drew every item it was given.
    uint16_t id_count = 0;
    for (size_t i = 0; i < entries.size(); i++) {
        const std::string filename = entries[i];

        MenuItem item = MenuItem();
        item.id       = id_count++;
        item.title    = filename;
        item.type     = MenuItemType::ACTION;
        item.action   = [this, filename](MenuItem* menuItem) {
            (void)menuItem;
            this->openFile(filename);
        };
        items.push_back(item);
    }
}

void LoadMenu::refreshEntries()
{
    entries = SDCard::listLoadableFiles(SD_CARD_PRG_PATH);
}

void LoadMenu::openFile(const std::string& filename)
{
    switch (imageFormatFromName(filename)) {
        case ImageFormat::PRG:
            loadPrg(filename);
            break;
        case ImageFormat::T64:
        case ImageFormat::D64:
            openImage(filename);
            break;
        default:
            ESP_LOGE(TAG, "cannot load %s", filename.c_str());
            break;
    }
}

void LoadMenu::loadPrg(const std::string& filename)
{
    ExternalCmds* ext = &c64emu->externalCmds;

    // Reset first so the program lands in a clean machine, then give the
    // kernal time to finish starting up before dropping it into RAM.
    ext->reset();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ext->loadFile(filename.c_str());
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    menuController->hide();
}

void LoadMenu::openImage(const std::string& filename)
{
    if (imageMenu == nullptr) {
        imageMenu = new ImageMenu(filename, this, menuController);
        imageMenu->init();
    }

    if (!imageMenu->openImage(SDCard::fullPath(filename.c_str()))) {
        ESP_LOGE(TAG, "no loadable programs in %s", filename.c_str());
        // openImage() still leaves a readable placeholder in the submenu, so
        // show it rather than silently ignoring the selection.
    }
    menuController->setCurrentMenu(imageMenu);
}

// Entering the menu starts again at the first page and rereads the directory,
// so a card swapped while the menu was closed shows up.
void LoadMenu::navigateBegin()
{
    needsRefresh = true;
    MenuBaseClass::navigateBegin();
}

void LoadMenu::update()
{
    if (needsRefresh) {
        needsRefresh = false;
        refreshEntries();
        displayMenu();
    }
}

bool LoadMenu::init()
{
    sdcard->init();
    ESP_LOGI(TAG, "initializing load menu");
    needsRefresh = true;
    displayMenu();
    return true;
}
