#include "LoadMenu.hpp"
#include <sys/_default_fcntl.h>
#include "C64Emu.hpp"
#include "ExternalCmds.hpp"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"

const static char* TAG = "LoadMenu";

LoadMenu::LoadMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController) {
    // Nothing else to do here
    c64emu = menuController->getC64Emu();
    // Initialize SD card
    // sdcard.init();
}

LoadMenu::~LoadMenu() {};

void LoadMenu::loadPrg(MenuItem* item) {
    ExternalCmds* ext = &c64emu->externalCmds;

    ext->loadPrg(item->title.c_str());
}

bool LoadMenu::init() {
    ESP_LOGI(TAG, "Initializing load menu...");
    // Setup the menu entries
    MenuItem* giana = new MenuItem();
    giana->id       = 1;
    giana->title    = "giana";
    giana->type     = MenuItemType::ACTION;
    giana->action   = [this](MenuItem* item) { this->loadPrg(item); };
    this->items.push_back(*giana);
    // Separator
    MenuItem* pooyan = new MenuItem();
    pooyan->id       = 2;
    pooyan->title    = "pooyan";
    pooyan->type     = MenuItemType::ACTION;
    pooyan->action   = [this](MenuItem* item) { this->loadPrg(item); };
    this->items.push_back(*pooyan);
    // Add menu items here
    MenuItem* boulderdash = new MenuItem();
    boulderdash->id       = 3;
    boulderdash->title    = "boulderdash";
    boulderdash->type     = MenuItemType::ACTION;
    boulderdash->action   = [this](MenuItem* item) { this->loadPrg(item); };
    this->items.push_back(*boulderdash);
    // Separator
    MenuItem* sep1 = new MenuItem();
    sep1->id       = 4;
    sep1->type     = MenuItemType::SPACER;
    items.push_back(*sep1);


    return true;
}
