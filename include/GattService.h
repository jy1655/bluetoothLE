#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include <vector>
#include "DBusInterface.h"
#include "GattUuid.h" 

namespace ggk {

class GattCharacteristic;

class GattService : public DBusInterface {
public:
    static const char* INTERFACE_NAME;    // "org.bluez.GattService1"

    // Primary/Secondary 서비스 구분
    enum class Type {
        PRIMARY,
        SECONDARY
    };

    // GattUuid를 직접 사용
    GattService(const GattUuid& uuid, Type type = Type::PRIMARY);
    virtual ~GattService() = default;

    // 서비스 속성
    const GattUuid& getUUID() const { return uuid; }
    Type getType() const { return type; }
    bool isPrimary() const { return type == Type::PRIMARY; }

    // Characteristic 관리
    bool addCharacteristic(std::shared_ptr<GattCharacteristic> characteristic);
    std::shared_ptr<GattCharacteristic> getCharacteristic(const GattUuid& uuid);
    const std::vector<std::shared_ptr<GattCharacteristic>>& getCharacteristics() const { 
        return characteristics; 
    }

    // D-Bus 프로퍼티 관리
    void addManagedObjectProperties(GVariantBuilder* builder);

    // D-Bus 경로 관리
    virtual DBusObjectPath getPath() const;

private:
    GattUuid uuid;  // GattUuid 사용
    Type type;
    std::vector<std::shared_ptr<GattCharacteristic>> characteristics;

    // D-Bus 프로퍼티
    static GVariant* getUUIDProperty(void* userData);
    static GVariant* getPrimaryProperty(void* userData);
};

} // namespace ggk