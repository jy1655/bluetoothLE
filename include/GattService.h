#pragma once

#include "GattTypes.h"
#include "DBusObject.h"
#include "BlueZConstants.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// Forward declarations
class GattCharacteristic;

// Smart pointer type
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

/**
 * @brief Class representing a GATT service
 * 
 * A GATT service contains one or more characteristics.
 */
class GattService : public DBusObject, public std::enable_shared_from_this<GattService> {
public:
    /**
     * @brief Constructor
     * 
     * @param connection D-Bus connection
     * @param path Object path
     * @param uuid Service UUID
     * @param isPrimary Whether this is a primary service
     */
    GattService(
        std::shared_ptr<IDBusConnection> connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        bool isPrimary
    );
    
    /**
     * @brief Destructor
     */
    virtual ~GattService() = default;
    
    /**
     * @brief Get UUID
     */
    const GattUuid& getUuid() const { return uuid; }
    
    /**
     * @brief Check if service is primary
     */
    bool isPrimary() const { return primary; }
    
    /**
     * @brief Create characteristic
     * 
     * @param uuid Characteristic UUID
     * @param properties Characteristic properties
     * @param permissions Characteristic permissions
     * @return Characteristic pointer (nullptr on failure)
     */
    GattCharacteristicPtr createCharacteristic(
        const GattUuid& uuid,
        uint8_t properties,
        uint8_t permissions
    );
    
    /**
     * @brief Find characteristic by UUID
     * 
     * @param uuid Characteristic UUID
     * @return Characteristic pointer (nullptr if not found)
     */
    GattCharacteristicPtr getCharacteristic(const GattUuid& uuid) const;
    
    /**
     * @brief Get all characteristics
     */
    const std::map<std::string, GattCharacteristicPtr>& getCharacteristics() const {
        std::lock_guard<std::mutex> lock(characteristicsMutex);
        return characteristics;
    }
    
    /**
     * @brief Setup BlueZ D-Bus interfaces
     * 
     * @return Success status
     */
    bool setupDBusInterfaces();
    
private:
    // D-Bus property getters
    GVariant* getUuidProperty();
    GVariant* getPrimaryProperty();
    GVariant* getCharacteristicsProperty();
    
    // Properties
    GattUuid uuid;
    bool primary;
    
    // Characteristic management
    std::map<std::string, GattCharacteristicPtr> characteristics;
    mutable std::mutex characteristicsMutex;
};

// Smart pointer definition
using GattServicePtr = std::shared_ptr<GattService>;

} // namespace ggk