#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

const char* GattService::INTERFACE_NAME = "org.bluez.GattService1";

GattService::GattService(const GattUuid& uuid, const DBusObjectPath& path, Type type)
    : DBusInterface(INTERFACE_NAME)
    , uuid(uuid)
    , objectPath(path)
    , type(type) {
    
    setupProperties();
    Logger::debug("Created GATT service: " + uuid.toString() + 
                 (type == Type::PRIMARY ? " (Primary)" : " (Secondary)"));
}

GattService* GattService::currentService = nullptr;

void GattService::setupProperties() {
    // 현재 서비스 설정
    currentService = this;

    // 속성 추가
    addProperty("UUID", "s", true, false, &GattService::getUUIDProperty, nullptr);
    addProperty("Primary", "b", true, false, &GattService::getPrimaryProperty, nullptr);
}

bool GattService::addCharacteristic(std::shared_ptr<GattCharacteristic> characteristic) {
    if (!characteristic) {
        Logger::error("Attempted to add null characteristic to service: " + uuid.toString());
        return false;
    }

    if (!validateCharacteristic(characteristic)) {
        Logger::error("Invalid characteristic for service: " + uuid.toString());
        return false;
    }

    // UUID 중복 검사
    for (const auto& existing : characteristics) {
        if (existing->getUUID() == characteristic->getUUID()) {
            Logger::error("Characteristic with UUID " + characteristic->getUUID().toString() + 
                         " already exists in service: " + uuid.toString());
            return false;
        }
    }

    characteristics.push_back(characteristic);
    onCharacteristicAdded(characteristic);
    Logger::debug("Added characteristic " + characteristic->getUUID().toString() + 
                 " to service: " + uuid.toString());
    return true;
}

std::shared_ptr<GattCharacteristic> GattService::getCharacteristic(const GattUuid& uuid) {
    for (const auto& characteristic : characteristics) {
        if (characteristic->getUUID() == uuid) {
            return characteristic;
        }
    }
    return nullptr;
}

bool GattService::validateCharacteristic(const std::shared_ptr<GattCharacteristic>& characteristic) const {
    if (!characteristic) return false;

    // 경로 유효성 검사
    if (characteristic->getPath().toString().empty()) {
        Logger::error("Characteristic has invalid path");
        return false;
    }

    // UUID 유효성 검사
    if (characteristic->getUUID().toString().empty()) {
        Logger::error("Characteristic has invalid UUID");
        return false;
    }

    // 필수 속성 검사
    bool hasReadOrWrite = false;
    for (const auto& prop : {
        GattCharacteristic::Property::READ,
        GattCharacteristic::Property::WRITE,
        GattCharacteristic::Property::WRITE_WITHOUT_RESPONSE
    }) {
        if (characteristic->hasProperty(prop)) {
            hasReadOrWrite = true;
            break;
        }
    }

    if (!hasReadOrWrite) {
        Logger::warn("Characteristic has neither read nor write properties");
    }

    return true;
}

std::string GattService::getUUIDString() const {
    return getUUID().toString();
}

void GattService::onCharacteristicAdded(const std::shared_ptr<GattCharacteristic>& characteristic) {
    // 기본 구현은 비어있음
    // 하위 클래스에서 필요에 따라 구현 가능
}

void GattService::onCharacteristicRemoved(const std::shared_ptr<GattCharacteristic>& characteristic) {
    // 기본 구현은 비어있음
    // 하위 클래스에서 필요에 따라 구현 가능
}

} // namespace ggk