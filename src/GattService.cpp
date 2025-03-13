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
    if (uuid.toString().empty()) {
        Logger::error("Cannot create characteristic with empty UUID");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    // 이미 존재하는 경우 기존 특성 반환
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end()) {
        if (!it->second) {
            Logger::error("Found null characteristic entry for UUID: " + uuidStr);
            characteristics.erase(it);  // 잘못된 항목 제거
        } else {
            return it->second;
        }
    }
    
    try {
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
        
        if (!characteristic) {
            Logger::error("Failed to create characteristic for UUID: " + uuidStr);
            return nullptr;
        }
        
        // 특성 등록
        if (!characteristic->setupDBusInterfaces()) {
            Logger::error("Failed to setup characteristic interfaces for: " + uuidStr);
            return nullptr;
        }
        
        // 맵에 추가
        characteristics[uuidStr] = characteristic;
        
        Logger::info("Created characteristic: " + uuidStr + " at path: " + charPath.toString());
        return characteristic;
    } catch (const std::exception& e) {
        Logger::error("Exception during characteristic creation: " + std::string(e.what()));
        return nullptr;
    }
}

GattCharacteristicPtr GattService::getCharacteristic(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end() && it->second) {
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
    if (!addInterface(BlueZConstants::GATT_SERVICE_INTERFACE, properties)) {
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
    try {
        return Utils::gvariantFromString(uuid.toBlueZFormat());
    } catch (const std::exception& e) {
        Logger::error("Exception in getUuidProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattService::getPrimaryProperty() {
    try {
        return Utils::gvariantFromBoolean(primary);
    } catch (const std::exception& e) {
        Logger::error("Exception in getPrimaryProperty: " + std::string(e.what()));
        return nullptr;
    }
}

GVariant* GattService::getCharacteristicsProperty() {
    try {
        std::vector<std::string> paths;
        
        std::lock_guard<std::mutex> lock(characteristicsMutex);
        
        for (const auto& pair : characteristics) {
            if (pair.second) {  // nullptr 체크
                paths.push_back(pair.second->getPath().toString());
            }
        }
        
        return Utils::gvariantFromStringArray(paths);
    } catch (const std::exception& e) {
        Logger::error("Exception in getCharacteristicsProperty: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace ggk