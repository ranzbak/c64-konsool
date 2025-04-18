#pragma once

#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "MenuDataStore.hpp"

class LoadMenu;

class MainMenu : public MenuBaseClass {
   private:
    C64Emu*   c64emu = nullptr;
    LoadMenu* loadMenu;
    void      resetC64(MenuItem* item);
    MenuDataStore* menuDataStore = MenuDataStore::getInstance();

   public:
    MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~MainMenu();
    bool init() override;
    void update() override {};
    void displayMenu() const;
    void handleInput(char input);
};
