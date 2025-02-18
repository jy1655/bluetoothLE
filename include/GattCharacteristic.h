#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include <bitset>
#include "DBusInterface.h"
#include "DBusProperty.h" 
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
    
    // BLE 속성 관리
    void addFlag(GattProperty::Flags flag);  // addProperty 대신 addFlag로 변경
    bool hasFlag(GattProperty::Flags flag) const;
    std::string getPropertyFlags() const;  // 추가

    // 디스크립터 관리
    bool addDescriptor(std::shared_ptr<GattDescriptor> descriptor);
    std::shared_ptr<GattDescriptor> getDescriptor(const GattUuid& uuid);
    const std::vector<std::shared_ptr<GattDescriptor>>& getDescriptors() const {
        return descriptors;
    }

    // 알림 관리
    bool isNotifying() const { return notifying; }
    void sendNotification();  // 선언 추가
    void addManagedObjectProperties(GVariantBuilder* builder);  // 선언 추가

    // D-Bus 프로퍼티 헬퍼 메서드
    void addDBusProperty(const char* name, 
                        const char* type, 
                        bool readable, 
                        bool writable,
                        GVariant* (*getter)(void*),
                        void (*setter)(GVariant*, void*));

    // D-Bus 메서드 헬퍼
    void addDBusMethod(const char* name,
                      const char** inArgs,
                      const char* outArgs,
                      DBusMethod::Callback callback);

protected:
    virtual void onValueChanged(const std::vector<uint8_t>& newValue);  // 선언 추가
    virtual void onNotifyingChanged(bool isNotifying);  // 선언 추가

    // GattService의 getPath 메서드 필요
    virtual DBusObjectPath getPath() const;  // 선언 추          


private:
    GattUuid uuid;
    GattService* service;
    std::vector<uint8_t> value;
    std::bitset<GattProperty::MAX_FLAGS> properties;
    std::vector<std::shared_ptr<GattDescriptor>> descriptors;
    bool notifying;
    
    void setupProperties();
    void setupMethods();

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