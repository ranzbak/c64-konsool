#ifndef DATASTORE_HPP
#define DATASTORE_HPP

#include <string>
#include <unordered_map>

class MenuDataStoreValue {
public:
    enum Type { BOOL, INT, STRING };

    Type type;
    union {
        bool b;
        int i;
    };
    std::string s;

    MenuDataStoreValue(); 
    MenuDataStoreValue(bool val);
    MenuDataStoreValue(int val);
    MenuDataStoreValue(const std::string& val);
    MenuDataStoreValue(const char* val);
    MenuDataStoreValue(const MenuDataStoreValue& other);
    MenuDataStoreValue& operator=(const MenuDataStoreValue& other);
};

class MenuDataStore {
public:


    void set(const std::string& key, bool value);
    void set(const std::string& key, int value);
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const char* value);

    bool getBool(const std::string& key, bool defaultVal = false) const;
    int getInt(const std::string& key, int defaultVal = 0) const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;

    static MenuDataStore* getInstance(); 

protected:
    static MenuDataStore* instance;

    MenuDataStore() {
        // Singleton
        // No copying
    };
    MenuDataStore(MenuDataStore &other) = delete;
    void operator=(MenuDataStore &other) = delete;
private:
    std::unordered_map<std::string, MenuDataStoreValue> store;
};

#endif // DATASTORE_HPP