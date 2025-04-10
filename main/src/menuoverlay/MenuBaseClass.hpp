#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "MenuTypes.hpp"
#include "menuoverlay/MenuController.hpp"

class MenuBaseClass {
   private:
    MenuBaseClass* parentMenu;
    size_t         selectedItemIndex = 0;

   protected:
    std::string           title;
    MenuController*       menuController;
    std::vector<MenuItem> items;

   public:
    MenuBaseClass(std::string title, MenuBaseClass* previousMenu = nullptr, MenuController* menuController = nullptr);

    virtual ~MenuBaseClass();

    // Override to implement specific menu behavior
    virtual bool init();

    std::string getTitle() const;

    // Menu structure
    MenuBaseClass*        getParentMenu() const;
    std::vector<MenuItem> getItems() const;

    // Navigation
    void navigateUp();
    void navigateDown();
    size_t getSelectedItemIndex() const;

    // Activate a menu item
    void activateItem(uint16_t id);
};