#include "MenuController.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include "C64Emu.hpp"
#include "DisplayDriver.hpp"
#include "MainMenu.hpp"
// #include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "icons.h"
#include "menuoverlay/MenuDataStore.hpp"
#include "menuoverlay/MenuTypes.hpp"
#include "menuoverlay/PetsciiText.hpp"
#include "pax_fonts.h"
#include "Theme.hpp"
#include "pax_gfx.h"
#include "pax_text.h"

__attribute__((unused)) static const char* TAG = "MenuController";

extern "C" {
#include "bsp/audio.h"
extern uint8_t* fb_memory;
}

MenuController::MenuController()
{
    return;
}

void MenuController::init(C64Emu* c64emu)
{
    load_icons();
    MenuController::c64emu = c64emu;
    // Setup the main menu
    rootMenu               = new MainMenu("Main Menu", nullptr, this);
    currentMenu            = rootMenu;
    // Initialize the menu overlay
    DisplayDriver* driver  = c64emu->cpu.vic->getDriver();
    fb                     = driver->getMenuFb();
    // HID Pax framebuffer
    pax_buf_init(fb, fb_memory, 800, 480, PAX_BUF_16_565RGB);
    pax_buf_reversed(fb, false);
    pax_background(fb, 0xFF000000);

    // Initialize the menus
    rootMenu->init();

    visible = true;
    driver->enableMenuOverlay(visible);

    // Call render function to initialize the menu overlay
    render();
}

void MenuController::render()
{
    // Allow the current menu to update itself
    currentMenu->update();

    bool full_update = false;

    if (previousMenu != currentMenu) {
        previousMenu = currentMenu;
        full_update  = true;
    }

    int currentJoystick = menuDataStore->getInt("kb_joystick_port", 1);

    const auto& items = currentMenu->getItems();

    // Each screen chooses its own row height, so a short settings list can
    // breathe while a directory of a hundred and forty entries stays
    // browsable. Everything below is derived from it rather than assumed.
    const int    rowH      = currentMenu->rowHeight();
    const size_t MENU_ROWS = static_cast<size_t>(Theme::CONTENT_H / rowH);
    const int    bodySize  = rowH >= Theme::BODY_SIZE + 6 ? Theme::BODY_SIZE : Theme::BODY_SIZE_DENSE;

    // The glyphs are scaled to fill the row exactly. A disk draws its title
    // box out of characters that are meant to touch, so any leading above or
    // below breaks the vertical bars into dashes. At a 24 pixel row that is
    // three screen pixels per ROM pixel; the division is what keeps the two
    // in step if either changes. A CBM name is 16 characters, plus one for
    // the splat marker an unclosed file gets.
    const int   PETSCII_SCALE = rowH / PETSCII_CELL > 0 ? rowH / PETSCII_CELL : 1;
    const float PETSCII_COL_X = Theme::SIDE_PAD;
    const float BLOCKS_X      = PETSCII_COL_X + 17 * PETSCII_CELL * PETSCII_SCALE + 12;

    size_t total = items.size();
    size_t rows  = total < MENU_ROWS ? total : MENU_ROWS;
    size_t first = currentMenu->getFirstVisibleItem();
    size_t sel   = currentMenu->getSelectedItemIndex();

    // Keep the selection on screen, moving the window by as little as possible.
    if (sel < first) first = sel;
    if (sel >= first + MENU_ROWS) first = sel - MENU_ROWS + 1;
    if (total <= MENU_ROWS) {
        first = 0;
    } else if (first + MENU_ROWS > total) {
        first = total - MENU_ROWS;
    }
    if (first != currentMenu->getFirstVisibleItem()) {
        // Everything moved, so the rows that were on screen are all stale.
        currentMenu->setFirstVisibleItem(first);
        full_update = true;
    }

    if (full_update) {
        drawChrome(currentJoystick);
        drawScrollbar(first, rows, total, MENU_ROWS);
    }

    for (size_t row = 0; row < rows; row++) {
        size_t idx = first + row;
        if (idx >= total) break;
        const MenuItem& item = items[idx];

        if (!(full_update || idx == currentMenu->getPreviousSelectedIndex() ||
              idx == currentMenu->getCurrentSelectedIndex())) {
            continue;
        }

        bool  selected = currentMenu->getSelectedItemIndex() == idx;
        bool  inert    = item.type == MenuItemType::SPACER;
        float rowY     = static_cast<float>(Theme::CONTENT_Y + static_cast<int>(row) * rowH);

        // Rows sit on the ground; only the selected one is raised, with a bar
        // down its left edge. That reads at a glance without painting every
        // row a slab of its own.
        pax_draw_rect(fb, (selected && !inert) ? Theme::SURFACE_RAISE : Theme::BACKGROUND, 0, rowY,
                      Theme::SCREEN_W, static_cast<float>(rowH));
        if (selected && !inert) {
            pax_draw_rect(fb, Theme::ACCENT, 0, rowY, Theme::SEL_BAR_W, static_cast<float>(rowH));
        }

        float textY = rowY + static_cast<float>(rowH - bodySize) / 2.0f;

        // A CBM name is PETSCII, and the menu font has no graphics characters
        // in it, so a directory row is drawn from the C64 character ROM. The
        // block count beside it is ASCII and stays in the menu font. Rows that
        // cannot be loaded are the disk's own artwork: they are drawn, but
        // never marked as selected.
        if (!item.petscii.empty()) {
            float glyphY = rowY + static_cast<float>(rowH - 8 * PETSCII_SCALE) / 2.0f;
            pax_draw_petscii(fb, Theme::TEXT_PRIMARY, PETSCII_COL_X, glyphY, item.petscii, PETSCII_SCALE);
            if (!inert && !item.title.empty()) {
                pax_draw_text(fb, Theme::TEXT_MUTED, pax_font_saira_regular, bodySize, BLOCKS_X, textY,
                              item.title.c_str());
            }
            continue;
        }

        if (inert) {
            // A separator with nothing to say is a hairline, not a blank gap.
            if (item.title.empty()) {
                pax_draw_rect(fb, Theme::HAIRLINE, Theme::SIDE_PAD, rowY + static_cast<float>(rowH) / 2.0f,
                              Theme::SCREEN_W - 2 * Theme::SIDE_PAD, 1);
            } else {
                pax_draw_text(fb, Theme::TEXT_MUTED, pax_font_saira_regular, bodySize, Theme::SIDE_PAD,
                              textY, item.title.c_str());
            }
            continue;
        }

        // The titles carry a trailing ": " from when the value was glued onto
        // the end of them. The value has a column of its own now, so drop it.
        std::string title = item.title;
        while (!title.empty() && (title.back() == ' ' || title.back() == ':')) title.pop_back();

        pax_draw_text(fb, Theme::TEXT_PRIMARY, pax_font_saira_regular, bodySize, Theme::SIDE_PAD, textY,
                      title.c_str());

        if (item.type == MenuItemType::TOGGLE) {
            bool checked = menuDataStore->getBool(item.value_name, false);
            pax_draw_text(fb, checked ? Theme::ACCENT : Theme::TEXT_MUTED, pax_font_saira_regular, bodySize,
                          Theme::VALUE_COL, textY, checked ? "On" : "Off");
        }
    }

    prevJoystick = currentJoystick;
}

// The frame around the list: the ground, the bar with the screen's name, and
// the strip of key hints along the bottom. Drawn only on a full update, since
// nothing in it changes as the selection moves.
void MenuController::drawChrome(int currentJoystick)
{
    pax_background(fb, Theme::BACKGROUND);

    pax_draw_rect(fb, Theme::SURFACE, 0, 0, Theme::SCREEN_W, Theme::TOPBAR_H);
    pax_draw_rect(fb, Theme::HAIRLINE, 0, Theme::TOPBAR_H - 1, Theme::SCREEN_W, 1);
    pax_draw_text(fb, Theme::TEXT_PRIMARY, pax_font_saira_regular, Theme::TITLE_SIZE, Theme::SIDE_PAD,
                  static_cast<float>(Theme::TOPBAR_H - Theme::TITLE_SIZE) / 2.0f, currentMenu->getTitle().c_str());

    float hintY = static_cast<float>(Theme::SCREEN_H - Theme::HINTBAR_H);
    pax_draw_rect(fb, Theme::SURFACE, 0, hintY, Theme::SCREEN_W, Theme::HINTBAR_H);
    pax_draw_rect(fb, Theme::HAIRLINE, 0, hintY, Theme::SCREEN_W, 1);

    float textY = hintY + static_cast<float>(Theme::HINTBAR_H - Theme::BODY_SIZE) / 2.0f;
    float iconY = hintY + 4;

    pax_draw_image(fb, get_icon(ICON_F6), Theme::SIDE_PAD, iconY);
    pax_draw_text(fb, Theme::TEXT_MUTED, pax_font_saira_regular, Theme::BODY_SIZE, Theme::SIDE_PAD + 40, textY,
                  "Back to the Commodore 64");

    if (currentMenu == rootMenu) {
        char port[40];
        snprintf(port, sizeof(port), "Joystick port %d", currentJoystick);
        pax_draw_image(fb, get_icon(ICON_F5), 320, iconY);
        pax_draw_text(fb, Theme::TEXT_MUTED, pax_font_saira_regular, Theme::BODY_SIZE, 360, textY, port);
    }
}

// How much of the list is on screen, and where. A thumb in the margin says it
// without spending a row on "1-20 of 144".
void MenuController::drawScrollbar(size_t first, size_t rows, size_t total, size_t visible)
{
    (void)rows;
    if (total <= visible) return;

    float x      = static_cast<float>(Theme::SCREEN_W - Theme::SCROLL_INSET - Theme::SCROLL_W);
    float trackY = static_cast<float>(Theme::CONTENT_Y + Theme::SCROLL_INSET);
    float trackH = static_cast<float>(Theme::CONTENT_H - 2 * Theme::SCROLL_INSET);

    pax_draw_rect(fb, Theme::HAIRLINE, x, trackY, Theme::SCROLL_W, trackH);

    float frac   = static_cast<float>(visible) / static_cast<float>(total);
    float thumbH = trackH * frac;
    if (thumbH < 16.0f) thumbH = 16.0f;
    float pos = static_cast<float>(first) / static_cast<float>(total - visible);

    pax_draw_rect(fb, Theme::ACCENT, x, trackY + pos * (trackH - thumbH), Theme::SCROLL_W, thumbH);
}

void MenuController::setCurrentMenu(MenuBaseClass* menu)
{
    currentMenu = menu;
    menu->navigateBegin();
}

void MenuController::show()
{
    visible = true;
}

void MenuController::hide()
{
    visible = false;
}

void MenuController::toggle()
{
    visible = !visible;
}

bool MenuController::getVisible() const
{
    return visible;
}

void MenuController::handleInput(menu_overlay_input_type_t input)
{
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
        case MENU_OVERLAY_INPUT_TYPE_LAST:
            if (currentMenu->getParentMenu() != nullptr) {
                currentMenu = currentMenu->getParentMenu();
            }
        default:
            break;
    }
    render();
}

// Implement other methods defined in MenuController.hpp
