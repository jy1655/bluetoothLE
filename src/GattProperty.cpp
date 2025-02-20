#include <optional>
#include "GattProperty.h"
#include "Logger.h"

namespace ggk {

GattProperty::GattProperty(const std::string& name, 
                         const std::string& type,
                         bool readable,
                         bool writable)
    : name(name)
    , type(type)
    , readable(readable)
    , writable(writable) {
}

void GattProperty::setGetter(GVariant* (*getter)(void*), void* userData) {
    getterFunc = getter;
    getterUserData = userData;
}

void GattProperty::setSetter(void (*setter)(GVariant*, void*), void* userData) {
    setterFunc = setter;
    setterUserData = userData;
}

void GattProperty::addToInterface(DBusInterface& interface) const {
    // 기존 DBusInterface의 함수 포인터 형식에 맞춤
    GVariant* (*getter)(void) = nullptr;
    void (*setter)(GVariant*) = nullptr;

    if (getterFunc) {
        getter = []() -> GVariant* {
            return nullptr;  // 기존 getterFunc의 래퍼 구현
        };
    }

    if (setterFunc) {
        setter = [](GVariant* value) {
            // 기존 setterFunc의 래퍼 구현
        };
    }

    interface.addProperty(name, type, readable, writable, getter, setter);
}


std::string GattProperty::getPropertyFlags() const {
    std::vector<std::string> flagStrings;
    
    if (hasFlag(READ))
        flagStrings.push_back("read");
    if (hasFlag(WRITE))
        flagStrings.push_back("write");
    if (hasFlag(WRITE_WITHOUT_RESPONSE))
        flagStrings.push_back("write-without-response");
    if (hasFlag(NOTIFY))
        flagStrings.push_back("notify");
    if (hasFlag(INDICATE))
        flagStrings.push_back("indicate");
    if (hasFlag(AUTHENTICATED_SIGNED_WRITES))
        flagStrings.push_back("authenticated-signed-writes");
    if (hasFlag(RELIABLE_WRITE))
        flagStrings.push_back("reliable-write");
    if (hasFlag(WRITABLE_AUXILIARIES))
        flagStrings.push_back("writable-auxiliaries");
    if (hasFlag(ENCRYPT_READ))
        flagStrings.push_back("encrypt-read");
    if (hasFlag(ENCRYPT_WRITE))
        flagStrings.push_back("encrypt-write");
    if (hasFlag(ENCRYPT_AUTHENTICATED_READ))
        flagStrings.push_back("encrypt-authenticated-read");
    if (hasFlag(ENCRYPT_AUTHENTICATED_WRITE))
        flagStrings.push_back("encrypt-authenticated-write");
    if (hasFlag(SECURE_READ))
        flagStrings.push_back("secure-read");
    if (hasFlag(SECURE_WRITE))
        flagStrings.push_back("secure-write");

    std::string result;
    for (size_t i = 0; i < flagStrings.size(); ++i) {
        if (i > 0) result += ",";
        result += flagStrings[i];
    }
    
    return result;
}

} // namespace ggk