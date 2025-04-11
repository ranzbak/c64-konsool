#pragma once

#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/MenuTypes.hpp"

class LoadMenu : public MenuBaseClass {
   private:
    C64Emu*               c64emu = nullptr;
    std::vector<MenuItem> getDirPage(uint16_t page);
    void                  displayMenu() const;
    void                  loadPrg(MenuItem* item);
    uint16_t              currentPage = 0;
    uint16_t              nextPage    = 0;
    size_t                pageSize    = 12;

   public:
    LoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~LoadMenu();

    SDCard* sdcard;

    bool init() override;
    void update() override;
    void toPrevPage();
    void toNextPage();
    void displayMenu();
    void handleInput(char input);
};
