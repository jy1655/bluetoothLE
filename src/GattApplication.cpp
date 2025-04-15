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
    
    Logger::info("Created GATT application at path: " + path.toString());
}

bool GattApplication::setupDBusInterfaces() {
    // If already registered, return success
    if (isRegistered()) {
        Logger::debug("Application object already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // Add ObjectManager interface and method
    addMethod("org.freedesktop.DBus.ObjectManager", "GetManagedObjects", 
              [this](const DBusMethodCall& call) { this->handleGetManagedObjects(call); });
    
    // Register object with D-Bus
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    registered = true;
    return true;
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("Cannot add null service");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
    // If already registered with BlueZ, cannot add more services
    if (registered) {
        Logger::error("Cannot add service to already registered application: " + uuidStr);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // Check for duplicates
        for (const auto& existingService : services) {
            if (existingService->getUuid().toString() == uuidStr) {
                Logger::warn("Service already exists: " + uuidStr);
                return false;
            }
        }
        
        // Add to list
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
    Logger::info("Ensuring all GATT interfaces are properly registered");
    
    // 1. Register the application object first (with ObjectManager interface)
    if (!isRegistered()) {
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to register application object with D-Bus");
            return false;
        }
    }
    
    // 2. Collect objects to register
    bool allSuccessful = true;
    std::vector<GattServicePtr> servicesToRegister;
    std::map<GattServicePtr, std::vector<GattCharacteristicPtr>> characteristicsToRegister;
    std::map<GattCharacteristicPtr, std::vector<GattDescriptorPtr>> descriptorsToRegister;
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        for (auto& service : services) {
            if (!service->isRegistered()) {
                servicesToRegister.push_back(service);
            }
            
            auto characteristics = service->getCharacteristics();
            std::vector<GattCharacteristicPtr> serviceCharacteristics;
            
            for (const auto& charPair : characteristics) {
                auto& characteristic = charPair.second;
                if (characteristic && !characteristic->isRegistered()) {
                    serviceCharacteristics.push_back(characteristic);
                    
                    auto descriptors = characteristic->getDescriptors();
                    std::vector<GattDescriptorPtr> charDescriptors;
                    
                    for (const auto& descPair : descriptors) {
                        auto& descriptor = descPair.second;
                        if (descriptor && !descriptor->isRegistered()) {
                            charDescriptors.push_back(descriptor);
                        }
                    }
                    
                    if (!charDescriptors.empty()) {
                        descriptorsToRegister[characteristic] = charDescriptors;
                    }
                }
            }
            
            if (!serviceCharacteristics.empty()) {
                characteristicsToRegister[service] = serviceCharacteristics;
            }
        }
    }
    
    // 3. Register in hierarchy (services -> characteristics -> descriptors)
    
    // 3.1 Register services
    for (auto& service : servicesToRegister) {
        if (!service->setupDBusInterfaces()) {
            Logger::error("Failed to register service: " + service->getUuid().toString());
            allSuccessful = false;
        }
    }
    
    // Allow D-Bus to process
    if (!servicesToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3.2 Register characteristics
    for (const auto& servicePair : characteristicsToRegister) {
        auto service = servicePair.first;
        for (auto& characteristic : servicePair.second) {
            if (!characteristic->setupDBusInterfaces()) {
                Logger::error("Failed to register characteristic: " + 
                             characteristic->getUuid().toString() + 
                             " for service: " + service->getUuid().toString());
                allSuccessful = false;
            }
        }
    }
    
    // Allow D-Bus to process
    if (!characteristicsToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3.3 Register descriptors
    for (const auto& charPair : descriptorsToRegister) {
        auto characteristic = charPair.first;
        for (auto& descriptor : charPair.second) {
            if (!descriptor->setupDBusInterfaces()) {
                Logger::error("Failed to register descriptor: " + 
                             descriptor->getUuid().toString() + 
                             " for characteristic: " + characteristic->getUuid().toString());
                allSuccessful = false;
            }
        }
    }
    
    // Count registered objects
    int totalChars = 0;
    for (const auto& pair : characteristicsToRegister) {
        totalChars += pair.second.size();
    }
    
    int totalDescs = 0;
    for (const auto& pair : descriptorsToRegister) {
        totalDescs += pair.second.size();
    }
    
    // Log registration statistics
    Logger::info("Interface registration complete: " +
                std::to_string(servicesToRegister.size()) + " services, " +
                std::to_string(totalChars) + " characteristics, " +
                std::to_string(totalDescs) + " descriptors" +
                ", success: " + (allSuccessful ? "true" : "false"));
    
    return allSuccessful;
}

bool GattApplication::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT application with BlueZ");
        
        // 1. Make sure application is registered with D-Bus
        if (!isRegistered()) {
            if (!setupDBusInterfaces()) {
                Logger::error("Failed to register application D-Bus interfaces");
                return false;
            }
        }

        // 2. Ensure all child objects are registered
        if (!ensureInterfacesRegistered()) {
            Logger::error("Failed to register GATT objects with D-Bus");
            return false;
        }
        
        // 3. Check if already registered
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 4. Create options dictionary
        GVariant* emptyDict = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);
        GVariant* params = g_variant_new("(o@a{sv})", 
                                       getPath().c_str(), 
                                       emptyDict);
        
        // Ensure ownership
        g_variant_ref_sink(params);
        
        // 5. Call BlueZ (with retry)
        try {
            // Register application
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                makeGVariantPtr(params, true),
                "",
                10000  // 10 second timeout
            );
            
            registered = true;
            Logger::info("Successfully registered GATT application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            
            // "AlreadyExists" error is success
            if (error.find("AlreadyExists") != std::string::npos) {
                Logger::info("Application already registered with BlueZ");
                registered = true;
                return true;
            }
            
            // Try unregistering first
            try {
                unregisterFromBlueZ();
                
                // Wait a moment
                usleep(500000); // 500ms
                
                // Create new parameters
                GVariant* emptyDict2 = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);
                GVariant* params2 = g_variant_new("(o@a{sv})", 
                                               getPath().c_str(), 
                                               emptyDict2);
                g_variant_ref_sink(params2);
                
                // Try registering again
                getConnection().callMethod(
                    BlueZConstants::BLUEZ_SERVICE,
                    DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                    BlueZConstants::GATT_MANAGER_INTERFACE,
                    BlueZConstants::REGISTER_APPLICATION,
                    makeGVariantPtr(params2, true),
                    "",
                    10000  // 10 second timeout
                );
                
                registered = true;
                Logger::info("Successfully registered GATT application with BlueZ on retry");
                return true;
            } catch (const std::exception& retryEx) {
                Logger::error("Failed to register with BlueZ on retry: " + std::string(retryEx.what()));
                return false;
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    // If not registered, nothing to do
    if (!registered) {
        Logger::debug("Application not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        // Create parameters
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        g_variant_ref_sink(params);
        
        // Call BlueZ
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                makeGVariantPtr(params, true),
                "",
                5000  // 5 second timeout
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // DoesNotExist error is okay
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos) {
                Logger::info("Application already unregistered from BlueZ");
            } else {
                Logger::warn("Failed to unregister from BlueZ: " + error);
            }
        }
        
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;  // Update state anyway
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    Logger::debug("GetManagedObjects called");
    
    try {
        GVariantPtr objectsDict = createManagedObjectsDict();
        
        if (!objectsDict) {
            Logger::error("Failed to create objects dictionary");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Failed to create objects dictionary"
            );
            return;
        }
        
        // Return dictionary
        g_dbus_method_invocation_return_value(call.invocation.get(), objectsDict.get());
        
        Logger::debug("GetManagedObjects response sent");
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
        // Create empty dictionary builder
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // Lock services
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 1. Add services
        for (const auto& service : services) {
            if (!service) continue;
            
            GVariantBuilder interfacesBuilder;
            g_variant_builder_init(&interfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // Service interface
            GVariantBuilder propsBuilder;
            g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
            
            // UUID property
            g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
                                Utils::gvariantPtrFromString(service->getUuid().toBlueZFormat()).get());
            
            // Primary property
            g_variant_builder_add(&propsBuilder, "{sv}", "Primary", 
                                Utils::gvariantPtrFromBoolean(service->isPrimary()).get());
            
            // Add interfaces dictionary entry
            g_variant_builder_add(&interfacesBuilder, "{sa{sv}}", 
                                BlueZConstants::GATT_SERVICE_INTERFACE.c_str(), 
                                &propsBuilder);
            
            // Add to main dictionary
            g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                service->getPath().c_str(), 
                                &interfacesBuilder);
            
            // 2. Add characteristics for this service
            for (const auto& charPair : service->getCharacteristics()) {
                auto characteristic = charPair.second;
                if (!characteristic) continue;
                
                GVariantBuilder charInterfacesBuilder;
                g_variant_builder_init(&charInterfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                // Characteristic interface
                GVariantBuilder charPropsBuilder;
                g_variant_builder_init(&charPropsBuilder, G_VARIANT_TYPE("a{sv}"));
                
                // UUID property
                g_variant_builder_add(&charPropsBuilder, "{sv}", "UUID", 
                                    Utils::gvariantPtrFromString(characteristic->getUuid().toBlueZFormat()).get());
                
                // Service property
                g_variant_builder_add(&charPropsBuilder, "{sv}", "Service", 
                                    Utils::gvariantPtrFromObject(service->getPath()).get());
                
                // Flags property (characteristic properties)
                GVariantBuilder flagsBuilder;
                g_variant_builder_init(&flagsBuilder, G_VARIANT_TYPE("as"));
                
                uint8_t props = characteristic->getProperties();
                if (props & GattProperty::PROP_BROADCAST)
                    g_variant_builder_add(&flagsBuilder, "s", "broadcast");
                if (props & GattProperty::PROP_READ)
                    g_variant_builder_add(&flagsBuilder, "s", "read");
                if (props & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
                    g_variant_builder_add(&flagsBuilder, "s", "write-without-response");
                if (props & GattProperty::PROP_WRITE)
                    g_variant_builder_add(&flagsBuilder, "s", "write");
                if (props & GattProperty::PROP_NOTIFY)
                    g_variant_builder_add(&flagsBuilder, "s", "notify");
                if (props & GattProperty::PROP_INDICATE)
                    g_variant_builder_add(&flagsBuilder, "s", "indicate");
                
                g_variant_builder_add(&charPropsBuilder, "{sv}", "Flags", 
                                    g_variant_builder_end(&flagsBuilder));
                
                // Add interfaces dictionary entry
                g_variant_builder_add(&charInterfacesBuilder, "{sa{sv}}", 
                                    BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(), 
                                    &charPropsBuilder);
                
                // Add to main dictionary
                g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                    characteristic->getPath().c_str(), 
                                    &charInterfacesBuilder);
                
                // 3. Add descriptors for this characteristic
                for (const auto& descPair : characteristic->getDescriptors()) {
                    auto descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    GVariantBuilder descInterfacesBuilder;
                    g_variant_builder_init(&descInterfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    // Descriptor interface
                    GVariantBuilder descPropsBuilder;
                    g_variant_builder_init(&descPropsBuilder, G_VARIANT_TYPE("a{sv}"));
                    
                    // UUID property
                    g_variant_builder_add(&descPropsBuilder, "{sv}", "UUID", 
                                        Utils::gvariantPtrFromString(descriptor->getUuid().toBlueZFormat()).get());
                    
                    // Characteristic property
                    g_variant_builder_add(&descPropsBuilder, "{sv}", "Characteristic", 
                                        Utils::gvariantPtrFromObject(characteristic->getPath()).get());
                    
                    // Add interfaces dictionary entry
                    g_variant_builder_add(&descInterfacesBuilder, "{sa{sv}}", 
                                        BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(), 
                                        &descPropsBuilder);
                    
                    // Add to main dictionary
                    g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                        descriptor->getPath().c_str(), 
                                        &descInterfacesBuilder);
                }
            }
        }
        
        // Build and return
        return makeGVariantPtr(g_variant_builder_end(&builder), true);
    } catch (const std::exception& e) {
        Logger::error("Exception in createManagedObjectsDict: " + std::string(e.what()));
        return makeNullGVariantPtr();
    }
}

} // namespace ggk