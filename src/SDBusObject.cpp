#include "SDBusObject.h"

namespace ggk {

SDBusObject::SDBusObject(SDBusConnection& connection, const std::string& objectPath)
    : connection(connection),
      objectPath(objectPath),
      registered(false) {
    
    try {
        sdbusObject = connection.createObject(objectPath);
        Logger::info("Created D-Bus object at path: " + objectPath);
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create D-Bus object: " + std::string(e.what()));
        sdbusObject = nullptr;
    }
}

SDBusObject::~SDBusObject() {
    unregisterObject();
}

bool SDBusObject::registerObject() {
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (registered) {
        Logger::debug("Object already registered: " + objectPath);
        return true;
    }
    
    if (!sdbusObject) {
        Logger::error("Cannot register null object");
        return false;
    }
    
    try {
        // 모든 등록 완료 및 D-Bus에 등록
        sdbusObject->finishRegistration();
        registered = true;
        Logger::info("Registered D-Bus object: " + objectPath);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to register D-Bus object: " + std::string(e.what()));
        return false;
    }
}

bool SDBusObject::unregisterObject() {
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (!registered) {
        return true;
    }
    
    if (!sdbusObject) {
        registered = false;
        return true;
    }
    
    try {
        sdbusObject->unregister();
        registered = false;
        Logger::info("Unregistered D-Bus object: " + objectPath);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to unregister D-Bus object: " + std::string(e.what()));
        return false;
    }
}

bool SDBusObject::addInterface(const std::string& interfaceName) {
    // 인터페이스 이름을 등록 (sdbus-c++는 이를 명시적으로 필요로 하지 않음)
    // 메서드 등록 시 자동으로 인터페이스를 생성함
    return true;
}

bool SDBusObject::emitPropertyChanged(
    const std::string& interfaceName,
    const std::string& propertyName) {
    
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (!registered || !sdbusObject) {
        return false;
    }
    
    try {
        sdbusObject->emitPropertiesChangedSignal(interfaceName, {propertyName});
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to emit property changed signal: " + std::string(e.what()));
        return false;
    }
}

sdbus::IObject& SDBusObject::getSdbusObject() {
    if (!sdbusObject) {
        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Object not initialized");
    }
    
    return *sdbusObject;
}

bool SDBusObject::registerObjectManager(
    std::function<std::map<sdbus::ObjectPath, 
                 std::map<std::string, 
                 std::map<std::string, sdbus::Variant>>>()> handler) {
    
    if (!sdbusObject) return false;
    
    try {
        // 직접 sdbus-c++ API 사용
        sdbusObject->registerMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .implementedAs(std::move(handler));
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("ObjectManager 등록 실패: " + std::string(e.what()));
        return false;
    }
}

} // namespace ggk