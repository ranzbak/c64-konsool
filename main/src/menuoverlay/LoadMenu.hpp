#pragma once

#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/MenuTypes.hpp"

class LoadMenu : public MenuBaseClass {
   private:
    C64Emu* c64emu = nullptr;
    void    loadPrg(MenuItem* item);

   public:
    LoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~LoadMenu();

    SDCard sdcard;

    bool init();
    void displayMenu() const;
    void handleInput(char input);
};
