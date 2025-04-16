#include "dbus/SDBusObject.h"
#include "Logger.h"

namespace ggk {

SDBusObject::SDBusObject(SDBusConnection& connection, const std::string& objectPath)
    : connection(connection)
    , objectPath(objectPath)
    , registered(false) {
    
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
        // Finalize all registrations and register with D-Bus
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
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (registered) {
        Logger::warn("Cannot add interface to already registered object: " + objectPath);
        return false;
    }
    
    if (!sdbusObject) {
        return false;
    }
    
    // Check if interface already added
    for (const auto& existingInterface : interfaces) {
        if (existingInterface == interfaceName) {
            return true; // Interface already added
        }
    }
    
    interfaces.push_back(interfaceName);
    Logger::debug("Added interface: " + interfaceName + " to object: " + objectPath);
    return true;
}

bool SDBusObject::registerSignal(
    const std::string& interfaceName,
    const std::string& signalName,
    const std::string& signature) {
    
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (registered) {
        Logger::warn("Cannot register signal on already registered object: " + objectPath);
        return false;
    }
    
    if (!sdbusObject) {
        return false;
    }
    
    try {
        // Register the signal with sdbus
        sdbusObject->registerSignal(interfaceName, signalName, signature);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to register signal: " + std::string(e.what()));
        return false;
    }
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

} // namespace ggk