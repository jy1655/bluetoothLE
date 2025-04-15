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
    
    Logger::info("Created GATT application at path: " + path.toString());
}

bool GattApplication::setupDBusInterfaces() {
    // 이미 등록된 경우 성공 반환
    if (DBusObject::isRegistered()) {
        Logger::debug("Application object already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // D-Bus에 객체 등록
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
    
    // 이미 BlueZ에 등록된 경우 서비스 추가 불가
    if (registered) {
        Logger::error("Cannot add service to already registered application: " + uuidStr);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 중복 서비스 확인
        for (const auto& existingService : services) {
            if (existingService->getUuid().toString() == uuidStr) {
                Logger::warn("Service already exists: " + uuidStr);
                return false;
            }
        }
        
        // 서비스 목록에 추가
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
    
    // 1. 애플리케이션 자체가 등록되어 있는지 확인
    if (!isRegistered()) {
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to register application object with D-Bus");
            return false;
        }
    }
    
    // 2. 등록할 객체 수집
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
    
    // 3. 순서대로 객체 등록
    
    // 3.1. 서비스 등록
    for (auto& service : servicesToRegister) {
        if (!service->setupDBusInterfaces()) {
            Logger::error("Failed to register service: " + service->getUuid().toString());
            allSuccessful = false;
        }
    }
    
    // D-Bus 처리 대기
    if (!servicesToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3.2. 특성 등록
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
    
    // D-Bus 처리 대기
    if (!characteristicsToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3.3. 디스크립터 등록
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
    
    // 등록된 객체 수 계산
    int totalChars = 0;
    for (const auto& pair : characteristicsToRegister) {
        totalChars += pair.second.size();
    }
    
    int totalDescs = 0;
    for (const auto& pair : descriptorsToRegister) {
        totalDescs += pair.second.size();
    }
    
    // 등록 통계 로그
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
        
        // 1. 모든 객체가 D-Bus에 등록되어 있는지 확인
        if (!ensureInterfacesRegistered()) {
            Logger::error("Failed to register GATT objects with D-Bus");
            return false;
        }
        
        // 2. 이미 BlueZ에 등록되어 있는지 확인
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 3. 등록 옵션 딕셔너리 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        
        // 모든 객체(특성 없는 서비스 포함) 등록 옵션
        g_variant_builder_add(&builder, "{sv}", "RegisterAll", g_variant_new_boolean(true));
        
        // 매개변수 튜플 생성
        GVariant* paramsRaw = g_variant_new("(oa{sv})", getPath().c_str(), &builder);
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        // 4. BlueZ에 등록 (최대 1회 재시도)
        try {
            // RegisterApplication 요청 전송
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                params ? makeGVariantPtr(g_variant_ref(params.get()), true) : makeNullGVariantPtr(),
                "",
                10000  // 10초 타임아웃
            );
            
            registered = true;
            Logger::info("Successfully registered GATT application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            
            // "AlreadyExists" 오류 처리
            if (error.find("AlreadyExists") != std::string::npos) {
                Logger::info("Application already registered with BlueZ");
                registered = true;
                return true;
            }
            
            Logger::error("Failed to register with BlueZ: " + error);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in registerWithBlueZ: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    // 등록되지 않은 경우 성공 반환
    if (!registered) {
        Logger::debug("Application not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        // 매개변수 생성
        GVariant* paramsRaw = g_variant_new("(o)", getPath().c_str());
        GVariantPtr params = makeGVariantPtr(paramsRaw, true);
        
        // BlueZ UnregisterApplication 호출
        try {
            getConnection().callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                std::move(params),
                "",
                5000  // 5초 타임아웃
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // DoesNotExist 오류 무시
            std::string error = e.what();
            if (error.find("DoesNotExist") != std::string::npos || 
                error.find("Does Not Exist") != std::string::npos) {
                Logger::info("Application already unregistered or not found in BlueZ");
            } else {
                Logger::warn("Failed to unregister from BlueZ: " + error);
            }
        }
        
        // 로컬 상태 업데이트
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in unregisterFromBlueZ: " + std::string(e.what()));
        registered = false;
        return false;
    }
}

void GattApplication::handleGetManagedObjects(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in GetManagedObjects");
        return;
    }
    
    Logger::info("BlueZ requested GetManagedObjects");
    
    try {
        // GVariant 객체 딕셔너리 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // 1. 애플리케이션 객체 추가 (ObjectManager 인터페이스)
        {
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // 객체 관리자 인터페이스 (빈 속성으로)
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                &props_builder);
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                getPath().c_str(),
                                &interfaces_builder);
        }
        
        // 2. 서비스 객체들 추가
        std::lock_guard<std::mutex> lock(servicesMutex);
        for (const auto& service : services) {
            if (!service) continue;
            
            // 이 서비스의 모든 인터페이스-속성 맵 구성
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // GattService1 인터페이스 추가
            {
                GVariantBuilder props_builder;
                g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
                
                // UUID 속성
                GVariantPtr uuid_variant = Utils::gvariantPtrFromString(service->getUuid().toBlueZFormat());
                g_variant_builder_add(&props_builder, "{sv}", 
                                    "UUID", 
                                    uuid_variant.get());
                
                // Primary 속성
                GVariantPtr primary_variant = Utils::gvariantPtrFromBoolean(service->isPrimary());
                g_variant_builder_add(&props_builder, "{sv}", 
                                    "Primary", 
                                    primary_variant.get());
                
                // 서비스 경로에 있는 특성들의 객체 경로 목록
                GVariantBuilder char_paths_builder;
                g_variant_builder_init(&char_paths_builder, G_VARIANT_TYPE("ao"));
                
                for (const auto& charPair : service->getCharacteristics()) {
                    if (charPair.second) {
                        g_variant_builder_add(&char_paths_builder, "o", 
                                            charPair.second->getPath().c_str());
                    }
                }
                
                GVariant* char_paths = g_variant_builder_end(&char_paths_builder);
                g_variant_builder_add(&props_builder, "{sv}", "Characteristics", char_paths);
                
                g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                    BlueZConstants::GATT_SERVICE_INTERFACE.c_str(),
                                    &props_builder);
            }
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                service->getPath().c_str(),
                                &interfaces_builder);
                                
            // 3. 이 서비스에 속한 모든 특성들 추가
            for (const auto& charPair : service->getCharacteristics()) {
                auto characteristic = charPair.second;
                if (!characteristic) continue;
                
                GVariantBuilder char_interfaces_builder;
                g_variant_builder_init(&char_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                // GattCharacteristic1 인터페이스 추가
                {
                    GVariantBuilder char_props_builder;
                    g_variant_builder_init(&char_props_builder, G_VARIANT_TYPE("a{sv}"));
                    
                    // UUID 속성
                    GVariantPtr char_uuid_variant = Utils::gvariantPtrFromString(
                        characteristic->getUuid().toBlueZFormat());
                    g_variant_builder_add(&char_props_builder, "{sv}", 
                                        "UUID", 
                                        char_uuid_variant.get());
                    
                    // Service 속성 (서비스 경로)
                    GVariantPtr service_path_variant = Utils::gvariantPtrFromObject(
                        service->getPath());
                    g_variant_builder_add(&char_props_builder, "{sv}", 
                                        "Service", 
                                        service_path_variant.get());
                    
                    // Flags 속성 (특성 속성 문자열 배열)
                    GVariantBuilder flags_builder;
                    g_variant_builder_init(&flags_builder, G_VARIANT_TYPE("as"));
                    
                    // 권한에 따라 플래그 추가
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
                    
                    GVariant* flags = g_variant_builder_end(&flags_builder);
                    g_variant_builder_add(&char_props_builder, "{sv}", "Flags", flags);
                    
                    // Notifying 속성
                    GVariantPtr notifying_variant = Utils::gvariantPtrFromBoolean(
                        characteristic->isNotifying());
                    g_variant_builder_add(&char_props_builder, "{sv}", 
                                        "Notifying", 
                                        notifying_variant.get());
                    
                    // 디스크립터가 있는 경우 추가
                    auto descriptors = characteristic->getDescriptors();
                    if (!descriptors.empty()) {
                        GVariantBuilder desc_paths_builder;
                        g_variant_builder_init(&desc_paths_builder, G_VARIANT_TYPE("ao"));
                        
                        for (const auto& descPair : descriptors) {
                            if (descPair.second) {
                                g_variant_builder_add(&desc_paths_builder, "o", 
                                                    descPair.second->getPath().c_str());
                            }
                        }
                        
                        GVariant* desc_paths = g_variant_builder_end(&desc_paths_builder);
                        g_variant_builder_add(&char_props_builder, "{sv}", "Descriptors", desc_paths);
                    }
                    
                    g_variant_builder_add(&char_interfaces_builder, "{sa{sv}}",
                                        BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(),
                                        &char_props_builder);
                }
                
                g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                    characteristic->getPath().c_str(),
                                    &char_interfaces_builder);
                
                // 4. 이 특성에 속한 모든 디스크립터들 추가
                for (const auto& descPair : characteristic->getDescriptors()) {
                    auto descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    GVariantBuilder desc_interfaces_builder;
                    g_variant_builder_init(&desc_interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    // GattDescriptor1 인터페이스 추가
                    {
                        GVariantBuilder desc_props_builder;
                        g_variant_builder_init(&desc_props_builder, G_VARIANT_TYPE("a{sv}"));
                        
                        // UUID 속성
                        GVariantPtr desc_uuid_variant = Utils::gvariantPtrFromString(
                            descriptor->getUuid().toBlueZFormat());
                        g_variant_builder_add(&desc_props_builder, "{sv}", 
                                            "UUID", 
                                            desc_uuid_variant.get());
                        
                        // Characteristic 속성 (특성 경로)
                        GVariantPtr char_path_variant = Utils::gvariantPtrFromObject(
                            characteristic->getPath());
                        g_variant_builder_add(&desc_props_builder, "{sv}", 
                                            "Characteristic", 
                                            char_path_variant.get());
                        
                        // Flags 속성 (디스크립터 권한 문자열 배열)
                        GVariantBuilder desc_flags_builder;
                        g_variant_builder_init(&desc_flags_builder, G_VARIANT_TYPE("as"));
                        
                        // 권한에 따라 플래그 추가
                        uint8_t perms = descriptor->getPermissions();
                        if (perms & GattPermission::PERM_READ)
                            g_variant_builder_add(&desc_flags_builder, "s", "read");
                        if (perms & GattPermission::PERM_WRITE)
                            g_variant_builder_add(&desc_flags_builder, "s", "write");
                        if (perms & GattPermission::PERM_READ_ENCRYPTED)
                            g_variant_builder_add(&desc_flags_builder, "s", "encrypt-read");
                        if (perms & GattPermission::PERM_WRITE_ENCRYPTED)
                            g_variant_builder_add(&desc_flags_builder, "s", "encrypt-write");
                        
                        GVariant* desc_flags = g_variant_builder_end(&desc_flags_builder);
                        g_variant_builder_add(&desc_props_builder, "{sv}", "Flags", desc_flags);
                        
                        g_variant_builder_add(&desc_interfaces_builder, "{sa{sv}}",
                                            BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(),
                                            &desc_props_builder);
                    }
                    
                    g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                        descriptor->getPath().c_str(),
                                        &desc_interfaces_builder);
                }
            }
        }
        
        // 최종 딕셔너리로 반환
        GVariant* result = g_variant_builder_end(&builder);
        g_dbus_method_invocation_return_value(call.invocation.get(), result);
        
        Logger::debug("GetManagedObjects response sent successfully");
    }
    catch (const std::exception& e) {
        Logger::error("Exception in handleGetManagedObjects: " + std::string(e.what()));
        
        // 오류 발생 시 빈 응답 반환
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        GVariant* empty = g_variant_builder_end(&builder);
        
        g_dbus_method_invocation_return_value(call.invocation.get(), empty);
    }
}

} // namespace ggk