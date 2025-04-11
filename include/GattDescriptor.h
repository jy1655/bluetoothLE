// include/GattDescriptor.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include "BlueZConstants.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

// Forward declaration to avoid circular dependency
class GattCharacteristic;

class GattDescriptor : public DBusObject, public std::enable_shared_from_this<GattDescriptor> {
public:
    // Constructor
    GattDescriptor(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        GattCharacteristic& characteristic,
        uint8_t permissions
    );
    
    virtual ~GattDescriptor() = default;
    
    // Properties
    const GattUuid& getUuid() const { return uuid; }
    
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    uint8_t getPermissions() const { return permissions; }
    
    // Value setter
    void setValue(const std::vector<uint8_t>& value);
    
    // Callbacks
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    // BlueZ D-Bus interface setup
    bool setupDBusInterfaces();

    // Get parent characteristic
    GattCharacteristic& getCharacteristic() const { return characteristic; }
    
private:
    // Properties
    GattUuid uuid;
    GattCharacteristic& characteristic;
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    
    // Callbacks
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    mutable std::mutex callbackMutex;
    
    // D-Bus method handlers
    void handleReadValue(const DBusMethodCall& call);
    void handleWriteValue(const DBusMethodCall& call);
    
    // D-Bus property getters
    GVariant* getUuidProperty();
    GVariant* getCharacteristicProperty();
    GVariant* getPermissionsProperty();
};

// Define shared pointer type
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

} // namespace ggk