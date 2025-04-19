#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "MenuTypes.hpp"
#include "menuoverlay/MenuController.hpp"
#include "MenuDataStore.hpp"

class MenuBaseClass {
   private:
    
    MenuBaseClass* parentMenu;
    MenuDataStore* const menuDataStore = MenuDataStore::getInstance();
    size_t         selectedItemIndex = 0;

   protected:
    std::string           title;
    MenuController*       menuController;
    std::vector<MenuItem> items;

   public:
    MenuBaseClass(std::string title, MenuBaseClass* previousMenu = nullptr, MenuController* menuController = nullptr);

    // Use for menu list to find menus by name
    MenuBaseClass* next = nullptr;

    virtual ~MenuBaseClass();

    // Override to implement specific menu behavior
    virtual bool init();

    virtual void update();

    std::string getTitle() const;

    // Menu structure
    MenuBaseClass*        getParentMenu() const;
    std::vector<MenuItem> getItems() const;

    // Navigation
    void navigateBegin();
    void navigateUp();
    void navigateDown();
    size_t getSelectedItemIndex() const;

    // Activate a menu item
    void activateItem(uint16_t id);
};