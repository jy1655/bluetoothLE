#include "GattApplication.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"
#include "Utils.h"
#include "BlueZConstants.h"
#include <unistd.h>

namespace ggk {

GattApplication::GattApplication(std::shared_ptr<IDBusConnection> connection, const DBusObjectPath& path)
    : DBusObject(connection, path),
      registered(false),
      standardServicesAdded(false) {
    
    Logger::info("Created GATT application at path: " + path.toString());
}

bool GattApplication::setupDBusInterfaces() {
    // 이미 등록된 경우 성공 반환
    if (isRegistered()) {
        Logger::debug("Application object already registered with D-Bus");
        return true;
    }

    Logger::info("Setting up D-Bus interfaces for application: " + getPath().toString());
    
    // ObjectManager 인터페이스와 메서드 추가
    addMethod("org.freedesktop.DBus.ObjectManager", "GetManagedObjects", 
              [this](const DBusMethodCall& call) { this->handleGetManagedObjects(call); });
    
    // 객체를 D-Bus에 등록
    if (!finishRegistration()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    return true;
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("Cannot add null service");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
    // BlueZ에 이미 등록된 경우 서비스를 추가할 수 없음
    if (registered) {
        Logger::error("Cannot add service to already registered application: " + uuidStr);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 중복 검사
        for (const auto& existingService : services) {
            if (existingService->getUuid().toString() == uuidStr) {
                Logger::warn("Service already exists: " + uuidStr);
                return false;
            }
        }
        
        // 목록에 추가
        services.push_back(service);
    }
    
    Logger::info("Added service to application: " + uuidStr);
    return true;
}

bool GattApplication::removeService(const GattUuid& uuid) {
    std::string uuidStr = uuid.toString();
    
    // BlueZ에 이미 등록된 경우 서비스를 제거할 수 없음
    if (registered) {
        Logger::error("Cannot remove service from already registered application: " + uuidStr);
        return false;
    }
    
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
    
    // 1. 애플리케이션 객체 먼저 등록 (ObjectManager 인터페이스 포함)
    if (!isRegistered()) {
        if (!setupDBusInterfaces()) {
            Logger::error("Failed to register application object with D-Bus");
            return false;
        }
    }
    
    // 2. 등록할 객체 수집
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
    
    // 3. 계층 순서대로 등록 (서비스 -> 특성 -> 설명자)
    bool allSuccessful = true;
    
    // 3.1 서비스 등록
    for (auto& service : servicesToRegister) {
        if (!service->finishRegistration()) {
            Logger::error("Failed to register service: " + service->getUuid().toString());
            allSuccessful = false;
        }
    }
    
    // D-Bus 처리 대기
    if (!servicesToRegister.empty()) {
        usleep(100000);  // 100ms
    }
    
    // 3.2 특성 등록
    for (const auto& servicePair : characteristicsToRegister) {
        auto service = servicePair.first;
        for (auto& characteristic : servicePair.second) {
            if (!characteristic->finishRegistration()) {
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
    
    // 3.3 설명자 등록
    for (const auto& charPair : descriptorsToRegister) {
        auto characteristic = charPair.first;
        for (auto& descriptor : charPair.second) {
            if (!descriptor->finishRegistration()) {
                Logger::error("Failed to register descriptor: " + 
                             descriptor->getUuid().toString() + 
                             " for characteristic: " + characteristic->getUuid().toString());
                allSuccessful = false;
            }
        }
    }
    
    // 등록된 객체 수 카운트
    int totalServices = servicesToRegister.size();
    int totalChars = 0;
    for (const auto& pair : characteristicsToRegister) {
        totalChars += pair.second.size();
    }
    
    int totalDescs = 0;
    for (const auto& pair : descriptorsToRegister) {
        totalDescs += pair.second.size();
    }
    
    // 등록 통계 로깅
    Logger::info("Interface registration complete: " +
                std::to_string(totalServices) + " services, " +
                std::to_string(totalChars) + " characteristics, " +
                std::to_string(totalDescs) + " descriptors" +
                ", success: " + (allSuccessful ? "true" : "false"));
    
    return allSuccessful;
}

bool GattApplication::finishAllRegistrations() {
    Logger::info("Finalizing all GATT object registrations");
    
    // 1. 애플리케이션 등록
    if (!isRegistered() && !setupDBusInterfaces()) {
        Logger::error("Failed to register application object");
        return false;
    }
    
    // 2. 모든 서비스, 특성, 설명자 등록
    bool success = true;
    
    // 2.1 서비스 등록
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        for (auto& service : services) {
            if (!service->finishRegistration()) {
                Logger::error("Failed to register service: " + service->getUuid().toString());
                success = false;
            }
            
            // 2.2 특성 등록
            for (const auto& charPair : service->getCharacteristics()) {
                auto& characteristic = charPair.second;
                if (characteristic && !characteristic->finishRegistration()) {
                    Logger::error("Failed to register characteristic: " + characteristic->getUuid().toString());
                    success = false;
                }
                
                // 2.3 설명자 등록
                if (characteristic) {
                    for (const auto& descPair : characteristic->getDescriptors()) {
                        auto& descriptor = descPair.second;
                        if (descriptor && !descriptor->finishRegistration()) {
                            Logger::error("Failed to register descriptor: " + descriptor->getUuid().toString());
                            success = false;
                        }
                    }
                }
            }
        }
    }
    
    // 3. 등록 검증
    if (success && !validateObjectHierarchy()) {
        Logger::warn("Object hierarchy validation failed");
        success = false;
    }
    
    return success;
}

bool GattApplication::registerWithBlueZ() {
    try {
        Logger::info("Registering GATT application with BlueZ");
        
        // 1. 모든 D-Bus 객체가 등록되었는지 확인
        if (!finishAllRegistrations()) {
            Logger::error("Failed to register D-Bus objects");
            return false;
        }

        // 2. 이미 등록된 경우 성공 반환
        if (registered) {
            Logger::info("Application already registered with BlueZ");
            return true;
        }
        
        // 3. 옵션 사전 생성
        GVariant* emptyDict = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);
        GVariant* params = g_variant_new("(o@a{sv})", 
                                       getPath().c_str(), 
                                       emptyDict);
        
        // 소유권 확보
        g_variant_ref_sink(params);
        
        // 4. BlueZ 호출 (재시도 포함)
        try {
            // 애플리케이션 등록
            getConnection()->callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::REGISTER_APPLICATION,
                makeGVariantPtr(params, true),
                "",
                10000  // 10초 타임아웃
            );
            
            registered = true;
            Logger::info("Successfully registered GATT application with BlueZ");
            return true;
        } catch (const std::exception& e) {
            std::string error = e.what();
            
            // "AlreadyExists" 오류는 성공으로 간주
            if (error.find("AlreadyExists") != std::string::npos) {
                Logger::info("Application already registered with BlueZ");
                registered = true;
                return true;
            }
            
            // 먼저 등록 해제 시도
            try {
                unregisterFromBlueZ();
                
                // 잠시 대기
                usleep(500000); // 500ms
                
                // 새 매개변수 생성
                GVariant* emptyDict2 = g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);
                GVariant* params2 = g_variant_new("(o@a{sv})", 
                                               getPath().c_str(), 
                                               emptyDict2);
                g_variant_ref_sink(params2);
                
                // 다시 등록 시도
                getConnection()->callMethod(
                    BlueZConstants::BLUEZ_SERVICE,
                    DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                    BlueZConstants::GATT_MANAGER_INTERFACE,
                    BlueZConstants::REGISTER_APPLICATION,
                    makeGVariantPtr(params2, true),
                    "",
                    10000  // 10초 타임아웃
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
    // 등록되지 않은 경우 할 일 없음
    if (!registered) {
        Logger::debug("Application not registered with BlueZ, nothing to unregister");
        return true;
    }
    
    try {
        // 매개변수 생성
        GVariant* params = g_variant_new("(o)", getPath().c_str());
        g_variant_ref_sink(params);
        
        // BlueZ 호출
        try {
            getConnection()->callMethod(
                BlueZConstants::BLUEZ_SERVICE,
                DBusObjectPath(BlueZConstants::ADAPTER_PATH),
                BlueZConstants::GATT_MANAGER_INTERFACE,
                BlueZConstants::UNREGISTER_APPLICATION,
                makeGVariantPtr(params, true),
                "",
                5000  // 5초 타임아웃
            );
            
            Logger::info("Successfully unregistered application from BlueZ");
        } catch (const std::exception& e) {
            // DoesNotExist 오류는 괜찮음
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
        registered = false;  // 상태 업데이트
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
        
        // BlueZ 5.82 호환성: 딕셔너리를 튜플로 래핑
        GVariant* wrappedDict = g_variant_new("(a{oa{sa{sv}}})", objectsDict.get());
        g_variant_ref(wrappedDict);
        
        // 딕셔너리 반환
        g_dbus_method_invocation_return_value(call.invocation.get(), wrappedDict);
        
        // 참조 카운트 감소
        g_variant_unref(wrappedDict);
        
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
        // 빈 딕셔너리 빌더 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // 서비스 락
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 1. 서비스 추가
        for (const auto& service : services) {
            if (!service) continue;
            
            GVariantBuilder interfacesBuilder;
            g_variant_builder_init(&interfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            // 서비스 인터페이스
            GVariantBuilder propsBuilder;
            g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
            
            // UUID 속성
            g_variant_builder_add(&propsBuilder, "{sv}", "UUID", 
                                Utils::gvariantPtrFromString(service->getUuid().toBlueZFormat()).get());
            
            // Primary 속성
            g_variant_builder_add(&propsBuilder, "{sv}", "Primary", 
                                Utils::gvariantPtrFromBoolean(service->isPrimary()).get());
            
            // 인터페이스 딕셔너리 항목 추가
            g_variant_builder_add(&interfacesBuilder, "{sa{sv}}", 
                                BlueZConstants::GATT_SERVICE_INTERFACE.c_str(), 
                                &propsBuilder);
            
            // 주 딕셔너리에 추가
            g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                service->getPath().c_str(), 
                                &interfacesBuilder);
            
            // 2. 이 서비스의 특성 추가
            for (const auto& charPair : service->getCharacteristics()) {
                auto characteristic = charPair.second;
                if (!characteristic) continue;
                
                GVariantBuilder charInterfacesBuilder;
                g_variant_builder_init(&charInterfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                
                // 특성 인터페이스
                GVariantBuilder charPropsBuilder;
                g_variant_builder_init(&charPropsBuilder, G_VARIANT_TYPE("a{sv}"));
                
                // UUID 속성
                g_variant_builder_add(&charPropsBuilder, "{sv}", "UUID", 
                                    Utils::gvariantPtrFromString(characteristic->getUuid().toBlueZFormat()).get());
                
                // Service 속성
                g_variant_builder_add(&charPropsBuilder, "{sv}", "Service", 
                                    Utils::gvariantPtrFromObject(service->getPath()).get());
                
                // Flags 속성 (특성 속성)
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
                
                // 인터페이스 딕셔너리 항목 추가
                g_variant_builder_add(&charInterfacesBuilder, "{sa{sv}}", 
                                    BlueZConstants::GATT_CHARACTERISTIC_INTERFACE.c_str(), 
                                    &charPropsBuilder);
                
                // 주 딕셔너리에 추가
                g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                    characteristic->getPath().c_str(), 
                                    &charInterfacesBuilder);
                
                // 3. 이 특성의 설명자 추가
                for (const auto& descPair : characteristic->getDescriptors()) {
                    auto descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    GVariantBuilder descInterfacesBuilder;
                    g_variant_builder_init(&descInterfacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
                    
                    // 설명자 인터페이스
                    GVariantBuilder descPropsBuilder;
                    g_variant_builder_init(&descPropsBuilder, G_VARIANT_TYPE("a{sv}"));
                    
                    // UUID 속성
                    g_variant_builder_add(&descPropsBuilder, "{sv}", "UUID", 
                                        Utils::gvariantPtrFromString(descriptor->getUuid().toBlueZFormat()).get());
                    
                    // Characteristic 속성
                    g_variant_builder_add(&descPropsBuilder, "{sv}", "Characteristic", 
                                        Utils::gvariantPtrFromObject(characteristic->getPath()).get());
                    
                    // 인터페이스 딕셔너리 항목 추가
                    g_variant_builder_add(&descInterfacesBuilder, "{sa{sv}}", 
                                        BlueZConstants::GATT_DESCRIPTOR_INTERFACE.c_str(), 
                                        &descPropsBuilder);
                    
                    // 주 딕셔너리에 추가
                    g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                                        descriptor->getPath().c_str(), 
                                        &descInterfacesBuilder);
                }
            }
        }
        
        // 빌드하여 반환
        return makeGVariantPtr(g_variant_builder_end(&builder), true);
    } catch (const std::exception& e) {
        Logger::error("Exception in createManagedObjectsDict: " + std::string(e.what()));
        return makeNullGVariantPtr();
    }
}

bool GattApplication::validateObjectHierarchy() const {
    // 모든 객체가 올바르게 등록되었는지 확인하는 빠른 검사
    bool valid = true;
    
    if (!isRegistered()) {
        Logger::error("Application object not registered");
        valid = false;
    }
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (const auto& service : services) {
        if (!service || !service->isRegistered()) {
            Logger::error("Service not registered: " + 
                        (service ? service->getUuid().toString() : "null"));
            valid = false;
            continue;
        }
        
        for (const auto& charPair : service->getCharacteristics()) {
            auto& characteristic = charPair.second;
            if (!characteristic || !characteristic->isRegistered()) {
                Logger::error("Characteristic not registered: " + 
                             (characteristic ? characteristic->getUuid().toString() : "null") +
                             " in service: " + service->getUuid().toString());
                valid = false;
                continue;
            }
            
            for (const auto& descPair : characteristic->getDescriptors()) {
                auto& descriptor = descPair.second;
                if (!descriptor || !descriptor->isRegistered()) {
                    Logger::error("Descriptor not registered: " + 
                                 (descriptor ? descriptor->getUuid().toString() : "null") +
                                 " in characteristic: " + characteristic->getUuid().toString());
                    valid = false;
                }
            }
        }
    }
    
    return valid;
}

void GattApplication::logObjectHierarchy() const {
    Logger::info("GATT Object Hierarchy:");
    Logger::info("Application: " + getPath().toString() + 
               " (Registered: " + (isRegistered() ? "Yes" : "No") + ")");
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (size_t i = 0; i < services.size(); i++) {
        const auto& service = services[i];
        Logger::info("  Service[" + std::to_string(i) + "]: " + 
                    service->getPath().toString() + " - " + 
                    service->getUuid().toString() +
                    " (Registered: " + (service->isRegistered() ? "Yes" : "No") + ")");
        
        size_t charIndex = 0;
        for (const auto& charPair : service->getCharacteristics()) {
            const auto& characteristic = charPair.second;
            Logger::info("    Characteristic[" + std::to_string(charIndex++) + "]: " + 
                       characteristic->getPath().toString() + " - " + 
                       characteristic->getUuid().toString() +
                       " (Registered: " + (characteristic->isRegistered() ? "Yes" : "No") + ")");
            
            size_t descIndex = 0;
            for (const auto& descPair : characteristic->getDescriptors()) {
                const auto& descriptor = descPair.second;
                Logger::info("      Descriptor[" + std::to_string(descIndex++) + "]: " + 
                           descriptor->getPath().toString() + " - " + 
                           descriptor->getUuid().toString() +
                           " (Registered: " + (descriptor->isRegistered() ? "Yes" : "No") + ")");
            }
        }
    }
}

} // namespace ggk