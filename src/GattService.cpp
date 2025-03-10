#include "GattService.h"
#include "GattCharacteristic.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

GattService::GattService(
    DBusConnection& connection,
    const DBusObjectPath& path,
    const GattUuid& uuid,
    bool isPrimary
) : DBusObject(connection, path),
    uuid(uuid),
    primary(isPrimary) {
}

GattCharacteristicPtr GattService::createCharacteristic(
    const GattUuid& uuid,
    uint8_t properties,
    uint8_t permissions
) {
    std::string uuidStr = uuid.toString();
    
    // 이미 존재하는 경우 기존 특성 반환
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end()) {
        return it->second;
    }
    
    // 새 경로 생성
    DBusObjectPath charPath = getPath() + "/char" + std::to_string(characteristics.size() + 1);
    
    // 특성 생성
    GattCharacteristicPtr characteristic = std::make_shared<GattCharacteristic>(
        getConnection(),
        charPath,
        uuid,
        *this,
        properties,
        permissions
    );
    
    // 특성 등록
    if (!characteristic->setupDBusInterfaces()) {
        Logger::error("Failed to setup characteristic interfaces for: " + uuidStr);
        return nullptr;
    }
    
    // 맵에 추가
    characteristics[uuidStr] = characteristic;
    
    Logger::info("Created characteristic: " + uuidStr + " at path: " + charPath.toString());
    return characteristic;
}

GattCharacteristicPtr GattService::getCharacteristic(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    auto it = characteristics.find(uuidStr);
    
    if (it != characteristics.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool GattService::setupDBusInterfaces() {
    // 속성 정의
    std::vector<DBusProperty> properties = {
        {
            "UUID",
            "s",
            true,
            false,
            false,
            [this]() { return getUuidProperty(); },
            nullptr
        },
        {
            "Primary",
            "b",
            true,
            false,
            false,
            [this]() { return getPrimaryProperty(); },
            nullptr
        },
        {
            "Characteristics",
            "ao",
            true,
            false,
            true,
            [this]() { return getCharacteristicsProperty(); },
            nullptr
        }
    };
    
    // 인터페이스 추가
    if (!addInterface(SERVICE_INTERFACE, properties)) {
        Logger::error("Failed to add service interface");
        return false;
    }
    
    // 서비스는 별도의 메서드가 없음
    
    // 객체 등록
    if (!registerObject()) {
        Logger::error("Failed to register service object");
        return false;
    }
    
    Logger::info("Registered GATT service: " + uuid.toString());
    return true;
}

GVariant* GattService::getUuidProperty() {
    return Utils::gvariantFromString(uuid.toBlueZFormat());
}

GVariant* GattService::getPrimaryProperty() {
    return Utils::gvariantFromBoolean(primary);
}

GVariant* GattService::getCharacteristicsProperty() {
    std::vector<std::string> paths;
    for (const auto& pair : characteristics) {
        paths.push_back(pair.second->getPath().toString());
    }
    
    return Utils::gvariantFromStringArray(paths);
}

} // namespace ggk