#include "MainMenu.hpp"
#include "C64Emu.hpp"
#include "LoadMenu.hpp"
#include "MenuDataStore.hpp"
#include "esp_log.h"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuDataStore.hpp"
#include "menuoverlay/MenuTypes.hpp"

extern "C" {
#include "bsp/audio.h"
}

MainMenu::MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController)
{
    // Nothing else to do here
    c64emu = menuController->getC64Emu();
}

MainMenu::~MainMenu() {};

void MainMenu::resetC64(MenuItem* item)
{
    ExternalCmds* ext = &c64emu->externalCmds;

    ext->reset();
}

bool MainMenu::init()
{
    int id_count = 1;
    loadMenu     = new LoadMenu("Load PRG", this, menuController);
    loadMenu->init();

    MenuDataStore* menuDataStore = MenuDataStore::getInstance();

    // Setup the menu entries
    MenuItem* load_prg = new MenuItem();
    load_prg->id       = id_count++;
    load_prg->title    = "Load PRG";
    load_prg->type     = MenuItemType::SUBMENU;
    load_prg->submenu  = loadMenu;
    items.push_back(*load_prg);

    // Separator
    MenuItem* sep1 = new MenuItem();
    sep1->id       = id_count++;
    sep1->type     = MenuItemType::SPACER;
    items.push_back(*sep1);

    // Add menu items here
    MenuItem* joystick_emu   = new MenuItem();
    joystick_emu->id         = id_count++;
    joystick_emu->title      = "keyboard joystick: ";
    joystick_emu->type       = MenuItemType::TOGGLE;
    joystick_emu->value_name = "kb_joystick_emu";
    menuDataStore->set("kb_joystick_emu", false);
    items.push_back(*joystick_emu);

    // Speaker audio enable/disable
    MenuItem* speaker_emu   = new MenuItem();
    speaker_emu->id         = id_count++;
    speaker_emu->title      = "speaker audio: ";
    speaker_emu->type       = MenuItemType::TOGGLE;
    speaker_emu->value_name = "speaker_ena";
    menuDataStore->set("speaker_ena", false);
    speaker_emu->action     = [](MenuItem* item) {
        MenuDataStore* menuDataStore = MenuDataStore::getInstance();
        bool enabled = menuDataStore->getBool("speaker_ena", true);
        ESP_LOGI("APM", "Toggling speaker audio: %s", enabled? "enabled" : "disabled");
        bsp_audio_set_amplifier(enabled);
    };
    items.push_back(*speaker_emu);

    // Separator
    MenuItem* sep3 = new MenuItem();
    sep3->id       = id_count++;
    sep3->type     = MenuItemType::SPACER;
    items.push_back(*sep3);

    // Add menu items here
    MenuItem* reset_item = new MenuItem();
    reset_item->id       = id_count++;
    reset_item->title    = "Reset C64";
    reset_item->type     = MenuItemType::ACTION;
    reset_item->action   = [this](MenuItem* item) { this->resetC64(item); };
    items.push_back(*reset_item);

    MenuItem* perf_mon = new MenuItem();
    perf_mon->id         = id_count++;
    perf_mon->title      = "Performance Monitor: ";
    perf_mon->type       = MenuItemType::TOGGLE;
    perf_mon->value_name = "perf_mon_ena";
    menuDataStore->set("perf_mon_ena", false);
    perf_mon->action     = [this, menuDataStore](MenuItem* item) {
        bool enabled = menuDataStore->getBool("perf_mon_ena", true);
        // Set the performance monitor enabled/disabled in C64Emu
        this->c64emu->perf = enabled;
    };
    items.push_back(*perf_mon);

    return true;
}
