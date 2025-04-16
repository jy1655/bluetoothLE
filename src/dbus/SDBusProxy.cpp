#include "dbus/SDBusProxy.h"
#include "Logger.h"

namespace ggk {

SDBusProxy::SDBusProxy(
    SDBusConnection& connection,
    const std::string& destination,
    const std::string& objectPath)
    : connection(connection)
    , destination(destination)
    , objectPath(objectPath) {
    
    try {
        sdbusProxy = connection.createProxy(destination, objectPath);
        Logger::info("Created D-Bus proxy for " + destination + " at " + objectPath);
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create D-Bus proxy: " + std::string(e.what()));
        sdbusProxy = nullptr;
    }
}

SDBusProxy::~SDBusProxy() {
    // The unique_ptr will clean up
}

sdbus::Variant SDBusProxy::getProperty(
    const std::string& interfaceName,
    const std::string& propertyName) {
    
    std::lock_guard<std::mutex> lock(proxyMutex);
    
    if (!sdbusProxy) {
        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Proxy not initialized");
    }
    
    try {
        return sdbusProxy->getProperty(propertyName).onInterface(interfaceName);
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to get property: " + std::string(e.what()));
        throw;
    }
}

void SDBusProxy::unregisterSignalHandler(
    const std::string& interfaceName,
    const std::string& signalName,
    uint32_t handlerId) {
    
    std::lock_guard<std::mutex> lock(proxyMutex);
    
    if (!sdbusProxy) {
        return;
    }
    
    try {
        // Unfortunately, sdbus-c++ doesn't provide a direct way to unregister a specific handler
        // A common approach is to simply create a new proxy without the handler
        
        // For now, we'll just log that this is not implemented
        Logger::warn("Unregistering specific signal handlers is not directly supported by sdbus-c++");
    }
    catch (const sdbus::Error& e) {
        Logger::error("Error unregistering signal handler: " + std::string(e.what()));
    }
}

sdbus::IProxy& SDBusProxy::getSdbusProxy() {
    if (!sdbusProxy) {
        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Proxy not initialized");
    }
    
    return *sdbusProxy;
}

} // namespace ggk