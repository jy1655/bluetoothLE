#pragma once
#include <gio/gio.h>
#include <string>
#include <bitset>

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
        // BlueZ 특화 플래그
        RELIABLE_WRITE = 8,
        WRITABLE_AUXILIARIES = 9,
        ENCRYPT_READ = 10,
        ENCRYPT_WRITE = 11,
        ENCRYPT_AUTHENTICATED_READ = 12,
        ENCRYPT_AUTHENTICATED_WRITE = 13,
        SECURE_READ = 14,
        SECURE_WRITE = 15,
        MAX_FLAGS
    };

    // 기존 생성자 유지
    GattProperty(const std::string& name, 
                GVariant* pValue,
                GDBusInterfaceGetPropertyFunc getter = nullptr,
                GDBusInterfaceSetPropertyFunc setter = nullptr);

    // 플래그 관리를 위한 메서드 추가
    void setFlag(Flags flag, bool value = true) {
        flags.set(flag, value);
    }
    
    bool hasFlag(Flags flag) const {
        return flags.test(flag);
    }

    // BlueZ 프로퍼티 플래그 문자열 생성
    std::string getPropertyFlags() const;

    // 기존 메서드들 유지
    const std::string& getName() const;
    GattProperty& setName(const std::string& name);
    const GVariant* getValue() const;
    GattProperty& setValue(GVariant* pValue);
    GDBusInterfaceGetPropertyFunc getGetterFunc() const;
    GattProperty& setGetterFunc(GDBusInterfaceGetPropertyFunc func);
    GDBusInterfaceSetPropertyFunc getSetterFunc() const;
    GattProperty& setSetterFunc(GDBusInterfaceSetPropertyFunc func);
    std::string generateIntrospectionXML(int depth) const;

private:
    std::string name;
    GVariant* pValue;
    GDBusInterfaceGetPropertyFunc getterFunc;
    GDBusInterfaceSetPropertyFunc setterFunc;
    std::bitset<MAX_FLAGS> flags;
};

} // namespace ggk