#pragma once

#include <gio/gio.h>
#include <string>
#include <bitset>
#include "DBusInterface.h"

namespace ggk {

class GattProperty {
public:
    // GATT 속성 플래그
    enum Flags {
        BROADCAST = 0,
        READ = 1,
        WRITE_WITHOUT_RESPONSE = 2,
        WRITE = 3,
        NOTIFY = 4,
        INDICATE = 5,
        AUTHENTICATED_SIGNED_WRITES = 6,
        EXTENDED_PROPERTIES = 7,
        RELIABLE_WRITE = 8,
        WRITABLE_AUXILIARIES = 9,
        ENCRYPT_READ = 10,
        ENCRYPT_WRITE = 11,
        ENCRYPT_AUTHENTICATED_READ = 12,
        ENCRYPT_AUTHENTICATED_WRITE = 13,
        SECURE_READ = 14,
        SECURE_WRITE = 15,
        MAX_FLAGS = 16
    };

    GattProperty(const std::string& name, 
                const std::string& type,
                bool readable,
                bool writable);
    
    virtual ~GattProperty() = default;

    // GATT 특화 기능
    void setFlag(Flags flag, bool value = true) {
        flags.set(static_cast<size_t>(flag), value);
    }
    
    bool hasFlag(Flags flag) const {
        return flags.test(static_cast<size_t>(flag));
    }

    // Getter/Setter 관리
    void setGetter(GVariant* (*getter)(void*), void* userData);
    void setSetter(void (*setter)(GVariant*, void*), void* userData);

    // DBus 인터페이스 통합
    void addToInterface(DBusInterface& interface) const;

    // 플래그 문자열 생성
    std::string getPropertyFlags() const;

private:
    std::string name;
    std::string type;
    bool readable;
    bool writable;
    std::bitset<MAX_FLAGS> flags;

    GVariant* (*getterFunc)(void*) = nullptr;
    void (*setterFunc)(GVariant*, void*) = nullptr;
    void* getterUserData = nullptr;
    void* setterUserData = nullptr;
};

} // namespace ggk