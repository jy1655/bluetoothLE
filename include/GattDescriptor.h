#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include "DBusInterface.h"
#include "GattUuid.h"

namespace ggk {

class GattCharacteristic;

class GattDescriptor : public DBusInterface {
public:
    static const char* INTERFACE_NAME;    // "org.bluez.GattDescriptor1"

    // 표준 디스크립터 UUID
    static const GattUuid UUID_CHARAC_EXTENDED_PROPERTIES;  // 0x2900
    static const GattUuid UUID_CHARAC_USER_DESCRIPTION;     // 0x2901
    static const GattUuid UUID_CLIENT_CHARAC_CONFIG;        // 0x2902
    static const GattUuid UUID_SERVER_CHARAC_CONFIG;        // 0x2903
    static const GattUuid UUID_CHARAC_PRESENTATION_FORMAT;  // 0x2904
    static const GattUuid UUID_CHARAC_AGGREGATE_FORMAT;     // 0x2905

    GattDescriptor(const GattUuid& uuid,
                  GattCharacteristic* characteristic);
    virtual ~GattDescriptor() = default;

    // 디스크립터 속성
    const GattUuid& getUUID() const { return uuid; }
    GattCharacteristic* getCharacteristic() { return characteristic; }

    // 값 관리
    const std::vector<uint8_t>& getValue() const { return value; }
    bool setValue(const std::vector<uint8_t>& newValue);

    // D-Bus 메서드 핸들러
    static void onReadValue(const DBusInterface& interface,
                          GDBusConnection* connection,
                          const std::string& methodName,
                          GVariant* parameters,
                          GDBusMethodInvocation* invocation,
                          void* userData);

    static void onWriteValue(const DBusInterface& interface,
                           GDBusConnection* connection,
                           const std::string& methodName,
                           GVariant* parameters,
                           GDBusMethodInvocation* invocation,
                           void* userData);

    // D-Bus 프로퍼티 관리
    void addManagedObjectProperties(GVariantBuilder* builder);

protected:
    virtual void onValueChanged(const std::vector<uint8_t>& newValue);

private:
    GattUuid uuid;
    GattCharacteristic* characteristic;
    std::vector<uint8_t> value;

    void setupProperties();
    void setupMethods();
};

} // namespace ggk