// include/GattApplication.h
#pragma once

#include "DBusObject.h"
#include "GattService.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

/**
 * @brief GATT Application class that manages GATT services for BlueZ
 * 
 * This class is responsible for registering a GATT application with BlueZ,
 * managing GATT services, and handling D-Bus communication for the GATT profile.
 */
class GattApplication : public DBusObject {
public:
    /**
     * @brief Constructor
     * 
     * @param connection D-Bus connection
     * @param path Object path (default: /com/example/gatt)
     */
    GattApplication(
        DBusConnection& connection, 
        const DBusObjectPath& path = DBusObjectPath("/com/example/gatt")
    );
    
    /**
     * @brief Destructor
     */
    virtual ~GattApplication() = default;
    
    /**
     * @brief Add a GATT service
     * 
     * @param service Service to add
     * @return Success status
     */
    bool addService(GattServicePtr service);
    
    /**
     * @brief Remove a GATT service
     * 
     * @param uuid Service UUID
     * @return Success status
     */
    bool removeService(const GattUuid& uuid);
    
    /**
     * @brief Find service by UUID
     * 
     * @param uuid Service UUID
     * @return Service pointer (nullptr if not found)
     */
    GattServicePtr getService(const GattUuid& uuid) const;
    
    /**
     * @brief Register with BlueZ
     * 
     * @return Success status
     */
    bool registerWithBlueZ();
    
    /**
     * @brief Unregister from BlueZ
     * 
     * @return Success status
     */
    bool unregisterFromBlueZ();
    
    /**
     * @brief Check if registered with BlueZ
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief Get all services
     * 
     * @return Vector of services
     */
    std::vector<GattServicePtr> getServices() const;
    
    /**
     * @brief Setup D-Bus interfaces
     * 
     * @return Success status
     */
    bool setupDBusInterfaces();

    /**
     * @brief Ensure all interfaces are registered
     * 
     * @return Success status
     */
    bool ensureInterfacesRegistered();
    
private:
    /**
     * @brief Handle GetManagedObjects method call
     * 
     * @param call Method call info
     */
    void handleGetManagedObjects(const DBusMethodCall& call);
    
    /**
     * @brief Create managed objects dictionary
     * 
     * @return GVariant containing objects dictionary
     */
    GVariantPtr createManagedObjectsDict() const;
    
    /**
     * @brief Register standard services
     * 
     * @return Success status
     */
    bool registerStandardServices();
    
    /**
     * @brief Validate object hierarchy
     * 
     * @return Validation result
     */
    bool validateObjectHierarchy() const;
    
    /**
     * @brief Log object hierarchy for debugging
     */
    void logObjectHierarchy() const;
    
    // Application state
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
    bool standardServicesAdded;
};

} // namespace ggk