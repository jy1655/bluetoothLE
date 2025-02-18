#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include "DBusInterface.h"
#include "GattUuid.h"
#include "GattProperty.h"

namespace ggk {

class GattDescriptor;
class GattService;

class GattCharacteristic : public DBusInterface {
public:
    static const char* INTERFACE_NAME;    // "org.bluez.GattCharacteristic1"

    GattCharacteristic(const GattUuid& uuid, 
                      GattService* service);
    virtual ~GattCharacteristic() = default;

    // 특성 속성
    const GattUuid& getUUID() const { return uuid; }
    GattService* getService() { return service; }

    // 값 관리
    const std::vector<uint8_t>& getValue() const { return value; }
    bool setValue(const std::vector<uint8_t>& newValue);
    
    // 속성 관리
    void addProperty(GattProperty::Flags flag);
    bool hasProperty(GattProperty::Flags flag) const;

    // 디스크립터 관리
    bool addDescriptor(std::shared_ptr<GattDescriptor> descriptor);
    std::shared_ptr<GattDescriptor> getDescriptor(const GattUuid& uuid);
    const std::vector<std::shared_ptr<GattDescriptor>>& getDescriptors() const {
        return descriptors;
    }

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

    static void onStartNotify(const DBusInterface& interface,
                            GDBusConnection* connection,
                            const std::string& methodName,
                            GVariant* parameters,
                            GDBusMethodInvocation* invocation,
                            void* userData);

    static void onStopNotify(const DBusInterface& interface,
                           GDBusConnection* connection,
                           const std::string& methodName,
                           GVariant* parameters,
                           GDBusMethodInvocation* invocation,
                           void* userData);

    // 알림 관리
    bool isNotifying() const { return notifying; }
    void sendNotification();

    // D-Bus 프로퍼티 관리
    void addManagedObjectProperties(GVariantBuilder* builder);

protected:
    virtual void onValueChanged(const std::vector<uint8_t>& newValue);
    virtual void onNotifyingChanged(bool isNotifying);

private:
    GattUuid uuid;
    GattService* service;
    std::vector<uint8_t> value;
    std::bitset<GattProperty::MAX_FLAGS> properties;
    std::vector<std::shared_ptr<GattDescriptor>> descriptors;
    bool notifying;
    
    void setupProperties();
    void setupMethods();
};

} // namespace ggk