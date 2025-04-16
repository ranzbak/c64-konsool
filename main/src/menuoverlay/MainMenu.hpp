#pragma once

#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/MenuTypes.hpp"

class LoadMenu;

class MainMenu : public MenuBaseClass {
   private:
    C64Emu*   c64emu = nullptr;
    LoadMenu* loadMenu;
    bool joystick_emu = false;
    int  joystick_emu_port = 1; // either 1 or 2
    void      resetC64(MenuItem* item);

   public:
    MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~MainMenu();
    bool init() override;
    void update() override {};
    void displayMenu() const;
    void handleInput(char input);
};
