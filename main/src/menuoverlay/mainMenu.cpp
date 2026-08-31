#include "MainMenu.hpp"
#include "Config.hpp"
#include "C64Emu.hpp"
#include "LoadMenu.hpp"
#include "MenuDataStore.hpp"
#include "UsbLoadMenu.hpp"
#include "esp_log.h"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuDataStore.hpp"
#include "menuoverlay/MenuTypes.hpp"

extern "C" {
#include "bsp/audio.h"
#include "bsp/device.h"
}

// Data store key holding the joystick port, 1 or 2
static const char* JOYSTICK_PORT_VALUE_NAME = "kb_joystick_port";

// Most games use port 2
static const int JOYSTICK_PORT_DEFAULT = 2;

static std::string joystickPortTitle(int port)
{
    return "Joystick port: " + std::to_string(port);
}

MainMenu::MainMenu(std::string title, MenuBaseClass* previousMenu, MenuController* menuController)
    : MenuBaseClass(title, previousMenu, menuController)
{
    // Nothing else to do here
    c64emu = menuController->getC64Emu();
}

MainMenu::~MainMenu() {};

void MainMenu::update()
{
    // The port can also be switched with F5, so keep the title in sync with the data store
    for (auto& item : items) {
        if (item.value_name == JOYSTICK_PORT_VALUE_NAME) {
            item.title = joystickPortTitle(menuDataStore->getInt(JOYSTICK_PORT_VALUE_NAME, JOYSTICK_PORT_DEFAULT));
        }
    }
}

void MainMenu::resetC64(MenuItem* item)
{
    ExternalCmds* ext = &c64emu->externalCmds;

    ext->reset();
}

void MainMenu::exitToLauncher(MenuItem* item)
{
    bsp_device_restart_to_launcher();
}

bool MainMenu::init()
{
    int id_count = 1;
    loadMenu     = new LoadMenu("Load file from SD card (c64prg folder)", this, menuController);
    loadMenu->init();
    usbLoadMenu = new UsbLoadMenu("Load file from USB disk (disk root)", this, menuController);
    usbLoadMenu->init();

    MenuDataStore* menuDataStore = MenuDataStore::getInstance();

    // Setup the menu entries
    MenuItem* load_prg = new MenuItem();
    load_prg->id       = id_count++;
    load_prg->title    = "Load file from SD card (c64prg folder)";
    load_prg->type     = MenuItemType::SUBMENU;
    load_prg->submenu  = loadMenu;
    items.push_back(*load_prg);

    MenuItem* usb_load_prg = new MenuItem();
    usb_load_prg->id       = id_count++;
    usb_load_prg->title    = "Load file from USB disk (disk root)";
    usb_load_prg->type     = MenuItemType::SUBMENU;
    usb_load_prg->submenu  = usbLoadMenu;
    items.push_back(*usb_load_prg);

    // Joystick port used by the keyboard joystick emulation, and by a USB gamepad
    // unless two player mode puts the gamepad on the other port
    MenuItem* joystick_port   = new MenuItem();
    joystick_port->id         = id_count++;
    joystick_port->type       = MenuItemType::ACTION;
    joystick_port->value_name = JOYSTICK_PORT_VALUE_NAME;
    joystick_port->title      = joystickPortTitle(JOYSTICK_PORT_DEFAULT);
    menuDataStore->set(JOYSTICK_PORT_VALUE_NAME, JOYSTICK_PORT_DEFAULT);
    joystick_port->action = [](MenuItem* item) {
        MenuDataStore* menuDataStore = MenuDataStore::getInstance();
        int            port = menuDataStore->getInt(JOYSTICK_PORT_VALUE_NAME, JOYSTICK_PORT_DEFAULT) == 1 ? 2 : 1;
        menuDataStore->set(JOYSTICK_PORT_VALUE_NAME, port);
        ESP_LOGI("MainMenu", "Switched to joystick port %d", port);
    };
    items.push_back(*joystick_port);

    // True drive emulation. Needs the 1541 ROM on the card, so the toggle
    // reports back if it could not be turned on.
    MenuItem* true_drive   = new MenuItem();
    true_drive->id         = id_count++;
    true_drive->title      = "1541 emulation: ";
    true_drive->type       = MenuItemType::TOGGLE;
    true_drive->value_name = "true_drive_ena";
    menuDataStore->set("true_drive_ena", false);
    true_drive->action     = [this, menuDataStore](MenuItem* item) {
        (void)item;
        bool wanted = menuDataStore->getBool("true_drive_ena", false);
        bool got    = this->c64emu->externalCmds.setTrueDriveEmulation(wanted);
        if (wanted && !got) {
            ESP_LOGE("MainMenu", "no " DRIVE_ROM_FILENAME " on the card");
            menuDataStore->set("true_drive_ena", false);
        }
    };
    items.push_back(*true_drive);

    // Separator
    MenuItem* sep1 = new MenuItem();
    sep1->id       = id_count++;
    sep1->type     = MenuItemType::SPACER;
    items.push_back(*sep1);

    // Add menu items here
    MenuItem* joystick_emu   = new MenuItem();
    joystick_emu->id         = id_count++;
    joystick_emu->title      = "Joystick emulation: ";
    joystick_emu->type       = MenuItemType::TOGGLE;
    joystick_emu->value_name = "kb_joystick_emu";
    menuDataStore->set("kb_joystick_emu", false);
    items.push_back(*joystick_emu);

    // Two players: the keyboard joystick stays on the port above and a USB gamepad
    // takes the other one, so two people can play at the same time
    MenuItem* two_player   = new MenuItem();
    two_player->id         = id_count++;
    two_player->title      = "Two players (gamepad on other port): ";
    two_player->type       = MenuItemType::TOGGLE;
    two_player->value_name = "two_player";
    menuDataStore->set("two_player", false);
    items.push_back(*two_player);

    // Speaker audio enable/disable
    MenuItem* speaker_emu   = new MenuItem();
    speaker_emu->id         = id_count++;
    speaker_emu->title      = "Speaker audio: ";
    speaker_emu->type       = MenuItemType::TOGGLE;
    speaker_emu->value_name = "speaker_ena";
    menuDataStore->set("speaker_ena", true);
    speaker_emu->action = [](MenuItem* item) {
        MenuDataStore* menuDataStore = MenuDataStore::getInstance();
        bool           enabled       = menuDataStore->getBool("speaker_ena", true);
        ESP_LOGI("APM", "Toggling speaker audio: %s", enabled ? "enabled" : "disabled");
        bsp_audio_set_amplifier(enabled);
    };
    items.push_back(*speaker_emu);

    // Add menu items here
    MenuItem* reset_item = new MenuItem();
    reset_item->id       = id_count++;
    reset_item->title    = "Reset C64";
    reset_item->type     = MenuItemType::ACTION;
    reset_item->action   = [this](MenuItem* item) { this->resetC64(item); };
    items.push_back(*reset_item);

    MenuItem* launcher_item = new MenuItem();
    launcher_item->id       = id_count++;
    launcher_item->title    = "Exit and return to launcher";
    launcher_item->type     = MenuItemType::ACTION;
    launcher_item->action   = [this](MenuItem* item) { this->exitToLauncher(item); };
    items.push_back(*launcher_item);

    /*MenuItem* perf_mon   = new MenuItem();
    perf_mon->id         = id_count++;
    perf_mon->title      = "Performance Monitor: ";
    perf_mon->type       = MenuItemType::TOGGLE;
    perf_mon->value_name = "perf_mon_ena";
    menuDataStore->set("perf_mon_ena", false);
    perf_mon->action = [this, menuDataStore](MenuItem* item) {
        bool enabled       = menuDataStore->getBool("perf_mon_ena", true);
        // Set the performance monitor enabled/disabled in C64Emu
        this->c64emu->perf = enabled;
    };
    items.push_back(*perf_mon);*/

    return true;
}
