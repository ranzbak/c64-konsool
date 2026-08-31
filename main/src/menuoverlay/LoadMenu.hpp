#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/ImageMenu.hpp"
#include "menuoverlay/MenuTypes.hpp"

// Lists the loadable files in the program directory on the SD card. A .prg is
// loaded straight away, a .t64 or .d64 opens a submenu listing what is inside
// it.
class LoadMenu : public MenuBaseClass {
   private:
    C64Emu*    c64emu    = nullptr;
    ImageMenu* imageMenu = nullptr;

    void openFile(const std::string& filename);
    void loadPrg(const std::string& filename);
    void openImage(const std::string& filename);

    // The whole sorted listing, read once when the menu is opened rather than
    // per page, so the pages stay consistent while walking through them.
    std::vector<std::string> entries;
    void                     refreshEntries();
    // The card can be swapped while the menu sits there, so entering the menu
    // always rereads the directory rather than trusting what is on screen.
    bool     needsRefresh = true;

   public:
    LoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~LoadMenu();

    SDCard* sdcard;

    bool init() override;
    void update() override;
    void navigateBegin() override;
    void displayMenu();
};
