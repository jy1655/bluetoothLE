// src/GattService.cpp - 순환 참조 개선
#include "GattService.h"
#include "Logger.h"

namespace ggk {

GattService::GattService(
    SDBusConnection& connection,
    const std::string& path,
    const GattUuid& uuid,
    bool isPrimary)
    : connection(connection),
      object(connection, path),
      uuid(uuid),
      primary(isPrimary) {
}

GattCharacteristicPtr GattService::createCharacteristic(
    const GattUuid& uuid,
    uint8_t properties,
    uint8_t permissions)
{
    if (uuid.toString().empty()) {
        Logger::error("빈 UUID로 특성을 생성할 수 없음");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    // 이미 존재하는지 확인
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end()) {
        if (!it->second) {
            Logger::error("UUID에 대한 null 특성 항목 발견: " + uuidStr);
            characteristics.erase(it);
        } else {
            return it->second;
        }
    }
    
    try {
        // 표준화된 객체 경로 생성
        // 형식: <service_path>/char<uuid_short>
        std::string uuidShort = "/char" + uuid.toBlueZShortFormat().substr(0, 8);
        std::string charPath = getPath() + uuidShort;
        
        // 특성 생성 - this로 자신에 대한 포인터 전달 (약한 참조)
        auto characteristic = std::make_shared<GattCharacteristic>(
            connection,
            charPath,
            uuid,
            this,  // 부모 서비스 포인터
            properties,
            permissions
        );
        
        if (!characteristic) {
            Logger::error("UUID에 대한 특성 생성 실패: " + uuidStr);
            return nullptr;
        }
        
        // 맵에 추가
        characteristics[uuidStr] = characteristic;
        
        Logger::info("특성 생성됨: " + uuidStr + ", 경로: " + charPath);
        return characteristic;
    } catch (const std::exception& e) {
        Logger::error("특성 생성 중 예외: " + std::string(e.what()));
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
    // UUID 속성 등록
    object.registerProperty(
        BlueZConstants::GATT_SERVICE_INTERFACE,
        BlueZConstants::PROPERTY_UUID,
        "s",
        [this]() -> std::string { return getUuidProperty(); }
    );
    
    // Primary 속성 등록
    object.registerProperty(
        BlueZConstants::GATT_SERVICE_INTERFACE,
        BlueZConstants::PROPERTY_PRIMARY,
        "b",
        [this]() -> bool { return getPrimaryProperty(); }
    );
    
    // Characteristics 속성 등록
    object.registerProperty(
        BlueZConstants::GATT_SERVICE_INTERFACE,
        "Characteristics",
        "ao",
        [this]() -> std::vector<sdbus::ObjectPath> { return getCharacteristicsProperty(); }
    );
    
    // 객체 등록
    return object.registerObject();
}

std::string GattService::getUuidProperty() const {
    return uuid.toBlueZFormat();
}

bool GattService::getPrimaryProperty() const {
    return primary;
}

std::vector<sdbus::ObjectPath> GattService::getCharacteristicsProperty() const {
    std::vector<sdbus::ObjectPath> paths;
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    for (const auto& pair : characteristics) {
        if (pair.second) {  // null 체크
            paths.push_back(sdbus::ObjectPath(pair.second->getPath()));
        }
    }
    
    return paths;
}

} // namespace ggk