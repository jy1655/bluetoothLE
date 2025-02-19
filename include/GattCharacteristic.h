#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include <map>
#include "DBusInterface.h"
#include "GattUuid.h"
#include "GattProperty.h"
#include "GattDescriptor.h"

namespace ggk {

class GattCharacteristic : public DBusInterface {
public:
    static const char* INTERFACE_NAME;    // "org.bluez.GattCharacteristic1"

    // 특성 속성 플래그
    enum class Property {
        BROADCAST = 0,
        READ = 1,
        WRITE_WITHOUT_RESPONSE = 2,
        WRITE = 3,
        NOTIFY = 4,
        INDICATE = 5,
        SIGNED_WRITE = 6,
        EXTENDED_PROPERTIES = 7
    };

    // 생성자
    GattCharacteristic(const GattUuid& uuid, const DBusObjectPath& path);
    virtual ~GattCharacteristic() = default;

    // DBusInterface 구현
    virtual DBusObjectPath getPath() const override { return objectPath; }

    // 특성 속성
    const GattUuid& getUUID() const { return uuid; }
    void addProperty(Property prop);
    bool hasProperty(Property prop) const;
    std::string getPropertyFlags() const;

    // 값 관리
    const std::vector<uint8_t>& getValue() const { return value; }
    bool setValue(const std::vector<uint8_t>& newValue);
    
    // 디스크립터 관리
    bool addDescriptor(std::shared_ptr<GattDescriptor> descriptor);
    std::shared_ptr<GattDescriptor> getDescriptor(const GattUuid& uuid);
    const std::vector<std::shared_ptr<GattDescriptor>>& getDescriptors() const { 
        return descriptors; 
    }

    // 알림/인디케이션 관리
    bool isNotifying() const { return notifying; }
    void startNotify();
    void stopNotify();

    // CCCD 관리
    void onCCCDChanged(bool notificationEnabled, bool indicationEnabled);

protected:
    virtual void onValueChanged(const std::vector<uint8_t>& newValue);
    virtual void onNotifyingChanged(bool isNotifying);

private:
    GattUuid uuid;
    DBusObjectPath objectPath;
    std::vector<uint8_t> value;
    std::bitset<8> properties;
    std::vector<std::shared_ptr<GattDescriptor>> descriptors;
    bool notifying{false};

    void setupProperties();
    void setupMethods();
    bool validateProperties() const;
    std::shared_ptr<GattDescriptor> createCCCD();

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
};

} // namespace ggk