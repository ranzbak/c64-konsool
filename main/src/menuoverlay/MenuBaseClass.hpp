#pragma once
#include "Theme.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include "MenuDataStore.hpp"
#include "MenuTypes.hpp"
#include "menuoverlay/MenuController.hpp"

class MenuBaseClass {
   private:
    MenuBaseClass*       parentMenu;
    MenuDataStore* const menuDataStore         = MenuDataStore::getInstance();
    size_t               selectedItemIndex     = 0;
    size_t               prevSelectedItemIndex = 0;
    // The list scrolls rather than paging, so a menu may hold far more items
    // than fit on screen. This is the first one drawn.
    size_t               firstVisibleItem      = 0;

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

    size_t getFirstVisibleItem() const
    {
        return firstVisibleItem;
    }
    void setFirstVisibleItem(size_t first)
    {
        firstVisibleItem = first;
    }

    // Navigation
    size_t       getCurrentSelectedIndex(void);
    size_t       getPreviousSelectedIndex(void);
    // How tall a row is on this screen. A settings list is short and can
    // afford to breathe; a directory of a hundred entries cannot.
    virtual int rowHeight() const;

    virtual void navigateBegin();
    void         navigateUp();
    void         navigateDown();
    size_t       getSelectedItemIndex() const;

    // Activate a menu item
    void activateItem(uint16_t id);
};