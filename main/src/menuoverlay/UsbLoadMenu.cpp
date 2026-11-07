#include "UsbLoadMenu.hpp"
#include <dirent.h>
#include <cstdio>
#include <string>
#include "C64Emu.hpp"
#include "ExternalCmds.hpp"
#include "SDCard.hpp"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "images/CbmImage.hpp"
#include "menuoverlay/ImageMenu.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "portmacro.h"

static const char* TAG = "UsbLoadMenu";

#define USB_PRG_PATH "/usb"

static std::string usbPath(const std::string& filename)
{
    return std::string(USB_PRG_PATH "/") + filename;
}

UsbLoadMenu::UsbLoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController)
{
    c64emu = menuController->getC64Emu();
}

UsbLoadMenu::~UsbLoadMenu()
{
    delete imageMenu;
}

void UsbLoadMenu::displayMenu()
{
    items.clear();

    DIR* dir = opendir(USB_PRG_PATH);
    if (dir == nullptr) {
        MenuItem item = MenuItem();
        item.id       = 0;
        item.title    = "(No USB disk mounted)";
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

void UsbLoadMenu::refreshEntries()
{
    entries = SDCard::listLoadableFiles(USB_PRG_PATH);
}

void UsbLoadMenu::openFile(const std::string& filename)
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

void UsbLoadMenu::loadPrg(const std::string& filename)
{
    ExternalCmds* ext = &c64emu->externalCmds;

    // Reset first so the program lands in a clean machine, then give the
    // kernal time to finish starting up before dropping it into RAM.
    ext->reset();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ext->loadPrgFromPath(usbPath(filename).c_str());
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    menuController->hide();
}

void UsbLoadMenu::openImage(const std::string& filename)
{
    if (imageMenu == nullptr) {
        imageMenu = new ImageMenu(filename, this, menuController);
        imageMenu->init();
    }

    if (!imageMenu->openImage(usbPath(filename))) {
        ESP_LOGE(TAG, "no loadable programs in %s", filename.c_str());
        // openImage() still leaves a readable placeholder in the submenu, so
        // show it rather than silently ignoring the selection.
    }
    menuController->setCurrentMenu(imageMenu);
}

// Entering the menu starts again at the first page and rereads the directory,
// so a disk plugged in while the menu was closed shows up.
void UsbLoadMenu::navigateBegin()
{
    needsRefresh = true;
    MenuBaseClass::navigateBegin();
}

void UsbLoadMenu::update()
{
    if (needsRefresh) {
        needsRefresh = false;
        refreshEntries();
        displayMenu();
    }
}

bool UsbLoadMenu::init()
{
    ESP_LOGI(TAG, "initializing usb load menu");
    needsRefresh = true;
    displayMenu();
    return true;
}
