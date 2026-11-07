#pragma once

#include "C64Emu.hpp"
#include "MenuBaseClass.hpp"
#include "MenuDataStore.hpp"
#include "menuoverlay/MenuTypes.hpp"

class LoadMenu;
class UsbLoadMenu;

class MainMenu : public MenuBaseClass {
   private:
    C64Emu*        c64emu = nullptr;
    LoadMenu*      loadMenu;
    UsbLoadMenu*   usbLoadMenu;
    void           resetC64(MenuItem* item);
    void           exitToLauncher(MenuItem* item);
    MenuDataStore* menuDataStore = MenuDataStore::getInstance();

   public:
    MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController);
    ~MainMenu();
    bool init() override;
    void update() override;
    void displayMenu() const;
    void handleInput(char input);
};
