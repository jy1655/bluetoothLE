#pragma once

#include <gio/gio.h>
#include <memory>
#include <vector>
#include "DBusInterface.h"
#include "GattUuid.h"
#include "GattCharacteristic.h"

namespace ggk {

class GattService : public DBusInterface {
public:
    static const char* INTERFACE_NAME;    // "org.bluez.GattService1"

    // Primary/Secondary 서비스 구분
    enum class Type {
        PRIMARY,
        SECONDARY
    };

    GattService(const GattUuid& uuid, const DBusObjectPath& path, Type type = Type::PRIMARY);
    virtual ~GattService() = default;

    // DBusInterface 구현
    virtual DBusObjectPath getPath() const override { return objectPath; }

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

    // D-Bus 객체 관리
    bool isRegistered() const { return registered; }
    void setRegistered(bool reg) { registered = reg; }
    std::string getUUIDString() const;
    
private:
    // 정적 멤버 변수로 현재 서비스 포인터 저장
    static GattService* currentService;

    // getter 함수들을 static 멤버 함수로 정의
    static GVariant* getUUIDProperty() {
        if (currentService) {
            return g_variant_new_string(currentService->uuid.toString128().c_str());
        }
        return nullptr;
    }

    static GVariant* getPrimaryProperty() {
        if (currentService) {
            return g_variant_new_boolean(currentService->type == Type::PRIMARY);
        }
        return nullptr;
    }

protected:
    virtual void onCharacteristicAdded(const std::shared_ptr<GattCharacteristic>& characteristic);
    virtual void onCharacteristicRemoved(const std::shared_ptr<GattCharacteristic>& characteristic);

private:
    GattUuid uuid;
    DBusObjectPath objectPath;
    Type type;
    std::vector<std::shared_ptr<GattCharacteristic>> characteristics;
    bool registered{false};

    void setupProperties();
    bool validateCharacteristic(const std::shared_ptr<GattCharacteristic>& characteristic) const;
};

} // namespace ggk