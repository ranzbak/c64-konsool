#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "images/CbmImage.hpp"
#include "menuoverlay/MenuTypes.hpp"

// Lists the programs inside a .t64 or .d64 and loads the one that is picked.
class ImageMenu : public MenuBaseClass {
   private:
    C64Emu*                 c64emu = nullptr;
    // The full path is what gets opened, the name is what gets shown.
    std::string             imagePath;
    std::string             imageName;
    std::vector<ImageEntry> imageEntries;

    void displayMenu();
    void loadEntry(uint16_t index);
    void mountDisk();
    void flipDisk();
    bool isDisk = false;

   public:
    // A disk directory runs to a hundred and forty entries, so it takes the
    // dense row and shows sixteen at a time instead of eleven.
    int rowHeight() const override
    {
        return Theme::ROW_H_DENSE;
    }

    ImageMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~ImageMenu() override;

    // Reads the directory of the container at `path` and rebuilds the menu
    // around it. Works for any path, so a USB disk reads the same as the card.
    // Returns false when the container cannot be read or holds nothing
    // loadable.
    bool openImage(const std::string& path);

    bool init() override;
    void update() override;
};
