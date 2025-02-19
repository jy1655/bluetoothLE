#include "GattObject.h"
#include "Logger.h"

namespace ggk {

GattObject::GattObject(DBusObject& root)
    : rootObject(root) {
}

DBusObjectPath GattObject::createServicePath() const {
    return DBusObjectPath(generateObjectPath("service", serviceCounter));
}

DBusObjectPath GattObject::createCharacteristicPath(const DBusObjectPath& servicePath) const {
    return servicePath + generateObjectPath("char", 0);  // 각 서비스별로 카운터 관리가 필요할 수 있음
}

DBusObjectPath GattObject::createDescriptorPath(const DBusObjectPath& characteristicPath) const {
    return characteristicPath + generateObjectPath("desc", 0);  // 각 특성별로 카운터 관리가 필요할 수 있음
}

bool GattObject::registerService(const DBusObjectPath& path, std::shared_ptr<GattService> service) {
    if (!service) {
        Logger::error("Attempted to add null GATT service");
        return false;
    }

    const std::string uuid = service->getUUID().toString();
    if (services.find(uuid) != services.end()) {
        Logger::error("Service with UUID " + uuid + " already exists");
        return false;
    }

    auto& serviceObject = rootObject.addChild(path);
    serviceObject.addInterface(service);
    services[uuid] = service;
    serviceCounter++;

    Logger::debug("Added GATT service: " + uuid);
    return true;
}

void GattObject::unregisterService(const GattUuid& uuid) {
    auto it = services.find(uuid.toString());
    if (it != services.end()) {
        services.erase(it);
        // Note: DBusObject tree cleanup would be handled by the service itself
        Logger::debug("Removed GATT service: " + uuid.toString());
    }
}

std::string GattObject::generateObjectPath(const std::string& prefix, size_t index) {
    return "/" + prefix + std::to_string(index);
}

} // namespace ggk