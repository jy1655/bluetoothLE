// GattApplication.cpp
#include "GattApplication.h"
#include "Logger.h"
#include "Utils.h"
#include <fstream>

namespace ggk {

GattApplication::GattApplication(DBusConnection& connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false),
      standardServicesAdded(false) {
    
    // Check for reserved path prefixes
    if (path.toString().find("/org/bluez/") == 0) {
        Logger::warn("Path starts with /org/bluez/ which is reserved by BlueZ");
    }
    
    // Log initialization
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
    
    // 5. Explicitly set registered flag to true
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
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    // Check for duplicate services
    for (const auto& existingService : services) {
        if (existingService->getUuid().toString() == uuidStr) {
            Logger::warn("Service already exists: " + uuidStr);
            return false;
        }
    }
    
    // Add service
    services.push_back(service);
    
    // If the application is already registered, we need to set up the service now
    if (registered && !service->isRegistered()) {
        if (!service->setupDBusInterfaces()) {
            Logger::error("Failed to setup interfaces for service: " + uuidStr);
            // Remove the service since setup failed
            services.pop_back();
            return false;
        }
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
    // If already registered, we're good
    if (registered) {
        Logger::debug("Application object already registered, skipping interface setup");
        return true;
    }

    // 1. Make sure all services are registered first
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (auto& service : services) {
        if (!service->isRegistered()) {
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // 2. Log the object hierarchy for debugging AFTER registering services
    logObjectHierarchy();
    
    // 3. Add standard services if needed
    if (!standardServicesAdded) {
        if (!registerStandardServices()) {
            Logger::warn("Failed to add standard services, continuing anyway");
        }
    }
    
    // 4. Add ObjectManager interface
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    // 5. Add GetManagedObjects method
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                 [this](const DBusMethodCall& call) { 
                     handleGetManagedObjects(call); 
                 })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    // 6. Register the application object itself
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    // 7. Set the registered flag
    registered = true;
    
    // 8. Only validate after everything is registered
    if (!validateObjectHierarchy()) {
        Logger::warn("Object hierarchy validation failed, registration may fail");
    }
    
    Logger::info("All D-Bus interfaces registered for GATT application");
    return true;
}

bool GattApplication::registerStandardServices() {
    // If we've already added standard services, skip
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
            
            // Check descriptors
            auto descriptors = characteristic->getDescriptors();
            for (const auto& descPair : descriptors) {
                const auto& descriptor = descPair.second;
                if (!descriptor->isRegistered()) {
                    Logger::error("Descriptor " + descriptor->getUuid().toString() + " not registered");
                    valid = false;
                }
            }
        }
    }
    
    return valid;
}

void GattApplication::logObjectHierarchy() const {
    Logger::debug("Object hierarchy for application: " + getPath().toString());
    
    // 복사본 만들기 - 락 안에서만 짧게 접근
    std::vector<GattServicePtr> servicesCopy;
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        servicesCopy = services;
    }
    
    Logger::debug("- Services: " + std::to_string(servicesCopy.size()));
    
    for (const auto& service : servicesCopy) {
        Logger::debug("  - Service: " + service->getUuid().toString() + " at " + service->getPath().toString());
        
        // 특성 목록 복사 - 락 없이 접근
        auto characteristics = service->getCharacteristics();  // 이 함수도 내부적으로 복사본을 반환하도록 수정 필요
        Logger::debug("    - Characteristics: " + std::to_string(characteristics.size()));
        
        for (const auto& pair : characteristics) {
            const auto& characteristic = pair.second;
            Logger::debug("      - Characteristic: " + characteristic->getUuid().toString() + 
                         " at " + characteristic->getPath().toString());
            
            // 설명자 목록 복사 - 락 없이 접근
            auto descriptors = characteristic->getDescriptors();  // 이 함수도 내부적으로 복사본을 반환하도록 수정 필요
            if (!descriptors.empty()) {
                Logger::debug("        - Descriptors: " + std::to_string(descriptors.size()));
                
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    Logger::debug("          - Descriptor: " + descriptor->getUuid().toString() + 
                                 " at " + descriptor->getPath().toString());
                }
            }
        }
    }
}

bool GattApplication::registerWithBlueZ() {
    try {
        // 2. Check if already registered
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 3. Prepare for registration - unregister if already registered
        if (!registered) {
            if (!ensureInterfacesRegistered()) {
                Logger::error("Failed to register application objects with D-Bus");
                return false;
            }
            
            // Double-check application registration succeeded
            if (!registered) {
                Logger::error("Application registration with D-Bus failed");
                return false;
            }
        }
        
        // 5. Check BlueZ service status
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Run: systemctl start bluetooth.service");
            return false;
        }
        
        // 6. Check BlueZ adapter availability
        if (system("hciconfig hci0 > /dev/null 2>&1") != 0) {
            Logger::error("BlueZ adapter 'hci0' not found or not available");
            return false;
        }
        
        Logger::info("Sending RegisterApplication request to BlueZ");
        
        // 7. Build registration options
        GVariantBuilder options_builder;
        g_variant_builder_init(&options_builder, G_VARIANT_TYPE("a{sv}"));
        
        // Add Experimental flag (required for some BlueZ features)
        g_variant_builder_add(&options_builder, "{sv}", "Experimental", 
                             g_variant_new_boolean(TRUE));

        // Add UseObjectPaths flag (ensures all objects are accessible)
        g_variant_builder_add(&options_builder, "{sv}", "UseObjectPaths", 
                             g_variant_new_boolean(TRUE));
        
        // 8. Build registration parameters with proper reference management
        GVariant* params_raw = g_variant_new("(o@a{sv})", 
                                            getPath().c_str(), 
                                            g_variant_builder_end(&options_builder));
        GVariant* params_sinked = g_variant_ref_sink(params_raw);
        GVariantPtr parameters(params_sinked, &g_variant_unref);
        
        // 9. Log registration parameters for debugging
        char* debug_str = g_variant_print(parameters.get(), TRUE);
        Logger::debug("RegisterApplication parameters: " + std::string(debug_str));
        g_free(debug_str);
        
        // 10. Call BlueZ RegisterApplication method
        try {
            GVariantPtr result = getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                std::move(parameters),
                "",
                30000  // 30 second timeout
            );
            
            // Check result
            if (!result) {
                Logger::error("Failed to register application with BlueZ - null result");
                system("hciconfig -a"); // Log adapter state for debugging
                return false;
            }
            
            registered = true;
            Logger::info("Successfully registered application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            
            // Handle timeout specially - BlueZ 5.64 sometimes doesn't respond but works
            if (error.find("Timeout") != std::string::npos) {
                Logger::warn("BlueZ registration timed out - this may be normal with BlueZ 5.64");
                registered = true;
                return true;
            } 
            // Handle "No object received" error - common with object hierarchy issues
            else if (error.find("Failed") != std::string::npos && 
                     error.find("No object received") != std::string::npos) {
                Logger::error("BlueZ registration failed: No object received - check GATT object structure");
                
                // Dump object hierarchy to log for debugging
                logObjectHierarchy();
                
                return false;
            }
            
            Logger::error("Exception in BlueZ registration: " + error);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Unknown exception in registerWithBlueZ");
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
        
        // Create parameters
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        GVariantPtr parameters(g_variant_ref_sink(params), &g_variant_unref);
        
        // Call BlueZ UnregisterApplication
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                std::move(parameters),
                "",
                5000  // 5 second timeout
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // Log error but continue
            Logger::warn("Failed to unregister from BlueZ (continuing): " + std::string(e.what()));
        }
        
        // Update local state
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;
        return false;
    } catch (...) {
        Logger::error("Unknown exception in unregisterFromBlueZ");
        registered = false;
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    try {
        Logger::info("GetManagedObjects called by BlueZ");
        
        // Create managed objects dictionary
        GVariantPtr result = createManagedObjectsDict();
        
        if (!result) {
            Logger::error("Failed to create managed objects dictionary");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create response"
            );
            return;
        }
        
        // Log dictionary preview for debugging
        char* debug_str = g_variant_print(result.get(), TRUE);
        Logger::debug("Returning managed objects: " + std::string(debug_str).substr(0, 500) + "...");
        g_free(debug_str);
        
        // Return the result
        g_dbus_method_invocation_return_value(call.invocation.get(), result.get());
        
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
    Logger::info("Creating managed objects dictionary");
    
    // Build the top-level dictionary: a{oa{sa{sv}}}
    // o = object path
    // s = interface name
    // sv = property name and value
    GVariantBuilder objects_builder;
    g_variant_builder_init(&objects_builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    // 1. First add the application object itself with ObjectManager interface
    GVariantBuilder app_interfaces_builder;
    g_variant_builder_init(&app_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
    
    // Empty properties for ObjectManager interface
    GVariantBuilder empty_props;
    g_variant_builder_init(&empty_props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&app_interfaces_builder, "{sa{sv}}",
                         BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                         &empty_props);
    
    // Add application to objects dictionary
    g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                         getPath().c_str(),
                         &app_interfaces_builder);

    // 2. Get all services
    std::vector<GattServicePtr> servicesList;
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        servicesList = services;
    }

    Logger::debug("Processing " + std::to_string(servicesList.size()) + " services");

    // 3. Process each service
    for (const auto& service : servicesList) {
        if (!service) continue;
        
        // Create interface map for this service
        GVariantBuilder interfaces_builder;
        g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

        // Create properties map for service interface
        GVariantBuilder service_props_builder;
        g_variant_builder_init(&service_props_builder, G_VARIANT_TYPE("a{sv}"));

        // Add UUID property
        g_variant_builder_add(&service_props_builder, "{sv}",
                             BlueZConstants::PROPERTY_UUID.c_str(),
                             g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));

        // Add Primary property
        g_variant_builder_add(&service_props_builder, "{sv}",
                             BlueZConstants::PROPERTY_PRIMARY.c_str(),
                             g_variant_new_boolean(service->isPrimary()));

        // Add Characteristics property (array of object paths)
        GVariantBuilder char_paths_builder;
        g_variant_builder_init(&char_paths_builder, G_VARIANT_TYPE("ao"));

        // Get all characteristics for this service
        auto characteristics = service->getCharacteristics();
        for (const auto& pair : characteristics) {
            if (pair.second) {
                g_variant_builder_add(&char_paths_builder, "o", 
                                     pair.second->getPath().c_str());
            }
        }

        // Add characteristics array to service properties
        g_variant_builder_add(&service_props_builder, "{sv}",
                             BlueZConstants::PROPERTY_CHARACTERISTIC.c_str(),
                             g_variant_builder_end(&char_paths_builder));

        // Add service interface to interfaces map
        g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                             BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                             &service_props_builder);

        // Add service object to objects dictionary
        g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                             service->getPath().c_str(),
                             &interfaces_builder);

        // 4. Process each characteristic for this service
        for (const auto& char_pair : characteristics) {
            const auto& characteristic = char_pair.second;
            if (!characteristic) continue;

            // Create interface map for this characteristic
            GVariantBuilder char_interfaces_builder;
            g_variant_builder_init(&char_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

            // Create properties map for characteristic interface
            GVariantBuilder char_props_builder;
            g_variant_builder_init(&char_props_builder, G_VARIANT_TYPE("a{sv}"));

            // Add UUID property
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_UUID.c_str(),
                                 g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));

            // Add Service property (object path to parent service)
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_SERVICE.c_str(),
                                 g_variant_new_object_path(service->getPath().c_str()));

            // Add Flags property (array of strings)
            GVariantBuilder flags_builder;
            g_variant_builder_init(&flags_builder, G_VARIANT_TYPE("as"));

            // Add flags based on properties
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

            // Add flags based on permissions
            uint8_t perms = characteristic->getPermissions();
            if (perms & GattPermission::PERM_READ_ENCRYPTED)
                g_variant_builder_add(&flags_builder, "s", "encrypt-read");
            if (perms & GattPermission::PERM_WRITE_ENCRYPTED)
                g_variant_builder_add(&flags_builder, "s", "encrypt-write");
            if (perms & GattPermission::PERM_READ_AUTHENTICATED)
                g_variant_builder_add(&flags_builder, "s", "auth-read");
            if (perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                g_variant_builder_add(&flags_builder, "s", "auth-write");

            // Add flags to characteristic properties
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_FLAGS.c_str(),
                                 g_variant_builder_end(&flags_builder));

            // Add Descriptors property (array of object paths)
            GVariantBuilder desc_paths_builder;
            g_variant_builder_init(&desc_paths_builder, G_VARIANT_TYPE("ao"));

            // Get all descriptors for this characteristic
            auto descriptors = characteristic->getDescriptors();
            for (const auto& desc_pair : descriptors) {
                if (desc_pair.second) {
                    g_variant_builder_add(&desc_paths_builder, "o", 
                                         desc_pair.second->getPath().c_str());
                }
            }

            // Add descriptors array to characteristic properties
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_DESCRIPTOR.c_str(),
                                 g_variant_builder_end(&desc_paths_builder));

            // Add Notifying property
            g_variant_builder_add(&char_props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_NOTIFYING.c_str(),
                                 g_variant_new_boolean(characteristic->isNotifying()));

            // Add Value property (byte array) - if initially available
            std::vector<uint8_t> charValue = characteristic->getValue();
            if (!charValue.empty()) {
                GVariant* valueVariant = g_variant_new_fixed_array(
                    G_VARIANT_TYPE_BYTE,
                    charValue.data(),
                    charValue.size(),
                    sizeof(uint8_t)
                );
                g_variant_builder_add(&char_props_builder, "{sv}",
                                    BlueZConstants::PROPERTY_VALUE.c_str(),
                                    valueVariant);
            }

            // Add characteristic interface to interfaces map
            g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                 &char_props_builder);

            // Add characteristic object to objects dictionary
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                                 characteristic->getPath().c_str(),
                                 &char_interfaces_builder);

            // 5. Process each descriptor for this characteristic
            for (const auto& desc_pair : descriptors) {
                const auto& descriptor = desc_pair.second;
                if (!descriptor) continue;

                // Create interface map for this descriptor
                GVariantBuilder desc_interfaces_builder;
                g_variant_builder_init(&desc_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));

                // Create properties map for descriptor interface
                GVariantBuilder desc_props_builder;
                g_variant_builder_init(&desc_props_builder, G_VARIANT_TYPE("a{sv}"));

                // Add UUID property
                g_variant_builder_add(&desc_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_UUID.c_str(),
                                     g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));

                // Add Characteristic property (object path to parent characteristic)
                g_variant_builder_add(&desc_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_CHARACTERISTIC.c_str(),
                                     g_variant_new_object_path(characteristic->getPath().c_str()));

                // Add Flags property (array of strings)
                GVariantBuilder desc_flags_builder;
                g_variant_builder_init(&desc_flags_builder, G_VARIANT_TYPE("as"));

                // Add flags based on descriptor permissions
                uint8_t desc_perms = descriptor->getPermissions();
                if (desc_perms & GattPermission::PERM_READ)
                    g_variant_builder_add(&desc_flags_builder, "s", "read");
                if (desc_perms & GattPermission::PERM_WRITE)
                    g_variant_builder_add(&desc_flags_builder, "s", "write");
                if (desc_perms & GattPermission::PERM_READ_ENCRYPTED)
                    g_variant_builder_add(&desc_flags_builder, "s", "encrypt-read");
                if (desc_perms & GattPermission::PERM_WRITE_ENCRYPTED)
                    g_variant_builder_add(&desc_flags_builder, "s", "encrypt-write");
                if (desc_perms & GattPermission::PERM_READ_AUTHENTICATED)
                    g_variant_builder_add(&desc_flags_builder, "s", "auth-read");
                if (desc_perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                    g_variant_builder_add(&desc_flags_builder, "s", "auth-write");

                // Add flags to descriptor properties
                g_variant_builder_add(&desc_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_FLAGS.c_str(),
                                     g_variant_builder_end(&desc_flags_builder));

                // Add Value property (byte array) - if initially available
                std::vector<uint8_t> descValue = descriptor->getValue();
                if (!descValue.empty()) {
                    GVariant* valueVariant = g_variant_new_fixed_array(
                        G_VARIANT_TYPE_BYTE,
                        descValue.data(),
                        descValue.size(),
                        sizeof(uint8_t)
                    );
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                        BlueZConstants::PROPERTY_VALUE.c_str(),
                                        valueVariant);
                }

                // Add descriptor interface to interfaces map
                g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                     BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                     &desc_props_builder);

                // Add descriptor object to objects dictionary
                g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}", 
                                     descriptor->getPath().c_str(),
                                     &desc_interfaces_builder);
            }
        }
    }

    // Complete the dictionary
    GVariant* result = g_variant_builder_end(&objects_builder);
    
    // Validate and log result
    if (result) {
        Logger::info("Successfully created managed objects dictionary");
        
        // Validate type
        if (!g_variant_is_of_type(result, G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
            Logger::error("Created variant has incorrect type: " + 
                         std::string(g_variant_get_type_string(result)));
        }
        
        // Log size
        gsize n_children = g_variant_n_children(result);
        Logger::debug("Managed objects dictionary contains " + 
                     std::to_string(n_children) + " objects");
        
        if (n_children < 1) {
            Logger::warn("Managed objects dictionary is empty!");
        }
        
        // Save to file for debugging (optional)
        char* debug_str = g_variant_print(result, TRUE);
        if (debug_str) {
            // Uncomment to save to file if needed
            // std::ofstream out("/tmp/managed_objects.txt");
            // out << debug_str;
            // out.close();
            g_free(debug_str);
        }
    } else {
        Logger::error("Failed to create managed objects dictionary - null result");
    }
    
    // Wrap in smart pointer for automatic memory management
    return GVariantPtr(result, &g_variant_unref);
}

} // namespace ggk