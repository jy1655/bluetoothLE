// GattApplication.h
#pragma once

#include "DBusObject.h"
#include "GattService.h"
#include "GattCharacteristic.h" 
#include "GattDescriptor.h"
#include "BlueZConstants.h"
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
     * @param connection D-Bus connection to use
     * @param path Object path for the application (default: /com/example/gatt)
     */
    GattApplication(DBusConnection& connection, 
                   const DBusObjectPath& path = DBusObjectPath("/com/example/gatt"));
    
    /**
     * @brief Destructor
     */
    virtual ~GattApplication() = default;
    
    /**
     * @brief Add a GATT service to the application
     * 
     * @param service Shared pointer to the service to add
     * @return true if the service was added successfully, false otherwise
     */
    bool addService(GattServicePtr service);
    
    /**
     * @brief Remove a GATT service from the application
     * 
     * @param uuid UUID of the service to remove
     * @return true if the service was removed successfully, false otherwise
     */
    bool removeService(const GattUuid& uuid);
    
    /**
     * @brief Get a service by UUID
     * 
     * @param uuid UUID of the service to get
     * @return Shared pointer to the service, or nullptr if not found
     */
    GattServicePtr getService(const GattUuid& uuid) const;
    
    /**
     * @brief Register the application with BlueZ
     * 
     * This registers the application and all its services, characteristics,
     * and descriptors with BlueZ.
     * 
     * @return true if the registration was successful, false otherwise
     */
    bool registerWithBlueZ();
    
    /**
     * @brief Unregister the application from BlueZ
     * 
     * @return true if the unregistration was successful, false otherwise
     */
    bool unregisterFromBlueZ();
    
    /**
     * @brief Check if the application is registered with BlueZ
     * 
     * @return true if the application is registered, false otherwise
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief Get all services
     * 
     * @return Vector of shared pointers to services
     */
    std::vector<GattServicePtr> getServices() const;
    
    /**
     * @brief Set up D-Bus interfaces for the application
     * 
     * @return true if successful, false otherwise
     */
    bool setupDBusInterfaces();

//private:
    /**
     * @brief Ensure all services, characteristics, and descriptors are registered
     * 
     * @return true if successful, false otherwise
     */
    bool ensureInterfacesRegistered();
    
    /**
     * @brief Handle GetManagedObjects method call from BlueZ
     * 
     * This method is called by BlueZ during registration to get all GATT objects.
     * 
     * @param call D-Bus method call information
     */
    void handleGetManagedObjects(const DBusMethodCall& call);
    
    /**
     * @brief Create a dictionary of all managed objects for BlueZ
     * 
     * This creates a D-Bus dictionary containing all services, characteristics,
     * and descriptors with their properties, following the structure expected by BlueZ.
     * 
     * @return GVariantPtr containing the object dictionary
     */
    GVariantPtr createManagedObjectsDict() const;
    
    /**
     * @brief Register standard services (GAP, Device Info)
     * 
     * @return true if successful, false otherwise
     */
    bool registerStandardServices();
    
    /**
     * @brief Validate the object hierarchy
     * 
     * @return true if valid, false otherwise
     */
    bool validateObjectHierarchy() const;
    
    /**
     * @brief Log detailed debug information about all objects
     */
    void logObjectHierarchy() const;
    
    // Application state
    std::vector<GattServicePtr> services;
    mutable std::mutex servicesMutex;
    bool registered;
    bool standardServicesAdded;
};

} // namespace ggk