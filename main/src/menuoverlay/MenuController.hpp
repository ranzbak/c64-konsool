#pragma once

#include "MenuTypes.hpp"
#include "pax_types.h"

class C64Emu;

class MenuController {
   private:
    C64Emu* c64emu;

    MenuBaseClass* rootMenu;
    MenuBaseClass* currentMenu;

    bool visible = false;

    static void drawMenu();
    pax_buf_t *fb;

   public:
    MenuController();

    C64Emu* getC64Emu() const {
        return c64emu;
    }

    void init(C64Emu* c64emu);
    void handleInput(menu_overlay_input_type_t type);

    // Render the current menu to the framebuffer and update the display
    void render();

    void show();
    void hide();
    void toggle();
    bool getVisible() const;

    void setCurrentMenu(MenuBaseClass* menu) {
        currentMenu = menu;
    }
    void gotoRootMenu() {
        setCurrentMenu(rootMenu);
    }


    void draw();

    // Add any additional methods you might need
};