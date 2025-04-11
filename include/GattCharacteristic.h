// include/GattCharacteristic.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include "BlueZConstants.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// Forward declarations
class GattService;
class GattDescriptor;

// Smart pointer types
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

/**
 * @brief Class representing a GATT characteristic
 * 
 * A GATT characteristic has properties (Read, Write, Notify, etc.)
 * and can contain one or more descriptors.
 */
class GattCharacteristic : public DBusObject, public std::enable_shared_from_this<GattCharacteristic> {
public:
    /**
     * @brief Constructor
     * 
     * @param connection D-Bus connection
     * @param path Object path
     * @param uuid Characteristic UUID
     * @param service Parent service
     * @param properties Characteristic properties (GattProperty enum combination)
     * @param permissions Characteristic permissions (GattPermission enum combination)
     */
    GattCharacteristic(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        GattService& service,
        uint8_t properties,
        uint8_t permissions
    );
    
    /**
     * @brief Destructor
     */
    virtual ~GattCharacteristic() = default;
    
    /**
     * @brief Get UUID
     */
    const GattUuid& getUuid() const { return uuid; }
    
    /**
     * @brief Get current value
     */
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    /**
     * @brief Get characteristic properties
     */
    uint8_t getProperties() const { return properties; }
    
    /**
     * @brief Get characteristic permissions
     */
    uint8_t getPermissions() const { return permissions; }
    
    /**
     * @brief Set value
     * 
     * @param value New value (copied)
     */
    void setValue(const std::vector<uint8_t>& value);
    
    /**
     * @brief Set value (move semantics)
     * 
     * @param value New value (moved)
     */
    void setValue(std::vector<uint8_t>&& value);
    
    /**
     * @brief Create descriptor
     * 
     * @param uuid Descriptor UUID
     * @param permissions Descriptor permissions
     * @return Descriptor pointer (nullptr on failure)
     */
    GattDescriptorPtr createDescriptor(
        const GattUuid& uuid,
        uint8_t permissions
    );
    
    /**
     * @brief Find descriptor by UUID
     * 
     * @param uuid Descriptor UUID
     * @return Descriptor pointer (nullptr if not found)
     */
    GattDescriptorPtr getDescriptor(const GattUuid& uuid) const;
    
    /**
     * @brief Get all descriptors
     */
    const std::map<std::string, GattDescriptorPtr>& getDescriptors() const {
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        return descriptors;
    }
    
    /**
     * @brief Start notifications
     * 
     * @return Success status
     */
    bool startNotify();
    
    /**
     * @brief Stop notifications
     * 
     * @return Success status
     */
    bool stopNotify();
    
    /**
     * @brief Check if notifications are active
     */
    bool isNotifying() const {
        std::lock_guard<std::mutex> lock(notifyMutex);
        return notifying;
    }
    
    /**
     * @brief Set read callback
     */
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    /**
     * @brief Set write callback
     */
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    /**
     * @brief Set notify callback
     */
    void setNotifyCallback(GattNotifyCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        notifyCallback = callback;
    }
    
    /**
     * @brief Setup BlueZ D-Bus interfaces
     * 
     * @return Success status
     */
    bool setupDBusInterfaces();

    /**
     * @brief Get parent service
     */
    GattService& getService() const { return service; }

    /**
     * @brief Ensure CCCD descriptor exists
     * 
     * Creates CCCD descriptor if needed for characteristics with
     * notify or indicate properties.
     */
    void ensureCCCDExists();
    
private:
    // D-Bus method handlers
    void handleReadValue(const DBusMethodCall& call);
    void handleWriteValue(const DBusMethodCall& call);
    void handleStartNotify(const DBusMethodCall& call);
    void handleStopNotify(const DBusMethodCall& call);
    
    // D-Bus property getters
    GVariant* getUuidProperty();
    GVariant* getServiceProperty();
    GVariant* getPropertiesProperty();
    GVariant* getDescriptorsProperty();
    GVariant* getNotifyingProperty();
    
    // Properties
    GattUuid uuid;
    GattService& service;
    uint8_t properties;
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    
    bool notifying;
    mutable std::mutex notifyMutex;
    
    // Descriptor management
    std::map<std::string, GattDescriptorPtr> descriptors;
    mutable std::mutex descriptorsMutex;
    
    // Callbacks
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    GattNotifyCallback notifyCallback;
    mutable std::mutex callbackMutex;
};

// Smart pointer definition
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

} // namespace ggk