// src/GattApplication.cpp
#include "GattApplication.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "Utils.h"
#include "BlueZConstants.h"
#include <unistd.h>

namespace ggk {

GattApplication::GattApplication(DBusConnection& connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false),
      standardServicesAdded(false) {
    
    // Check for reserved path prefixes
    if (path.toString().find("/org/bluez/") == 0) {
        Logger::warn("Path starts with /org/bluez/ which is reserved by BlueZ");
    }
    
    Logger::info("Created GATT application at path: " + path.toString());
}

bool GattApplication::setupDBusInterfaces() {
    // If already registered, just return success
    if (DBusObject::isRegistered()) {
        Logger::debug("Application object already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // 1. Register all services and their characteristics first
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (auto& service : services) {
        if (!service->isRegistered()) {
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // 2. Add ObjectManager interface - required by BlueZ
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    // 3. Add GetManagedObjects method required by BlueZ
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                 [this](const DBusMethodCall& call) { 
                     Logger::info("GetManagedObjects called by BlueZ");
                     handleGetManagedObjects(call); 
                 })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 4. Register the application object itself
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    registered = true;
    Logger::info("Registered GATT application: " + getPath().toString());
    return true;
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("Cannot add null service");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
    // Check if application is already registered with BlueZ
    if (registered) {
        Logger::error("Cannot add service to already registered application: " + uuidStr);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // Check for duplicate service
        for (const auto& existingService : services) {
            if (existingService->getUuid().toString() == uuidStr) {
                Logger::warn("Service already exists: " + uuidStr);
                return false;
            }
        }
        
        // Add service to list
        services.push_back(service);
    }
    
    Logger::info("Added service to application: " + uuidStr);
    return true;
}

bool GattApplication::removeService(const GattUuid& uuid) {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (auto it = services.begin(); it != services.end(); ++it) {
        if ((*it)->getUuid().toString() == uuidStr) {
            services.erase(it);
            Logger::info("Removed service from application: " + uuidStr);
            return true;
        }
    }
    
    Logger::warn("Service not found: " + uuidStr);
    return false;
}


// src/GattApplication.cpp (continued)
GattServicePtr GattApplication::getService(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (const auto& service : services) {
        if (service->getUuid().toString() == uuidStr) {
            return service;
        }
    }
    
    return nullptr;
}

std::vector<GattServicePtr> GattApplication::getServices() const {
    std::lock_guard<std::mutex> lock(servicesMutex);
    return services;
}

bool GattApplication::ensureInterfacesRegistered() {
    if (registered) {
        return true;
    }

    Logger::info("Ensuring all GATT objects are registered with D-Bus");
    
    // Step 1: Collect all objects that need registration
    std::vector<GattServicePtr> allServices;
    std::vector<GattCharacteristicPtr> allCharacteristics;
    std::vector<GattDescriptorPtr> allDescriptors;
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // Collect services
        for (auto& service : services) {
            allServices.push_back(service);
            
            // Collect characteristics for this service
            const auto& characteristics = service->getCharacteristics();
            for (const auto& charPair : characteristics) {
                auto characteristic = charPair.second;
                if (characteristic) {
                    // Ensure CCCD exists for characteristics with notify/indicate
                    if ((characteristic->getProperties() & GattProperty::PROP_NOTIFY) ||
                        (characteristic->getProperties() & GattProperty::PROP_INDICATE)) {
                        characteristic->ensureCCCDExists();
                    }
                    
                    allCharacteristics.push_back(characteristic);
                    
                    // Collect descriptors for this characteristic
                    const auto& descriptors = characteristic->getDescriptors();
                    for (const auto& descPair : descriptors) {
                        auto descriptor = descPair.second;
                        if (descriptor) {
                            allDescriptors.push_back(descriptor);
                        }
                    }
                }
            }
        }
    }
    
    // Step 2: Register in hierarchical order
    
    // First register services
    Logger::debug("Registering " + std::to_string(allServices.size()) + " services");
    for (auto& service : allServices) {
        if (!service->isRegistered()) {
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to register service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // Then register characteristics
    Logger::debug("Registering " + std::to_string(allCharacteristics.size()) + " characteristics");
    for (auto& characteristic : allCharacteristics) {
        if (!characteristic->isRegistered()) {
            if (!characteristic->setupDBusInterfaces()) {
                Logger::error("Failed to register characteristic: " + characteristic->getUuid().toString());
                return false;
            }
        }
    }
    
    // Finally register descriptors
    Logger::debug("Registering " + std::to_string(allDescriptors.size()) + " descriptors");
    for (auto& descriptor : allDescriptors) {
        if (!descriptor->isRegistered()) {
            if (!descriptor->setupDBusInterfaces()) {
                Logger::error("Failed to register descriptor: " + descriptor->getUuid().toString());
                return false;
            }
        }
    }
    
    // Register the application object itself
    if (!isRegistered()) {
        if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
            Logger::error("Failed to add ObjectManager interface");
            return false;
        }
        
        if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                     [this](const DBusMethodCall& call) { handleGetManagedObjects(call); })) {
            Logger::error("Failed to add GetManagedObjects method");
            return false;
        }
        
        if (!registerObject()) {
            Logger::error("Failed to register application object");
            return false;
        }
    }
    
    Logger::info("All GATT objects registered successfully with D-Bus");
    return true;
}

bool GattApplication::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT application with BlueZ");
        
        // Step 1: Ensure all objects are registered with D-Bus first
        if (!ensureInterfacesRegistered()) {
            Logger::error("Failed to register application objects with D-Bus");
            return false;
        }
        
        // Step 2: Validate object hierarchy for BlueZ compatibility
        if (!validateObjectHierarchy()) {
            Logger::warn("Object hierarchy validation issues detected");
            // Log details but continue - this is a warning, not a fatal error
            logObjectHierarchy();
        }
        
        // Step 3: Check if already registered
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // Step 4: Check BlueZ service status
        if (system("systemctl is-active --quiet bluetooth.service") != 0) {
            Logger::error("BlueZ service is not active. Attempting to restart...");
            system("sudo systemctl restart bluetooth.service");
            sleep(2);
            
            if (system("systemctl is-active --quiet bluetooth.service") != 0) {
                Logger::error("Failed to restart BlueZ service");
                return false;
            }
            Logger::info("BlueZ service restarted successfully");
        }
        
        // Step 5: Register with BlueZ using D-Bus API
        Logger::info("Sending RegisterApplication request to BlueZ");
        
        // Create options dictionary using makeGVariantPtr pattern
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&builder, "{sv}", "RegisterAll", g_variant_new_boolean(true));
        
        // Build parameters tuple
        GVariant* paramsRaw = g_variant_new("(oa{sv})", getPath().c_str(), &builder);
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        // Send request to BlueZ with proper error handling
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                std::move(params),
                "",
                10000  // 10 second timeout
            );
            
            registered = true;
            Logger::info("Successfully registered application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            Logger::error("Failed to register application with BlueZ: " + error);
            
            // Check for common errors
            if (error.find("AlreadyExists") != std::string::npos) {
                Logger::info("Application already registered, attempting to unregister first");
                unregisterFromBlueZ();
                // Retry registration
                return registerWithBlueZ();
            }
            
            return false;
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    // Skip if not registered
    if (!registered) {
        Logger::debug("Application not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        // Check D-Bus connection
        if (!getConnection().isConnected()) {
            Logger::warn("D-Bus connection not available, updating local registration state only");
            registered = false;
            return true;
        }
        
        // Create parameters using makeGVariantPtr pattern
        GVariant* paramsRaw = g_variant_new("(o)", getPath().c_str());
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        // Call BlueZ UnregisterApplication
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                std::move(params),
                "",
                5000  // 5 second timeout
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // Ignore DoesNotExist errors
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos) {
                Logger::info("Application already unregistered or not found in BlueZ");
            } else {
                Logger::warn("Failed to unregister from BlueZ: " + error);
            }
        }
        
        // Update local state
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;
        return false;
    }
}

bool GattApplication::registerStandardServices() {
    // If already added standard services, skip
    if (standardServicesAdded) {
        return true;
    }
    
    // Implementation would go here to add GAP service
    // For now, we're skipping this as it's not critical
    standardServicesAdded = true;
    return true;
}

bool GattApplication::validateObjectHierarchy() const {
    bool valid = true;
    
    // Check application
    if (!registered) {
        Logger::warn("Application object not registered");
        valid = false;
    }
    
    // Check services
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (const auto& service : services) {
        if (!service->isRegistered()) {
            Logger::warn("Service " + service->getUuid().toString() + " not registered");
            valid = false;
        }
        
        // Check characteristics
        auto characteristics = service->getCharacteristics();
        for (const auto& pair : characteristics) {
            const auto& characteristic = pair.second;
            if (!characteristic->isRegistered()) {
                Logger::error("Characteristic " + characteristic->getUuid().toString() + " not registered");
                valid = false;
            }
            
            // Check CCCD for notify/indicate characteristics
            uint8_t props = characteristic->getProperties();
            bool needsCccd = (props & GattProperty::PROP_NOTIFY) || (props & GattProperty::PROP_INDICATE);
            bool hasCccd = false;
            
            // Check descriptors
            auto descriptors = characteristic->getDescriptors();
            for (const auto& descPair : descriptors) {
                const auto& descriptor = descPair.second;
                if (!descriptor->isRegistered()) {
                    Logger::error("Descriptor " + descriptor->getUuid().toString() + " not registered");
                    valid = false;
                }
                
                // Check for CCCD
                if (descriptor->getUuid().toBlueZShortFormat() == "00002902") {
                    hasCccd = true;
                }
            }
            
            // Critical error if notify/indicate is set but no CCCD exists
            if (needsCccd && !hasCccd) {
                Logger::error("CRITICAL ERROR: Characteristic " + characteristic->getUuid().toString() + 
                            " has NOTIFY/INDICATE but no CCCD descriptor!");
                valid = false;
            }
        }
    }
    
    return valid;
}

void GattApplication::logObjectHierarchy() const {
    Logger::debug("===== Object hierarchy for application: " + getPath().toString() + " =====");
    
    // Make a copy - access under lock only briefly
    std::vector<GattServicePtr> servicesCopy;
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        servicesCopy = services;
    }
    
    Logger::debug("- Services: " + std::to_string(servicesCopy.size()));
    
    for (const auto& service : servicesCopy) {
        Logger::debug("  - Service: " + service->getUuid().toString() + 
                     " at " + service->getPath().toString() + 
                     " (Registered: " + (service->isRegistered() ? "Yes" : "No") + ")");
        
        // Copy characteristics list
        auto characteristics = service->getCharacteristics();
        Logger::debug("    - Characteristics: " + std::to_string(characteristics.size()));
        
        for (const auto& pair : characteristics) {
            const auto& characteristic = pair.second;
            
            // Display characteristic properties
            uint8_t props = characteristic->getProperties();
            std::string propStr = "";
            if (props & GattProperty::PROP_READ) propStr += "READ ";
            if (props & GattProperty::PROP_WRITE) propStr += "WRITE ";
            if (props & GattProperty::PROP_NOTIFY) propStr += "NOTIFY ";
            if (props & GattProperty::PROP_INDICATE) propStr += "INDICATE ";
            
            Logger::debug("      - Characteristic: " + characteristic->getUuid().toString() + 
                         " at " + characteristic->getPath().toString() +
                         " (Registered: " + (characteristic->isRegistered() ? "Yes" : "No") + ")" +
                         " Props: [" + propStr + "]");
            
            // Check for CCCD
            bool hasCccd = false;
            
            // Check descriptors
            auto descriptors = characteristic->getDescriptors();
            if (!descriptors.empty()) {
                Logger::debug("        - Descriptors: " + std::to_string(descriptors.size()));
                
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    Logger::debug("          - Descriptor: " + descriptor->getUuid().toString() + 
                                 " at " + descriptor->getPath().toString() +
                                 " (Registered: " + (descriptor->isRegistered() ? "Yes" : "No") + ")");
                    
                    // Check for CCCD
                    if (descriptor->getUuid().toBlueZShortFormat() == "00002902") {
                        hasCccd = true;
                    }
                }
            }
            
            // Warning if NOTIFY/INDICATE without CCCD
            if ((props & GattProperty::PROP_NOTIFY || props & GattProperty::PROP_INDICATE) && !hasCccd) {
                Logger::error("WARNING: Characteristic " + characteristic->getUuid().toString() + 
                             " has NOTIFY/INDICATE but no CCCD descriptor!");
            }
        }
    }
    
    Logger::debug("===== End of object hierarchy =====");
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    try {
        Logger::info("BlueZ requested GetManagedObjects");
        
        // Ensure all objects are registered
        ensureInterfacesRegistered();
        
        // Create managed objects dictionary
        GVariantPtr objects = createManagedObjectsDict();
        
        if (!objects) {
            Logger::error("Failed to create managed objects dictionary");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create managed objects dictionary"
            );
            return;
        }
        
        // Return the dictionary
        g_dbus_method_invocation_return_value(call.invocation.get(), objects.get());
        
    } catch (const std::exception& e) {
        Logger::error("Exception in handleGetManagedObjects: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            e.what()
        );
    }
}

GVariantPtr GattApplication::createManagedObjectsDict() const {
    try {
        Logger::info("Creating managed objects dictionary");
        
        // Build top-level dictionary: a{oa{sa{sv}}}
        // o = object path
        // s = interface name
        // sv = property name and value
        GVariantBuilder objects_builder;
        g_variant_builder_init(&objects_builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    
        // 1. Add application object with ObjectManager interface
        {
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // Empty properties for ObjectManager interface
            GVariantBuilder empty_props;
            g_variant_builder_init(&empty_props, G_VARIANT_TYPE("a{sv}"));
            
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                 &empty_props);
            
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                 getPath().c_str(),
                                 &interfaces_builder);
        }
        
        // 2. Get services
        std::vector<GattServicePtr> servicesList;
        {
            std::lock_guard<std::mutex> lock(servicesMutex);
            servicesList = services;
        }
        
        // 3. Process each service
        for (const auto& service : servicesList) {
            if (!service) continue;
            
            // Add service object
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
    
            // Add GATT service interface
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            
            // Add UUID property
            g_variant_builder_add(&props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_UUID.c_str(),
                                 g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));
            
            // Add Primary property
            g_variant_builder_add(&props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_PRIMARY.c_str(),
                                 g_variant_new_boolean(service->isPrimary()));
            
            // Add Characteristics property
            GVariantBuilder char_paths_builder;
            g_variant_builder_init(&char_paths_builder, G_VARIANT_TYPE("ao"));
            
            auto characteristics = service->getCharacteristics();
            for (const auto& pair : characteristics) {
                if (pair.second) {
                    g_variant_builder_add(&char_paths_builder, "o",
                                         pair.second->getPath().c_str());
                }
            }
            
            g_variant_builder_add(&props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_CHARACTERISTIC.c_str(),
                                 g_variant_builder_end(&char_paths_builder));
            
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                                 &props_builder);
            
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                 service->getPath().c_str(),
                                 &interfaces_builder);
            
            // 4. Process each characteristic
            for (const auto& pair : characteristics) {
                const auto& characteristic = pair.second;
                if (!characteristic) continue;
                
                GVariantBuilder char_interfaces_builder;
                g_variant_builder_init(&char_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                GVariantBuilder char_props_builder;
                g_variant_builder_init(&char_props_builder, G_VARIANT_TYPE("a{sv}"));
                
                // Add UUID property
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_UUID.c_str(),
                                     g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));
                
                // Add Service property
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_SERVICE.c_str(),
                                     g_variant_new_object_path(service->getPath().c_str()));
                
                // Add Flags property
                GVariantBuilder flags_builder;
                g_variant_builder_init(&flags_builder, G_VARIANT_TYPE("as"));
                
                uint8_t props = characteristic->getProperties();
                if (props & GattProperty::PROP_BROADCAST)
                    g_variant_builder_add(&flags_builder, "s", "broadcast");
                if (props & GattProperty::PROP_READ)
                    g_variant_builder_add(&flags_builder, "s", "read");
                if (props & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
                    g_variant_builder_add(&flags_builder, "s", "write-without-response");
                if (props & GattProperty::PROP_WRITE)
                    g_variant_builder_add(&flags_builder, "s", "write");
                if (props & GattProperty::PROP_NOTIFY)
                    g_variant_builder_add(&flags_builder, "s", "notify");
                if (props & GattProperty::PROP_INDICATE)
                    g_variant_builder_add(&flags_builder, "s", "indicate");
                if (props & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
                    g_variant_builder_add(&flags_builder, "s", "authenticated-signed-writes");
                
                // Add permission flags
                uint8_t perms = characteristic->getPermissions();
                if (perms & GattPermission::PERM_READ_ENCRYPTED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-read");
                if (perms & GattPermission::PERM_WRITE_ENCRYPTED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-write");
                if (perms & GattPermission::PERM_READ_AUTHENTICATED)
                    g_variant_builder_add(&flags_builder, "s", "auth-read");
                if (perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                    g_variant_builder_add(&flags_builder, "s", "auth-write");
                
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_FLAGS.c_str(),
                                     g_variant_builder_end(&flags_builder));
                
                // Add Descriptors property
                GVariantBuilder desc_paths_builder;
                g_variant_builder_init(&desc_paths_builder, G_VARIANT_TYPE("ao"));
                
                auto descriptors = characteristic->getDescriptors();
                for (const auto& descPair : descriptors) {
                    if (descPair.second) {
                        g_variant_builder_add(&desc_paths_builder, "o",
                                             descPair.second->getPath().c_str());
                    }
                }
                
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_DESCRIPTOR.c_str(),
                                     g_variant_builder_end(&desc_paths_builder));
                
                // Add Notifying property
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_NOTIFYING.c_str(),
                                     g_variant_new_boolean(characteristic->isNotifying()));
                
                g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                     BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                     &char_props_builder);
                
                g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                     characteristic->getPath().c_str(),
                                     &char_interfaces_builder);
                
                // 5. Process each descriptor
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    GVariantBuilder desc_interfaces_builder;
                    g_variant_builder_init(&desc_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    GVariantBuilder desc_props_builder;
                    g_variant_builder_init(&desc_props_builder, G_VARIANT_TYPE("a{sv}"));
                    
                    // Add UUID property
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_UUID.c_str(),
                                         g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));
                    
                    // Add Characteristic property
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_CHARACTERISTIC.c_str(),
                                         g_variant_new_object_path(characteristic->getPath().c_str()));
                    
                    // Add Flags property
                    GVariantBuilder desc_flags_builder;
                    g_variant_builder_init(&desc_flags_builder, G_VARIANT_TYPE("as"));
                    
                    uint8_t descPerms = descriptor->getPermissions();
                    if (descPerms & GattPermission::PERM_READ)
                        g_variant_builder_add(&desc_flags_builder, "s", "read");
                    if (descPerms & GattPermission::PERM_WRITE)
                        g_variant_builder_add(&desc_flags_builder, "s", "write");
                    if (descPerms & GattPermission::PERM_READ_ENCRYPTED)
                        g_variant_builder_add(&desc_flags_builder, "s", "encrypt-read");
                    if (descPerms & GattPermission::PERM_WRITE_ENCRYPTED)
                        g_variant_builder_add(&desc_flags_builder, "s", "encrypt-write");
                    if (descPerms & GattPermission::PERM_READ_AUTHENTICATED)
                        g_variant_builder_add(&desc_flags_builder, "s", "auth-read");
                    if (descPerms & GattPermission::PERM_WRITE_AUTHENTICATED)
                        g_variant_builder_add(&desc_flags_builder, "s", "auth-write");
                    
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_FLAGS.c_str(),
                                         g_variant_builder_end(&desc_flags_builder));
                    
                    g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                         BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                         &desc_props_builder);
                    
                    g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                         descriptor->getPath().c_str(),
                                         &desc_interfaces_builder);
                }
            }
        }
        
        // Build the result GVariant
        GVariant* result = g_variant_builder_end(&objects_builder);
        
        // Validate the result
        if (!g_variant_is_of_type(result, G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
            Logger::error("Created variant has incorrect type: " + 
                         std::string(g_variant_get_type_string(result)));
            g_variant_unref(result);
            return makeNullGVariantPtr();
        }
        
        // Check if empty
        gsize n_children = g_variant_n_children(result);
        Logger::debug("Managed objects dictionary contains " + 
                     std::to_string(n_children) + " objects");
        
        if (n_children < 1) {
            Logger::warn("Managed objects dictionary is empty!");
        }
        
        // Return the result wrapped in a smart pointer
        return makeGVariantPtr(result, true);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in createManagedObjectsDict: " + std::string(e.what()));
        return makeNullGVariantPtr();
    }
}

} // namespace ggk