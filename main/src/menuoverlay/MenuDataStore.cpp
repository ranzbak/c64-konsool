#include "MenuDataStore.hpp"
// #include "esp_log.h"

// static const char *TAG = "MenuDataStore";


MenuDataStoreValue::MenuDataStoreValue() : type(BOOL), b(false) {}
MenuDataStoreValue::MenuDataStoreValue(bool val) : type(BOOL), b(val) {}
MenuDataStoreValue::MenuDataStoreValue(int val) : type(INT), i(val) {}
MenuDataStoreValue::MenuDataStoreValue(const std::string& val) : type(STRING), s(val) {}
MenuDataStoreValue::MenuDataStoreValue(const char* val) : type(STRING), s(val) {}

MenuDataStoreValue::MenuDataStoreValue(const MenuDataStoreValue& other) : type(other.type) {
    switch (type) {
        case BOOL: b = other.b; break;
        case INT: i = other.i; break;
        case STRING: s = other.s; break;
    }
}

MenuDataStoreValue& MenuDataStoreValue::operator=(const MenuDataStoreValue& other) {
    if (this != &other) {
        type = other.type;
        switch (type) {
            case BOOL: b = other.b; break;
            case INT: i = other.i; break;
            case STRING: s = other.s; break;
        }
    }
    return *this;
}

MenuDataStore *MenuDataStore::instance = nullptr;

MenuDataStore* MenuDataStore::getInstance() {
    if (!instance) {
        instance = new MenuDataStore();
    }
    return instance;
}

void MenuDataStore::set(const std::string& key, bool value) {
    store[key] = MenuDataStoreValue(value);
}

void MenuDataStore::set(const std::string& key, int value) {
    store[key] = MenuDataStoreValue(value);
}

void MenuDataStore::set(const std::string& key, const std::string& value) {
    store[key] = MenuDataStoreValue(value);
}

void MenuDataStore::set(const std::string& key, const char* value) {
    store[key] = MenuDataStoreValue(value);
}

bool MenuDataStore::getBool(const std::string& key, bool defaultVal) const {
    // if empty key, raise exception
    auto it = store.find(key);
    // return value if found and type is bool, else return default value
    return (it != store.end() && it->second.type == MenuDataStoreValue::BOOL) ? it->second.b : defaultVal;
}

int MenuDataStore::getInt(const std::string& key, int defaultVal) const {
    auto it = store.find(key);
    return (it != store.end() && it->second.type == MenuDataStoreValue::INT) ? it->second.i : defaultVal;
}

std::string MenuDataStore::getString(const std::string& key, const std::string& defaultVal) const {
    auto it = store.find(key);
    return (it != store.end() && it->second.type == MenuDataStoreValue::STRING) ? it->second.s : defaultVal;
}
