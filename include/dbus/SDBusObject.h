#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <map>
#include "SDBusConnection.h"

namespace ggk {

/**
 * @brief Wrapper around sdbus::IObject
 *
 * Handles registration of D-Bus object interfaces, methods, signals and properties
 */
class SDBusObject {
public:
    /**
     * @brief Constructor
     * 
     * @param connection SDBusConnection to use
     * @param objectPath D-Bus object path
     */
    SDBusObject(SDBusConnection& connection, const std::string& objectPath);
    
    /**
     * @brief Destructor
     */
    virtual ~SDBusObject();
    
    /**
     * @brief Register the object with D-Bus
     * 
     * All interfaces, methods and properties must be added before calling this.
     * 
     * @return true if registration was successful
     */
    bool registerObject();
    
    /**
     * @brief Unregister the object from D-Bus
     * 
     * @return true if unregistration was successful
     */
    bool unregisterObject();
    
    /**
     * @brief Check if the object is registered
     * 
     * @return true if registered
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief Get the object path
     * 
     * @return Object path
     */
    const std::string& getPath() const { return objectPath; }
    
    /**
     * @brief Add an interface to the object
     * 
     * @param interfaceName Name of the interface
     * @return true if interface was added successfully
     */
    bool addInterface(const std::string& interfaceName);
    
    /**
     * @brief Register a method handler
     * 
     * @param interfaceName Interface name
     * @param methodName Method name
     * @param inputSignature Input parameter signature
     * @param outputSignature Output parameter signature
     * @param handler Method handler function as std::function
     * @return true if method was registered successfully
     */
    template<typename Handler>
    bool registerMethod(
        const std::string& interfaceName,
        const std::string& methodName,
        const std::string& inputSignature,
        const std::string& outputSignature,
        Handler&& handler)
    {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered) {
            return false; // Cannot add methods after registration
        }
        
        if (!sdbusObject) {
            return false;
        }
        
        try {
            // Register the method with sdbus
            sdbusObject->registerMethod(interfaceName, methodName, 
                                       inputSignature, outputSignature)
                        .onInterface(interfaceName)
                        .implementedAs(std::forward<Handler>(handler));
            return true;
        }
        catch (const sdbus::Error& e) {
            // Log error and return false
            return false;
        }
    }
    
    /**
     * @brief Register a property
     * 
     * @param interfaceName Interface name
     * @param propertyName Property name
     * @param signature Property type signature
     * @param readCallback Function to get property value
     * @param writeCallback Function to set property value (nullptr for read-only)
     * @param emitsChangedSignal Whether property emits changed signal
     * @return true if property was registered successfully
     */
    template<typename ReadCallback, typename WriteCallback = std::nullptr_t>
    bool registerProperty(
        const std::string& interfaceName,
        const std::string& propertyName,
        const std::string& signature,
        ReadCallback&& readCallback,
        WriteCallback&& writeCallback = nullptr,
        bool emitsChangedSignal = true)
    {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered) {
            return false; // Cannot add properties after registration
        }
        
        if (!sdbusObject) {
            return false;
        }
        
        try {
            auto property = sdbusObject->registerProperty(interfaceName, propertyName, signature)
                                      .onInterface(interfaceName);
            
            // Configure read access
            property.withGetter(std::forward<ReadCallback>(readCallback));
            
            // Configure write access if provided
            if constexpr (!std::is_same_v<WriteCallback, std::nullptr_t>) {
                if (writeCallback != nullptr) {
                    property.withSetter(std::forward<WriteCallback>(writeCallback));
                }
            }
            
            // Configure signal emissions
            if (emitsChangedSignal) {
                property.withUpdateBehavior(sdbus::EmitsChangedSignal);
            } else {
                property.withUpdateBehavior(sdbus::EmitsNoSignal);
            }
            
            return true;
        }
        catch (const sdbus::Error& e) {
            // Log error and return false
            return false;
        }
    }
    
    /**
     * @brief Register a signal
     * 
     * @param interfaceName Interface name
     * @param signalName Signal name
     * @param signature Signal parameter signature
     * @return true if signal was registered successfully
     */
    bool registerSignal(
        const std::string& interfaceName,
        const std::string& signalName,
        const std::string& signature);
    
    /**
     * @brief Emit a signal
     * 
     * @param interfaceName Interface name
     * @param signalName Signal name
     * @param args Signal arguments
     * @return true if signal was emitted successfully
     */
    template<typename... Args>
    bool emitSignal(
        const std::string& interfaceName,
        const std::string& signalName,
        Args&&... args)
    {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (!registered || !sdbusObject) {
            return false;
        }
        
        try {
            sdbusObject->emitSignal(interfaceName, signalName)
                       .onInterface(interfaceName)
                       .withArguments(std::forward<Args>(args)...);
            return true;
        }
        catch (const sdbus::Error& e) {
            // Log error and return false
            return false;
        }
    }
    
    /**
     * @brief Emit a property changed signal
     * 
     * @param interfaceName Interface name
     * @param propertyName Property name
     * @return true if signal was emitted successfully
     */
    bool emitPropertyChanged(
        const std::string& interfaceName,
        const std::string& propertyName);
    
    /**
     * @brief Get the underlying sdbus::IObject
     * 
     * @return Reference to sdbus::IObject
     */
    sdbus::IObject& getSdbusObject();

private:
    SDBusConnection& connection;
    std::string objectPath;
    std::unique_ptr<sdbus::IObject> sdbusObject;
    bool registered;
    mutable std::mutex objectMutex;
    std::vector<std::string> interfaces;
};

} // namespace ggk