#include "MainMenu.hpp"
#include "C64Emu.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "LoadMenu.hpp"

MainMenu::MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController) : MenuBaseClass(title, previousMenu, menuController) {
    // Nothing else to do here
    c64emu = menuController->getC64Emu();
}

MainMenu::~MainMenu() {};

void MainMenu::resetC64(MenuItem* item) {
    ExternalCmds* ext = &c64emu->externalCmds;

    ext->reset();
}

bool MainMenu::init() {
    loadMenu = new LoadMenu("Load PRG", this, menuController); 
    loadMenu->init();

    // Setup the menu entries
    MenuItem* load_prg = new MenuItem();
    load_prg->id       = 1;
    load_prg->title    = "Load PRG";
    load_prg->type     = MenuItemType::SUBMENU;
    load_prg->submenu  = loadMenu;
    items.push_back(*load_prg);
    // Separator
    MenuItem* sep1 = new MenuItem();
    sep1->id       = 2;
    sep1->type     = MenuItemType::SPACER;
    items.push_back(*sep1);
    // Add menu items here
    MenuItem* reset_item = new MenuItem();
    reset_item->id       = 3;
    reset_item->title    = "Reset C64";
    reset_item->type     = MenuItemType::ACTION;
    reset_item->action   = [this](MenuItem* item) { this->resetC64(item); };
    items.push_back(*reset_item);
    // Separator
    MenuItem* sep2 = new MenuItem();
    sep2->id       = 4;
    sep2->type     = MenuItemType::SPACER;
    items.push_back(*sep2);

    return true;
}
