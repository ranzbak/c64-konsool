#include "MenuController.hpp"
#include <cstring>
#include <string>
#include "C64Emu.hpp"
#include "MainMenu.hpp"
#include "esp_log.h"
#include "pax_gfx.h"
#include "soc/clk_tree_defs.h"

static const char* TAG = "MenuController";

MenuController::MenuController() {
    return;
}

void MenuController::init(C64Emu* c64emu) {
    MenuController::c64emu = c64emu;
    // Setup the main menu
    rootMenu               = new MainMenu("Main Menu", nullptr, this);
    currentMenu            = rootMenu;
    // Initialize the menu overlay
    fb                     = c64emu->cpu.vic->getDriver()->getMenuFb();
    // HID Pax framebuffer
    pax_buf_init(fb, NULL, 640, 400, PAX_BUF_16_565RGB);
    pax_buf_reversed(fb, true);
    pax_background(fb, 0xff000000);

    // Initialize the menus
    rootMenu->init();

    // Call render function to initialize the menu overlay
    render();
}

void MenuController::render() {
    pax_background(fb, 0xff000000);
    pax_draw_rect(fb, 0xffffffff, 0, 0, 640, 40);
    pax_draw_text(fb, 0xff000000, pax_font_sky_mono, 16, 10, 10, currentMenu->getTitle().c_str());

    const auto& items = currentMenu->getItems();
    ESP_LOGI(TAG, "Menu items: %d", items.size());
    size_t i = 0;
    for (const auto& item : items) {
        uint32_t    color = currentMenu->getSelectedItemIndex() == i ? 0xff0000ff : 0xffffffff;
        std::string title = ("-- " + item.title + " --");
        ESP_LOGI(TAG, "Menu Item %d: %s", i, title.c_str());
        pax_draw_text(fb, color, pax_font_sky_mono, 16, 30, 60 + i * 20, title.c_str());
        ++i;
    }
}

void MenuController::show() {
    visible = true;
}

void MenuController::hide() {
    visible = false;
}

void MenuController::toggle() {
    visible = !visible;
}

bool MenuController::getVisible() const {
    return visible;
}

void MenuController::handleInput(menu_overlay_input_type_t input) {
    // Handle user input for menu navigation and selection
    switch (input) {
        case MENU_OVERLAY_INPUT_TYPE_UP:
            currentMenu->navigateUp();
            break;
        case MENU_OVERLAY_INPUT_TYPE_DOWN:
            currentMenu->navigateDown();
            break;
        case MENU_OVERLAY_INPUT_TYPE_SELECT:
            currentMenu->activateItem(currentMenu->getSelectedItemIndex());
            break;
        default:
            break;
    }
}

// Implement other methods defined in MenuController.hpp