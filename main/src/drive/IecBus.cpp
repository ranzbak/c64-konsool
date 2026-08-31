#include "IecBus.hpp"

void IecBus::attach(IecDevice* device)
{
    if (device == nullptr) return;
    uint8_t number = device->deviceNumber();
    if (number < MAX_DEVICES) {
        devices[number] = device;
    }
}

void IecBus::detach(uint8_t deviceNumber)
{
    if (deviceNumber >= MAX_DEVICES) return;
    IecDevice* device = devices[deviceNumber];
    if (listener == device) listener = nullptr;
    if (talker == device) talker = nullptr;
    if (pendingListener == device) pendingListener = nullptr;
    if (pendingTalker == device) pendingTalker = nullptr;
    devices[deviceNumber] = nullptr;
}

bool IecBus::hasDevices() const
{
    for (uint8_t i = 0; i < MAX_DEVICES; i++) {
        if (devices[i] != nullptr && devices[i]->present()) return true;
    }
    return false;
}

bool IecBus::listen(uint8_t deviceNumber)
{
    pendingListener = nullptr;
    if (deviceNumber >= MAX_DEVICES) return false;

    IecDevice* device = devices[deviceNumber];
    if (device == nullptr || !device->present()) return false;

    pendingListener = device;
    return true;
}

bool IecBus::talk(uint8_t deviceNumber)
{
    pendingTalker = nullptr;
    if (deviceNumber >= MAX_DEVICES) return false;

    IecDevice* device = devices[deviceNumber];
    if (device == nullptr || !device->present()) return false;

    pendingTalker = device;
    return true;
}

bool IecBus::second(uint8_t secondary)
{
    if (pendingListener == nullptr) return false;
    listener = pendingListener;
    listener->listen(secondary);
    return true;
}

bool IecBus::tksa(uint8_t secondary)
{
    if (pendingTalker == nullptr) return false;
    talker = pendingTalker;
    talker->talk(secondary);
    return true;
}

bool IecBus::ciout(uint8_t value)
{
    if (listener == nullptr) return false;
    return listener->write(value);
}

bool IecBus::acptr(uint8_t* value, bool* eoi)
{
    *value = 0;
    *eoi   = true;
    if (talker == nullptr) return false;
    return talker->read(value, eoi);
}

void IecBus::unlisten()
{
    if (listener != nullptr) {
        listener->unlisten();
        listener = nullptr;
    }
    pendingListener = nullptr;
}

void IecBus::untalk()
{
    if (talker != nullptr) {
        talker->untalk();
        talker = nullptr;
    }
    pendingTalker = nullptr;
}
