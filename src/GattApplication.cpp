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
    
    // 필수 인터페이스 미리 추가
    if (!addInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE, {})) {
        Logger::error("Failed to add ObjectManager interface during initialization");
    }
    
    // GetManagedObjects 메서드 추가
    if (!addMethod(BlueZConstants::OBJECT_MANAGER_INTERFACE, "GetManagedObjects", 
                  [this](const DBusMethodCall& call) { handleGetManagedObjects(call); })) {
        Logger::error("Failed to add GetManagedObjects method during initialization");
    }
    
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
        // 1. 객체 딕셔너리 빌더 초기화
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
        
        // 2. 애플리케이션 객체 (ObjectManager 인터페이스)
        {
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
            GVariantBuilder props_builder;
            g_variant_builder_init(&props_builder, G_VARIANT_TYPE("a{sv}"));
            
            g_variant_builder_add(&interfaces_builder, "{sa{sv}}",
                                 BlueZConstants::OBJECT_MANAGER_INTERFACE.c_str(),
                                 &props_builder);
            
            g_variant_builder_add(&builder, "{oa{sa{sv}}}",
                                 getPath().c_str(),
                                 &interfaces_builder);
        }
        
        // 3. 서비스 객체
        std::lock_guard<std::mutex> lock(servicesMutex);
        for (const auto& service : services) {
            if (!service) continue;
            
            GVariantBuilder interfaces_builder;
            g_variant_builder_init(&interfaces_builder, G_VARIANT_TYPE("a{sa{sv}}"));
            
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
        
        // 4. 응답 반환
        GVariant* result = g_variant_builder_end(&builder);
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
    }
}

} // namespace ggk