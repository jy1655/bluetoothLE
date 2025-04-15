// src/GattApplication.cpp
#include "GattApplication.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "Utils.h"
#include "BlueZConstants.h"
#include <unistd.h>
#include <numeric>

namespace ggk {

    GattApplication::GattApplication(DBusConnection& connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false),
      standardServicesAdded(false) {
    
    // 객체 경로 검증
    if (path.toString().find("/org/bluez/") == 0) {
        Logger::warn("Path starts with /org/bluez/ which is reserved by BlueZ");
    }
    
    // *** 중요: 객체 등록 전에 필수 인터페이스부터 추가 ***
    // ObjectManager 인터페이스 미리 추가
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface during initialization");
    }
    
    // GetManagedObjects 메소드 추가
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                  [this](const DBusMethodCall& call) { handleGetManagedObjects(call); })) {
        Logger::error("Failed to add GetManagedObjects method during initialization");
    }
    
    Logger::info("Created GATT application at path: " + path.toString());
}

bool GattApplication::setupDBusInterfaces() {
    // 이미 등록되었으면 성공으로 간주
    if (DBusObject::isRegistered()) {
        Logger::debug("Application object already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // 객체 계층을 확보하기 위해 서비스와 특성을 먼저 생성
    // 그러나 아직 등록하지는 않음 (이것이 중요함)
    Logger::debug("Preparing services and characteristics (without registering)");
    
    // 계층 구조 생성 전 등록 상태 확인
    bool anyRegistered = false;
    for (auto& service : services) {
        if (service->isRegistered()) {
            anyRegistered = true;
            break;
        }
        
        for (const auto& pair : service->getCharacteristics()) {
            if (pair.second && pair.second->isRegistered()) {
                anyRegistered = true;
                break;
            }
        }
        
        if (anyRegistered) break;
    }
    
    if (anyRegistered) {
        Logger::warn("Some objects are already registered - this may cause issues with object hierarchy");
    }
    
    // 이제 애플리케이션 객체 자체를 등록
    // ObjectManager 인터페이스는 이미 생성자에서 추가됨
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
    
    // 약간의 지연을 줘서 등록 완료 대기
    usleep(100000);  // 100ms
    validateObjectHierarchy();
    
    return nullptr;
}

std::vector<GattServicePtr> GattApplication::getServices() const {
    std::lock_guard<std::mutex> lock(servicesMutex);
    return services;
}

bool GattApplication::ensureInterfacesRegistered() {
    Logger::info("Ensuring all GATT interfaces are properly registered");
    
    // 1. Make sure application itself is registered first
    if (!isRegistered()) {
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to register application object with D-Bus");
            return false;
        }
    }
    
    // 2. Register all objects in a controlled order to ensure proper hierarchy
    bool allSuccessful = true;
    std::vector<GattServicePtr> servicesToRegister;
    std::map<GattServicePtr, std::vector<GattCharacteristicPtr>> characteristicsToRegister;
    std::map<GattCharacteristicPtr, std::vector<GattDescriptorPtr>> descriptorsToRegister;
    
    // First collect all objects that need registration
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
    
    // Now register everything in the correct order
    
    // 1. Register services
    for (auto& service : servicesToRegister) {
        if (!service->setupDBusInterfaces()) {
            Logger::error("Failed to register service: " + service->getUuid().toString());
            allSuccessful = false;
        }
    }
    
    // Add small delay to allow D-Bus to process
    if (!servicesToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 2. Register characteristics for each service
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
    
    // Add small delay to allow D-Bus to process
    if (!characteristicsToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3. Register descriptors for each characteristic
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
    
    // Count total registered objects
    int totalChars = 0;
    for (const auto& pair : characteristicsToRegister) {
        totalChars += pair.second.size();
    }
    
    int totalDescs = 0;
    for (const auto& pair : descriptorsToRegister) {
        totalDescs += pair.second.size();
    }
    
    // Log registration stats
    Logger::info("Interface registration complete: " +
                std::to_string(servicesToRegister.size()) + " services, " +
                std::to_string(totalChars) + " characteristics, " +
                std::to_string(totalDescs) + " descriptors" +
                ", success: " + (allSuccessful ? "true" : "false"));
    
    // Validate object hierarchy if all registrations succeeded
    if (allSuccessful) {
        validateObjectHierarchy();
    }
    
    return allSuccessful;
}

bool GattApplication::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT application with BlueZ");
        
        // 1. First ensure all objects are properly registered with D-Bus
        if (!ensureInterfacesRegistered()) {
            Logger::error("Failed to register GATT objects with D-Bus");
            return false;
        }
        
        // 2. Check if already registered with BlueZ
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 3. Create options dictionary for registration
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        
        // Force registration of all objects (including those without characteristics)
        g_variant_builder_add(&builder, "{sv}", "RegisterAll", g_variant_new_boolean(true));
        
        // Build parameters tuple
        GVariant* paramsRaw = g_variant_new("(oa{sv})", getPath().c_str(), &builder);
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        // 4. Register with BlueZ with retries
        const int MAX_RETRIES = 3;
        bool success = false;
        std::string lastError;
        
        for (int attempt = 0; attempt < MAX_RETRIES && !success; attempt++) {
            if (attempt > 0) {
                Logger::info("Retrying BlueZ registration (attempt " + 
                            std::to_string(attempt + 1) + " of " + std::to_string(MAX_RETRIES) + ")");
                usleep(1000000);  // 1 second between retries
            }
            
            try {
                // Send RegisterApplication request
                GVariantPtr result = getConnection().callMethod(
                    BlueZConstants::BLUEZ_SERVICE,
                    DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                    BlueZConstants::GATT_MANAGER_INTERFACE,
                    BlueZConstants::REGISTER_APPLICATION,
                    params ? makeGVariantPtr(g_variant_ref(params.get()), true) : makeNullGVariantPtr(),
                    "",
                    10000  // 10 second timeout
                );
                
                success = true;
                registered = true;
                Logger::info("Successfully registered GATT application with BlueZ");
                break;
            } catch (const std::exception& e) {
                lastError = e.what();
                
                // Check for "AlreadyExists" error - try to unregister first
                if (lastError.find("AlreadyExists") != std::string::npos) {
                    Logger::info("Application already registered with BlueZ, attempting to unregister first");
                    
                    try {
                        unregisterFromBlueZ();
                        // Continue with the next retry
                    } catch (...) {
                        Logger::warn("Error during unregister operation, continuing with retry");
                    }
                } else {
                    Logger::error("Failed to register with BlueZ: " + lastError);
                }
            }
        }
        
        if (!success) {
            Logger::error("Failed to register application with BlueZ after " + 
                         std::to_string(MAX_RETRIES) + " attempts. Last error: " + lastError);
            return false;
        }
        
        return true;
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
            
            // Check descriptors
            auto descriptors = characteristic->getDescriptors();
            if (!descriptors.empty()) {
                Logger::debug("        - Descriptors: " + std::to_string(descriptors.size()));
                
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    Logger::debug("          - Descriptor: " + descriptor->getUuid().toString() + 
                                 " at " + descriptor->getPath().toString() +
                                 " (Registered: " + (descriptor->isRegistered() ? "Yes" : "No") + ")");
                }
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
    
    Logger::info("BlueZ requested GetManagedObjects");
    
    try {
        // 매우 단순한 구현 - 각 서비스에 대해 최소한의 정보만 반환
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // 애플리케이션 객체 추가 (ObjectManager 인터페이스만)
        {
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // ObjectManager 인터페이스 (빈 속성)
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                 &props_builder);
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                 getPath().c_str(),
                                 &interfaces_builder);
        }
        
        // 각 서비스 추가 (UUID와 Primary 속성만)
        for (const auto& service : services) {
            if (!service) continue;
            
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // 서비스 인터페이스 및 속성
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            
            // UUID 속성 추가
            g_variant_builder_add(&props_builder, "{sv}", 
                                 "UUID", 
                                 g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));
            
            // Primary 속성 추가
            g_variant_builder_add(&props_builder, "{sv}", 
                                 "Primary", 
                                 g_variant_new_boolean(service->isPrimary()));
            
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                                 &props_builder);
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                 service->getPath().c_str(),
                                 &interfaces_builder);
        }
        
        // 응답 반환
        GVariant* result = g_variant_builder_end(&builder);
        
        // 로그로 응답 내용 출력 (디버깅용)
        char* debug_str = g_variant_print(result, TRUE);
        Logger::debug("GetManagedObjects response: " + std::string(debug_str));
        g_free(debug_str);
        
        g_dbus_method_invocation_return_value(call.invocation.get(), result);
        Logger::info("GetManagedObjects response sent successfully");
    }
    catch (const std::exception& e) {
        Logger::error("Exception in handleGetManagedObjects: " + std::string(e.what()));
        
        // 오류 발생 시 빈 응답 반환
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        GVariant* empty = g_variant_builder_end(&builder);
        
        g_dbus_method_invocation_return_value(call.invocation.get(), empty);
        g_variant_unref(empty);
    }
}



GVariantPtr GattApplication::createManagedObjectsDict() const {
    try {
        Logger::info("Creating managed objects dictionary");
        
        // 1. 최상위 딕셔너리 빌더 초기화
        // 형식: a{oa{sa{sv}}} (오브젝트 경로 -> 인터페이스 -> 속성 딕셔너리)
        GVariantBuilder objects_builder;
        g_variant_builder_init(&objects_builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    
        // 2. 애플리케이션 객체 (ObjectManager 인터페이스)
        {
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // ObjectManager 인터페이스용 빈 속성 맵
            GVariantBuilder empty_props;
            g_variant_builder_init(&empty_props, G_VARIANT_TYPE("a{sv}"));
            
            // 인터페이스 추가
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                 &empty_props);
            
            // 애플리케이션 객체 추가
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                 getPath().c_str(),
                                 &interfaces_builder);
        }
        
        // 3. 서비스 목록 가져오기 (스레드 안전하게)
        std::vector<GattServicePtr> servicesList;
        {
            std::lock_guard<std::mutex> lock(servicesMutex);
            servicesList = services;
        }
        
        // 4. 각 서비스 정보 추가
        for (const auto& service : servicesList) {
            if (!service) continue;
            
            // 서비스용 인터페이스 빌더
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
    
            // GATT 서비스 인터페이스용 속성 빌더
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            
            // UUID 속성 추가
            g_variant_builder_add(&props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_UUID.c_str(),
                                 g_variant_new_string(service->getUuid().toBlueZFormat().c_str()));
            
            // Primary 속성 추가
            g_variant_builder_add(&props_builder, "{sv}",
                                 BlueZConstants::PROPERTY_PRIMARY.c_str(),
                                 g_variant_new_boolean(service->isPrimary()));
            
            // BlueZ 5.82 호환성을 위한 Characteristics 속성 추가
            GVariantBuilder char_paths_builder;
            g_variant_builder_init(&char_paths_builder, G_VARIANT_TYPE("ao"));
            
            auto characteristics = service->getCharacteristics();
            for (const auto& pair : characteristics) {
                if (pair.second) {
                    g_variant_builder_add(&char_paths_builder, "o",
                                         pair.second->getPath().c_str());
                }
            }
            
            // Characteristics 경로 배열 추가
            g_variant_builder_add(&props_builder, "{sv}",
                                 "Characteristics",
                                 g_variant_builder_end(&char_paths_builder));
            
            // 서비스 인터페이스 추가
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                                 &props_builder);
            
            // 서비스 객체 추가
            g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                 service->getPath().c_str(),
                                 &interfaces_builder);
            
            // 5. 각 특성 정보 추가
            for (const auto& pair : characteristics) {
                const auto& characteristic = pair.second;
                if (!characteristic) continue;
                
                GVariantBuilder char_interfaces_builder;
                g_variant_builder_init(&char_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                GVariantBuilder char_props_builder;
                g_variant_builder_init(&char_props_builder, G_VARIANT_TYPE("a{sv}"));
                
                // UUID 속성 추가
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_UUID.c_str(),
                                     g_variant_new_string(characteristic->getUuid().toBlueZFormat().c_str()));
                
                // Service 속성 추가
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_SERVICE.c_str(),
                                     g_variant_new_object_path(service->getPath().c_str()));
                
                // Flags(속성) 배열 생성
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
                
                // 권한 플래그 추가
                uint8_t perms = characteristic->getPermissions();
                if (perms & GattPermission::PERM_READ_ENCRYPTED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-read");
                if (perms & GattPermission::PERM_WRITE_ENCRYPTED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-write");
                if (perms & GattPermission::PERM_READ_AUTHENTICATED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-authenticated-read");
                if (perms & GattPermission::PERM_WRITE_AUTHENTICATED)
                    g_variant_builder_add(&flags_builder, "s", "encrypt-authenticated-write");
                
                // Flags 속성 추가
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_FLAGS.c_str(),
                                     g_variant_builder_end(&flags_builder));
                
                // Descriptors 배열 생성
                GVariantBuilder desc_paths_builder;
                g_variant_builder_init(&desc_paths_builder, G_VARIANT_TYPE("ao"));
                
                auto descriptors = characteristic->getDescriptors();
                for (const auto& descPair : descriptors) {
                    if (descPair.second) {
                        g_variant_builder_add(&desc_paths_builder, "o",
                                             descPair.second->getPath().c_str());
                    }
                }
                
                // Descriptors 속성 추가
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     "Descriptors",
                                     g_variant_builder_end(&desc_paths_builder));
                
                // Notifying 속성 추가
                g_variant_builder_add(&char_props_builder, "{sv}",
                                     BlueZConstants::PROPERTY_NOTIFYING.c_str(),
                                     g_variant_new_boolean(characteristic->isNotifying()));
                
                // 특성 인터페이스 추가
                g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                     BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                     &char_props_builder);
                
                // 특성 객체 추가
                g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                     characteristic->getPath().c_str(),
                                     &char_interfaces_builder);
                
                // 6. 각 디스크립터 정보 추가
                for (const auto& descPair : descriptors) {
                    const auto& descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    GVariantBuilder desc_interfaces_builder;
                    g_variant_builder_init(&desc_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    GVariantBuilder desc_props_builder;
                    g_variant_builder_init(&desc_props_builder, G_VARIANT_TYPE("a{sv}"));
                    
                    // UUID 속성 추가
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_UUID.c_str(),
                                         g_variant_new_string(descriptor->getUuid().toBlueZFormat().c_str()));
                    
                    // Characteristic 속성 추가
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_CHARACTERISTIC.c_str(),
                                         g_variant_new_object_path(characteristic->getPath().c_str()));
                    
                    // Flags 배열 생성 (디스크립터용)
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
                        g_variant_builder_add(&desc_flags_builder, "s", "encrypt-authenticated-read");
                    if (descPerms & GattPermission::PERM_WRITE_AUTHENTICATED)
                        g_variant_builder_add(&desc_flags_builder, "s", "encrypt-authenticated-write");
                    
                    // Flags 속성 추가
                    g_variant_builder_add(&desc_props_builder, "{sv}",
                                         BlueZConstants::PROPERTY_FLAGS.c_str(),
                                         g_variant_builder_end(&desc_flags_builder));
                    
                    // 디스크립터 인터페이스 추가
                    g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                         BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                         &desc_props_builder);
                    
                    // 디스크립터 객체 추가
                    g_variant_builder_add(&objects_builder, "{oa{sa{sv}}}",
                                         descriptor->getPath().c_str(),
                                         &desc_interfaces_builder);
                }
            }
        }
        
        // 7. 딕셔너리 완성 및 반환
        GVariant* result = g_variant_builder_end(&objects_builder);
        
        // 결과 검증
        if (!g_variant_is_of_type(result, G_VARIANT_TYPE("a{oa{sa{sv}}}"))) {
            Logger::error("Created variant has incorrect type: " + 
                         std::string(g_variant_get_type_string(result)));
            g_variant_unref(result);
            return makeNullGVariantPtr();
        }
        
        // 객체 수 로그
        gsize n_children = g_variant_n_children(result);
        Logger::debug("Managed objects dictionary contains " + 
                     std::to_string(n_children) + " objects");
        
        if (n_children < 1) {
            Logger::warn("Managed objects dictionary is empty!");
        }
        
        // 스마트 포인터로 래핑하여 반환
        return makeGVariantPtr(result, true);
        
    } catch (const std::exception& e) {
        Logger::error("Exception in createManagedObjectsDict: " + std::string(e.what()));
        return makeNullGVariantPtr();
    }
}

} // namespace ggk