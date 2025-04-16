#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <variant>
#include <vector>
#include "SDBusConnection.h"

namespace ggk {

/**
 * @brief Wrapper around sdbus::IProxy
 *
 * Provides methods to call remote D-Bus methods and receive signals
 */
class SDBusProxy {
public:
    /**
     * @brief Constructor
     * 
     * @param connection SDBusConnection to use
     * @param destination Service name
     * @param objectPath Object path
     */
    SDBusProxy(SDBusConnection& connection, 
               const std::string& destination, 
               const std::string& objectPath);
    
    /**
     * @brief Destructor
     */
    virtual ~SDBusProxy();
    
    /**
     * @brief Call a remote method
     * 
     * @param interfaceName Interface name
     * @param methodName Method name
     * @param args Method arguments
     * @return Result of the method call
     */
    template<typename... Args>
    sdbus::Variant callMethod(
        const std::string& interfaceName,
        const std::string& methodName,
        Args&&... args)
    {
        std::lock_guard<std::mutex> lock(proxyMutex);
        
        if (!sdbusProxy) {
            throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Proxy not initialized");
        }
        
        try {
            auto methodCall = sdbusProxy->createMethodCall(interfaceName, methodName);
            
            if constexpr (sizeof...(args) > 0) {
                methodCall.appendArguments(std::forward<Args>(args)...);
            }
            
            auto reply = methodCall.send();
            
            // Get the result as a Variant
            sdbus::Variant result;
            if (!reply.isEmpty()) {
                result = reply.readVariant();
            }
            
            return result;
        }
        catch (const sdbus::Error& e) {
            // Re-throw the error
            throw;
        }
    }
    
    /**
     * @brief Get a property value
     * 
     * @param interfaceName Interface name
     * @param propertyName Property name
     * @return Property value as a Variant
     */
    sdbus::Variant getProperty(
        const std::string& interfaceName,
        const std::string& propertyName);
    
    /**
     * @brief Set a property value
     * 
     * @param interfaceName Interface name
     * @param propertyName Property name
     * @param value New property value
     */
    template<typename T>
    void setProperty(
        const std::string& interfaceName,
        const std::string& propertyName,
        const T& value)
    {
        std::lock_guard<std::mutex> lock(proxyMutex);
        
        if (!sdbusProxy) {
            throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Proxy not initialized");
        }
        
        try {
            sdbusProxy->setProperty(interfaceName, propertyName, value);
        }
        catch (const sdbus::Error& e) {
            // Re-throw the error
            throw;
        }
    }
    
    /**
     * @brief Register a signal handler
     * 
     * @param interfaceName Interface name
     * @param signalName Signal name
     * @param handler Signal handler function
     * @return Registration ID (for unregistering)
     */
    template<typename Handler>
    uint32_t registerSignalHandler(
        const std::string& interfaceName,
        const std::string& signalName,
        Handler&& handler)
    {
        std::lock_guard<std::mutex> lock(proxyMutex);
        
        if (!sdbusProxy) {
            throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Proxy not initialized");
        }
        
        try {
            static uint32_t handlerId = 1;
            uint32_t currentId = handlerId++;
            
            sdbusProxy->registerSignalHandler(interfaceName, signalName, 
                                           std::forward<Handler>(handler));
            
            return currentId;
        }
        catch (const sdbus::Error& e) {
            // Re-throw the error
            throw;
        }
    }
    
    /**
     * @brief Unregister a signal handler
     * 
     * @param interfaceName Interface name
     * @param signalName Signal name
     * @param handlerId Handler ID from registerSignalHandler
     */
    void unregisterSignalHandler(
        const std::string& interfaceName,
        const std::string& signalName,
        uint32_t handlerId);
    
    /**
     * @brief Get the destination service name
     * 
     * @return Destination name
     */
    const std::string& getDestination() const { return destination; }
    
    /**
     * @brief Get the object path
     * 
     * @return Object path
     */
    const std::string& getPath() const { return objectPath; }
    
    /**
     * @brief Get the underlying sdbus::IProxy
     * 
     * @return Reference to sdbus::IProxy
     */
    sdbus::IProxy& getSdbusProxy();

private:
    SDBusConnection& connection;
    std::string destination;
    std::string objectPath;
    std::unique_ptr<sdbus::IProxy> sdbusProxy;
    mutable std::mutex proxyMutex;
};

} // namespace ggk