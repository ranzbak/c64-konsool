
#include "MenuBaseClass.hpp"
#include "esp_log.h"
#include "menuoverlay/MenuController.hpp"
#include "menuoverlay/MenuTypes.hpp"

const static char* TAG = "MenuBaseClass";

MenuBaseClass::MenuBaseClass(std::string title, MenuBaseClass* previousMenu, MenuController* menuController) {
    this->title          = title;
    this->parentMenu     = previousMenu;
    this->menuController = menuController;
}

MenuBaseClass::~MenuBaseClass() = default;

bool MenuBaseClass::init() {
    // Implement logic to initialize the menu items
    return true;
};

void MenuBaseClass::update() {
    // Implement logic to update the menu items based on user input
};

std::string MenuBaseClass::getTitle() const {
    return title;
};

MenuBaseClass* MenuBaseClass::getParentMenu() const {
    return parentMenu;
};

std::vector<MenuItem> MenuBaseClass::getItems() const {
    return items;
};

void MenuBaseClass::navigateBegin() {
    selectedItemIndex = 0;
}

void MenuBaseClass::navigateUp() {
    if (selectedItemIndex > 0) {
        selectedItemIndex--;
    } else {
        selectedItemIndex = items.size() - 1;
    }
    // menuController->render();
}

void MenuBaseClass::navigateDown() {
    if (selectedItemIndex < items.size() - 1) {
        selectedItemIndex++;
    } else {
        selectedItemIndex = 0;
    }
    // menuController->render();
}

size_t MenuBaseClass::getSelectedItemIndex() const {
    return selectedItemIndex;
}

void MenuBaseClass::activateItem(uint16_t id) {
    // Implement logic to activate the selected menu item
    if (id >= items.size()) {
        ESP_LOGI(TAG, "Invalid menu item ID: %d", id);
        return;
    }
    MenuItem item = items[selectedItemIndex];
    if (item.disabled) {
        return;
    }
    switch (item.type) {
        case MenuItemType::ACTION:
            // Perform action
            item.action(&item);
            break;
        case MenuItemType::SUBMENU:
            // Open submenu
            ESP_LOGI(TAG, "Opening submenu: %s", item.title.c_str());
            menuController->setCurrentMenu(item.submenu);
            // menuController->render();
            break;
        case MenuItemType::TOGGLE:
            item.checked = !item.checked;
            // Toggle state
            break;
        // Add other types as needed
        default:
            break;
    }
}
