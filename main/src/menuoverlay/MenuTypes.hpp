#pragma once

// Navigation input
#include <cstdint>
#include <functional>
#include <string>

// Forward declarations
class MenuBaseClass;

typedef enum _menu_overlay_input_type {
    MENU_OVERLAY_INPUT_TYPE_NONE,
    MENU_OVERLAY_INPUT_TYPE_SELECT,
    MENU_OVERLAY_INPUT_TYPE_UP,
    MENU_OVERLAY_INPUT_TYPE_DOWN,
    MENU_OVERLAY_INPUT_TYPE_LEFT,
    MENU_OVERLAY_INPUT_TYPE_RIGHT,
    MENU_OVERLAY_INPUT_TYPE_BACK,
    MENU_OVERLAY_INPUT_TYPE_LAST,
} menu_overlay_input_type_t;

// Menu item type
enum class MenuItemType {
    ACTION,
    SUBMENU,
    TOGGLE,
    SPACER,
    // Add other types as needed
};

// Menu item structure
struct MenuItem {
    uint16_t                       id;
    MenuItemType                   type = MenuItemType::SPACER;
    std::string                    title = "------";
    std::function<void(MenuItem*)> action = nullptr;
    std::function<void()>          additionalFunction;
    MenuBaseClass*                 submenu = nullptr;
    bool                           disabled = false;
    bool                           checked = false;
    void*                          content = nullptr;
};