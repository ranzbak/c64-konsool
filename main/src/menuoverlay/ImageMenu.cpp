#include "ImageMenu.hpp"
#include <cstdio>
#include "C64Emu.hpp"
#include "ExternalCmds.hpp"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "images/D64Image.hpp"
#include "images/T64Image.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "portmacro.h"

const static char* TAG = "ImageMenu";

ImageMenu::ImageMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController)
{
    c64emu = menuController->getC64Emu();
}

ImageMenu::~ImageMenu()
{
}

bool ImageMenu::init()
{
    return true;
}

bool ImageMenu::openImage(const std::string& path)
{
    imagePath = path;
    size_t slash = path.find_last_of('/');
    imageName    = (slash == std::string::npos) ? path : path.substr(slash + 1);
    imageEntries.clear();

    ImageFormat format = imageFormatFromName(imageName);
    isDisk             = (format == ImageFormat::D64);
    T64Image  t64;
    D64Image  d64;
    CbmImage* image = nullptr;
    if (format == ImageFormat::T64) {
        image = &t64;
    } else if (format == ImageFormat::D64) {
        image = &d64;
    } else {
        ESP_LOGE(TAG, "%s is not a container", imagePath.c_str());
    }

    bool ok = false;
    if (image != nullptr) {
        if (image->open(imagePath.c_str())) {
            // Copy the directory out so the image file does not have to stay
            // open while the user is browsing.
            imageEntries = image->entries();
            image->close();
            ok = !imageEntries.empty();
            ESP_LOGI(TAG, "%s holds %zu programs", imagePath.c_str(), imageEntries.size());
        } else {
            ESP_LOGE(TAG, "cannot read %s", imagePath.c_str());
        }
    }

    // Rebuild either way. Leaving the previous image's entries on screen after
    // a failed open would let the wrong program be picked, and a disk with
    // nothing extractable on it can still be mounted.
    title = imageName;
    displayMenu();
    navigateBegin();
    return ok;
}

void ImageMenu::displayMenu()
{
    items.clear();

    // A disk can be handed to the C64 whole, so it can LOAD from it the way it
    // would from a real drive. That is the only route that works for anything
    // that loads more than one part.
    if (isDisk) {
        MenuItem mountItem = MenuItem();
        mountItem.id       = 0xfffd;
        mountItem.title    = "=== Mount as drive 8 (resets the C64) ===";
        mountItem.type     = MenuItemType::ACTION;
        mountItem.action   = [this](MenuItem* item) {
            (void)item;
            this->mountDisk();
        };
        items.push_back(mountItem);

        // Turning the disk over in the middle of something. Only on offer when
        // there is already a disk in the drive, since otherwise it is just a
        // mount.
        if (c64emu->externalCmds.diskMounted()) {
            MenuItem flipItem = MenuItem();
            flipItem.id       = 0xfffc;
            flipItem.title    = "=== Swap in without resetting ===";
            flipItem.type     = MenuItemType::ACTION;
            flipItem.action   = [this](MenuItem* item) {
                (void)item;
                this->flipDisk();
            };
            items.push_back(flipItem);
        }
    }

    for (size_t i = 0; i < imageEntries.size(); i++) {
        const ImageEntry& entry = imageEntries[i];

        MenuItem item = MenuItem();
        item.id       = entry.index;
        item.petscii  = entry.petscii;

        // With the name drawn from the character ROM the title carries only
        // the block count, which is not a CBM name and stays in the menu font.
        // A slot with no name at all has nothing to draw, so that one falls
        // back to the ASCII rendering.
        char label[40];
        if (item.petscii.empty()) {
            snprintf(label, sizeof(label), "%-16s %4u", entry.name.c_str(), static_cast<unsigned>(entry.blocks));
        } else {
            snprintf(label, sizeof(label), "%4u", static_cast<unsigned>(entry.blocks));
        }
        item.title = label;
        if (!entry.loadable) {
            // Directory art and scratched slots are part of what the disk
            // draws, so they are listed, but there is nothing to load from
            // them and they must not be selectable.
            item.type = MenuItemType::SPACER;
            items.push_back(item);
            continue;
        }
        item.type  = MenuItemType::ACTION;
        uint16_t idx  = entry.index;
        item.action   = [this, idx](MenuItem* menuItem) {
            (void)menuItem;
            this->loadEntry(idx);
        };
        items.push_back(item);
    }

    if (items.empty()) {
        MenuItem empty = MenuItem();
        empty.id       = 0;
        empty.title    = "(no programs found)";
        empty.type     = MenuItemType::SPACER;
        items.push_back(empty);
    }
}

void ImageMenu::loadEntry(uint16_t index)
{
    ExternalCmds* ext = &c64emu->externalCmds;

    // Same sequence the .prg path uses: reset first so the program lands in a
    // clean machine, then give the kernal time to finish its start up before
    // dropping the program into RAM.
    ext->reset();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    ext->loadImageEntryFromPath(imagePath.c_str(), index);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    menuController->hide();
}

void ImageMenu::mountDisk()
{
    ExternalCmds* ext = &c64emu->externalCmds;

    // Reset first so the Kernal starts clean with the drive already attached.
    ext->reset();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    if (ext->mountDiskFromPath(imagePath.c_str())) {
        ESP_LOGI(TAG, "%s mounted as drive 8", imageName.c_str());
    } else {
        ESP_LOGE(TAG, "could not mount %s", imageName.c_str());
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    menuController->hide();
}

// Changes the disk with everything left running, so a demo waiting for the
// next side carries on where it was.
void ImageMenu::flipDisk()
{
    ESP_LOGI(TAG, "swap selected for %s", imagePath.c_str());
    ExternalCmds* ext = &c64emu->externalCmds;

    if (ext->swapDisk(imagePath.c_str())) {
        ESP_LOGI(TAG, "%s swapped in", imageName.c_str());
    } else {
        ESP_LOGE(TAG, "could not swap in %s", imageName.c_str());
    }
    // No reset and no delay: whatever is running should not notice anything
    // beyond the disk changing under it.
    menuController->hide();
}

void ImageMenu::update()
{
}
