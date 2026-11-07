#pragma once

#include <string>
#include <vector>
#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/ImageMenu.hpp"
#include "menuoverlay/MenuTypes.hpp"

// Lists the loadable files in the root of a USB disk. A .prg is loaded
// straight away, a .t64 or .d64 opens a submenu listing what is inside it.
class UsbLoadMenu : public MenuBaseClass {
   private:
    C64Emu*    c64emu    = nullptr;
    ImageMenu* imageMenu = nullptr;

    void     openFile(const std::string& filename);
    void     loadPrg(const std::string& filename);
    void     openImage(const std::string& filename);
    // Read once when the menu is opened rather than per page, so the pages
    // stay consistent while walking through them.
    std::vector<std::string> entries;
    void                     refreshEntries();
    bool     needsRefresh = true;

   public:
    UsbLoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~UsbLoadMenu();

    bool init() override;
    void update() override;
    void navigateBegin() override;
    void displayMenu();
};
