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
    
    // 이 부분 제거 - 자동 등록 하지 않음
    // if (registered && !service->isRegistered()) {
    //    if (!service->setupDBusInterfaces()) {
    //        Logger::error("Failed to setup interfaces for service: " + uuidStr);
    //        // Remove the service since setup failed
    //        services.pop_back();
    //        return false;
    //    }
    // }
    
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
    // 이미 등록되었다면 건너뜀
    if (registered) {
        Logger::debug("Application object already registered, skipping interface setup");
        return true;
    }

    Logger::info("Ensuring all interfaces are registered for application: " + getPath().toString());
    
    // 1. 먼저 모든 서비스를 수집하고 특성/설명자도 함께 수집
    std::vector<GattServicePtr> allServices;
    std::vector<GattCharacteristicPtr> allCharacteristics;
    std::vector<GattDescriptorPtr> allDescriptors;
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 서비스 수집
        for (auto& service : services) {
            allServices.push_back(service);
            
            // 각 서비스의 특성 수집
            for (const auto& charPair : service->getCharacteristics()) {
                auto characteristic = charPair.second;
                if (characteristic) {
                    allCharacteristics.push_back(characteristic);
                    
                    // 각 특성의 설명자 수집
                    for (const auto& descPair : characteristic->getDescriptors()) {
                        auto descriptor = descPair.second;
                        if (descriptor) {
                            allDescriptors.push_back(descriptor);
                        }
                    }
                }
            }
        }
    }
    
    // 2. 모든 서비스를 먼저 등록 (BlueZ 계층 구조 준수)
    for (auto& service : allServices) {
        if (!service->isRegistered()) {
            Logger::debug("Registering service: " + service->getUuid().toString());
            if (!service->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for service: " + service->getUuid().toString());
                return false;
            }
        }
    }
    
    // 3. 그 다음 모든 특성 등록
    for (auto& characteristic : allCharacteristics) {
        if (!characteristic->isRegistered()) {
            Logger::debug("Registering characteristic: " + characteristic->getUuid().toString());
            if (!characteristic->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for characteristic: " + 
                             characteristic->getUuid().toString());
                return false;
            }
        }
    }
    
    // 4. 마지막으로 모든 설명자 등록
    for (auto& descriptor : allDescriptors) {
        if (!descriptor->isRegistered()) {
            Logger::debug("Registering descriptor: " + descriptor->getUuid().toString());
            if (!descriptor->setupDBusInterfaces()) {
                Logger::error("Failed to setup interfaces for descriptor: " + 
                             descriptor->getUuid().toString());
                return false;
            }
        }
    }
    
    // 5. 애플리케이션 객체 자체를 D-Bus에 등록
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface");
        return false;
    }
    
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                 [this](const DBusMethodCall& call) { 
                     handleGetManagedObjects(call); 
                 })) {
        Logger::error("Failed to add GetManagedObjects method");
        return false;
    }
    
    if (!registerObject()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    registered = true;
    Logger::info("All D-Bus interfaces registered successfully");
    
    // 검증 수행
    logObjectHierarchy();
    
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
    
    // 애플리케이션 확인
    if (!registered) {
        Logger::warn("Application object not registered");
        valid = false;
    }
    
    // 서비스 확인
    std::lock_guard<std::mutex> lock(servicesMutex);
    for (const auto& service : services) {
        if (!service->isRegistered()) {
            Logger::warn("Service " + service->getUuid().toString() + " not registered");
            valid = false;
        }
        
        // 특성 확인
        auto characteristics = service->getCharacteristics();
        for (const auto& pair : characteristics) {
            const auto& characteristic = pair.second;
            if (!characteristic->isRegistered()) {
                Logger::error("Characteristic " + characteristic->getUuid().toString() + " not registered");
                valid = false;
            }
            
            // NOTIFY/INDICATE 특성에 CCCD 확인
            uint8_t props = characteristic->getProperties();
            bool needsCccd = (props & GattProperty::PROP_NOTIFY) || (props & GattProperty::PROP_INDICATE);
            bool hasCccd = false;
            
            // 설명자 확인
            auto descriptors = characteristic->getDescriptors();
            for (const auto& descPair : descriptors) {
                const auto& descriptor = descPair.second;
                if (!descriptor->isRegistered()) {
                    Logger::error("Descriptor " + descriptor->getUuid().toString() + " not registered");
                    valid = false;
                }
                
                // CCCD 확인
                if (descriptor->getUuid().toBlueZShortFormat() == "00002902") {
                    hasCccd = true;
                }
            }
            
            // NOTIFY/INDICATE가 있지만 CCCD가 없으면 중요한 오류
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
    
    // 복사본 만들기 - 락 안에서만 짧게 접근
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
        
        // 특성 목록 복사
        auto characteristics = service->getCharacteristics();
        Logger::debug("    - Characteristics: " + std::to_string(characteristics.size()));
        
        for (const auto& pair : characteristics) {
            const auto& characteristic = pair.second;
            // 특성 속성 표시 추가
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
            
            // CCCD 확인 - NOTIFY 속성이 있지만 CCCD가 없는 경우 경고
            bool hasCccd = false;
            
            // 설명자 확인
            auto descriptors = characteristic->getDescriptors();
            if (!descriptors.empty()) {
                Logger::debug("        - Descriptors: " + std::to_string(descriptors.size()));
                
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    Logger::debug("          - Descriptor: " + descriptor->getUuid().toString() + 
                                 " at " + descriptor->getPath().toString() +
                                 " (Registered: " + (descriptor->isRegistered() ? "Yes" : "No") + ")");
                    
                    // CCCD 확인
                    if (descriptor->getUuid().toBlueZShortFormat() == "00002902") {
                        hasCccd = true;
                    }
                }
            }
            
            // NOTIFY 또는 INDICATE 속성이 있지만 CCCD가 없는 경우 경고
            if ((props & GattProperty::PROP_NOTIFY || props & GattProperty::PROP_INDICATE) && !hasCccd) {
                Logger::error("WARNING: Characteristic " + characteristic->getUuid().toString() + 
                             " has NOTIFY/INDICATE but no CCCD descriptor!");
            }
        }
    }
    
    Logger::debug("===== End of object hierarchy =====");
}

bool GattApplication::registerWithBlueZ() {
    try {
        // 1. 확인할 인터페이스 목록
        if (!validateObjectHierarchy()) {
            Logger::error("Object hierarchy validation failed - check your GATT structure");
            // 문제 확인을 위한 계층 구조 로깅
            logObjectHierarchy();
        }
        
        // 2. 이미 등록되었는지 확인
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 3. 등록 준비 - 아직 등록되지 않은 경우 등록
        if (!registered) {
            if (!ensureInterfacesRegistered()) {
                Logger::error("Failed to register application objects with D-Bus");
                return false;
            }
        }
        
        // 4. BlueZ 서비스 상태 확인
        int bluezStatus = system("systemctl is-active --quiet bluetooth.service");
        if (bluezStatus != 0) {
            Logger::error("BlueZ service is not active. Run: systemctl start bluetooth.service");
            return false;
        }
        
        // 5. BlueZ 어댑터 가용성 확인
        if (system("hciconfig hci0 > /dev/null 2>&1") != 0) {
            Logger::error("BlueZ adapter 'hci0' not found or not available");
            return false;
        }
        
        Logger::info("Sending RegisterApplication request to BlueZ");
        
        // 6. 등록 옵션 설정 - GVariantBuilder 수정
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&builder, "{sv}", "Experimental", g_variant_new_boolean(TRUE));
        GVariant* dict = g_variant_builder_end(&builder);
        
        // 7. 등록 매개변수 생성 - 더 간단한 방식으로
        GVariant* params = g_variant_new("(o@a{sv})", 
                                        getPath().c_str(), 
                                        dict);
        
        // floating reference 고정
        g_variant_ref_sink(params);
        
        // 8. 디버깅을 위한 매개변수 로깅
        char* debug_str = g_variant_print(params, TRUE);
        Logger::debug("RegisterApplication parameters: " + std::string(debug_str));
        g_free(debug_str);
        
        // 9. BlueZ RegisterApplication 메서드 호출
        try {
            // 간소화된 호출 방식
            GError* error = nullptr;
            GDBusConnection* conn = getConnection().getRawConnection();
            
            GVariant* result = g_dbus_connection_call_sync(
                conn,
                BlueZConstants::BLUEZ_SERVICE.c_str(),
                BlueZConstants::ADAPTER_PATH.c_str(),
                BlueZConstants::GATT_MANAGER_INTERFACE.c_str(),
                BlueZConstants::REGISTER_APPLICATION.c_str(),
                params,
                nullptr,  // 반환 타입 없음
                G_DBUS_CALL_FLAGS_NONE,
                60000,  // 60초 타임아웃
                nullptr,
                &error
            );
            
            // params 참조 해제
            g_variant_unref(params);
            
            if (error) {
                std::string errorMsg = error->message ? error->message : "Unknown error";
                Logger::error("Failed to register application: " + errorMsg);
                g_error_free(error);
                return false;
            }
            
            if (result) {
                g_variant_unref(result);
            }
            
            registered = true;
            Logger::info("Successfully registered application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            // params 참조 해제 확인
            g_variant_unref(params);
            
            std::string error = e.what();
            Logger::error("Exception in BlueZ registration: " + error);
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

// Modified handleGetManagedObjects method for GattApplication class
// Add this to src/GattApplication.cpp, replacing the existing method

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    try {
        Logger::info("GetManagedObjects called by BlueZ");
        
        // Register standard services (e.g., GAP) if needed
        if (!standardServicesAdded) {
            registerStandardServices();
        }
        
        // Make sure all objects are registered with D-Bus
        ensureInterfacesRegistered();
        
        // Log the object hierarchy for debugging
        logObjectHierarchy();
        
        // Create managed objects dictionary
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // Add root application object with ObjectManager interface
        {
            GVariantBuilder ifacesBuilder;
            g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // Add empty ObjectManager properties
            GVariantBuilder propsBuilder;
            g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
            
            g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
                                 BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                 &propsBuilder);
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                 getPath().c_str(), 
                                 &ifacesBuilder);
        }
        
        // Add services to the dictionary
        std::vector<GattServicePtr> servicesList;
        {
            std::lock_guard<std::mutex> lock(servicesMutex);
            servicesList = services;
        }
        
        for (const auto& service : servicesList) {
            if (!service) continue;
            
            // Add service object
            {
                GVariantBuilder ifacesBuilder;
                g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                GVariantBuilder propsBuilder;
                g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
                
                // Add UUID property
                g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
                                     g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));
                
                // Add Primary property
                g_variant_builder_add(&propsBuilder, "{sv}", "Primary", 
                                     g_variant_new_boolean(service->isPrimary()));
                
                // Get characteristics paths
                GVariantBuilder charPathsBuilder;
                g_variant_builder_init(&charPathsBuilder, G_VARIANT_TYPE("ao"));
                
                auto characteristics = service->getCharacteristics();
                for (const auto& pair : characteristics) {
                    if (pair.second) {
                        g_variant_builder_add(&charPathsBuilder, "o", 
                                             pair.second->getPath().c_str());
                    }
                }
                
                // Add characteristics array
                g_variant_builder_add(&propsBuilder, "{sv}", "Characteristics", 
                                     g_variant_builder_end(&charPathsBuilder));
                
                g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
                                     BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                                     &propsBuilder);
                
                g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                     service->getPath().c_str(),
                                     &ifacesBuilder);
            }
            
            // Process characteristics for this service
            auto characteristics = service->getCharacteristics();
            for (const auto& pair : characteristics) {
                const auto& characteristic = pair.second;
                if (!characteristic) continue;
                
                // Add characteristic object
                {
                    GVariantBuilder ifacesBuilder;
                    g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    GVariantBuilder propsBuilder;
                    g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
                    
                    // Add UUID property
                    g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
                                         g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));
                    
                    // Add Service property (path to parent service)
                    g_variant_builder_add(&propsBuilder, "{sv}", "Service", 
                                         g_variant_new_object_path(service->getPath().c_str()));
                    
                    // Add Flags property
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
                    
                    g_variant_builder_add(&propsBuilder, "{sv}", "Flags", 
                                         g_variant_builder_end(&flagsBuilder));
                    
                    // Get descriptor paths
                    GVariantBuilder descPathsBuilder;
                    g_variant_builder_init(&descPathsBuilder, G_VARIANT_TYPE("ao"));
                    
                    auto descriptors = characteristic->getDescriptors();
                    for (const auto& descPair : descriptors) {
                        if (descPair.second) {
                            g_variant_builder_add(&descPathsBuilder, "o", 
                                                 descPair.second->getPath().c_str());
                        }
                    }
                    
                    // Add descriptors array
                    g_variant_builder_add(&propsBuilder, "{sv}", "Descriptors", 
                                         g_variant_builder_end(&descPathsBuilder));
                    
                    // Add Notifying property
                    g_variant_builder_add(&propsBuilder, "{sv}", "Notifying", 
                                         g_variant_new_boolean(characteristic->isNotifying()));
                    
                    g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
                                         BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                         &propsBuilder);
                    
                    g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                         characteristic->getPath().c_str(),
                                         &ifacesBuilder);
                }
                
                // Process descriptors for this characteristic
                auto descriptors = characteristic->getDescriptors();
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    // Add descriptor object
                    {
                        GVariantBuilder ifacesBuilder;
                        g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                        
                        GVariantBuilder propsBuilder;
                        g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
                        
                        // Add UUID property
                        g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
                                             g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));
                        
                        // Add Characteristic property (path to parent characteristic)
                        g_variant_builder_add(&propsBuilder, "{sv}", "Characteristic", 
                                             g_variant_new_object_path(characteristic->getPath().c_str()));
                        
                        // Add Flags property
                        GVariantBuilder flagsBuilder;
                        g_variant_builder_init(&flagsBuilder, G_VARIANT_TYPE("as"));
                        
                        uint8_t perms = descriptor->getPermissions();
                        if (perms & GattPermission::PERM_READ)
                            g_variant_builder_add(&flagsBuilder, "s", "read");
                        if (perms & GattPermission::PERM_WRITE)
                            g_variant_builder_add(&flagsBuilder, "s", "write");
                        
                        g_variant_builder_add(&propsBuilder, "{sv}", "Flags", 
                                             g_variant_builder_end(&flagsBuilder));
                        
                        g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
                                             BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                             &propsBuilder);
                        
                        g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                             descriptor->getPath().c_str(),
                                             &ifacesBuilder);
                    }
                }
            }
        }
        
        // Build the final variant
        GVariant* result = g_variant_builder_end(&builder);
        
        // Log the result for debugging
        gsize n_children = g_variant_n_children(result);
        Logger::debug("Returning " + std::to_string(n_children) + " managed objects");
        
        // Print the first few objects as debug
        char* debug_str = g_variant_print(result, TRUE);
        if (debug_str) {
            std::string str = debug_str;
            Logger::debug("First 200 chars of result: " + str.substr(0, 200));
            g_free(debug_str);
        }
        
        // Return the result
        g_dbus_method_invocation_return_value(call.invocation.get(), result);
        
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