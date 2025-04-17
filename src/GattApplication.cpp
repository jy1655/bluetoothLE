// src/GattApplication.cpp
#include "GattApplication.h"
#include "Logger.h"
#include "BlueZConstants.h"
#include <unistd.h>

namespace ggk {

GattApplication::GattApplication(SDBusConnection& connection, const std::string& path)
    : connection(connection),
      object(connection, path),
      registered(false) {
    
    Logger::info("GATT 애플리케이션 생성됨: " + path);
}

GattApplication::~GattApplication() {
    if (registered) {
        unregisterFromBlueZ();
    }
}

bool GattApplication::setupDBusInterfaces() {
    // 이미 등록된 경우 성공 반환
    if (object.isRegistered()) {
        Logger::debug("애플리케이션 객체가 이미 D-Bus에 등록됨");
        return true;
    }

    Logger::info("애플리케이션 D-Bus 인터페이스 설정: " + object.getPath());
    
    // ObjectManager 인터페이스 - GetManagedObjects 메서드
    if (!object.registerGetManagedObjectsMethod(
            [this]() -> std::map<sdbus::ObjectPath, 
                              std::map<std::string, 
                              std::map<std::string, sdbus::Variant>>> {
                // 복잡한 중첩 딕셔너리 생성 및 반환
                return handleGetManagedObjects();
            })) {
        Logger::error("GetManagedObjects 메서드 등록 실패");
        return false;
    }
    
    // InterfacesAdded, InterfacesRemoved 시그널 등록
    if (!object.registerSignal("org.freedesktop.DBus.ObjectManager", "InterfacesAdded")) {
        Logger::error("InterfacesAdded 시그널 등록 실패");
        return false;
    }
    
    if (!object.registerSignal("org.freedesktop.DBus.ObjectManager", "InterfacesRemoved")) {
        Logger::error("InterfacesRemoved 시그널 등록 실패");
        return false;
    }
    
    // D-Bus에 객체 등록
    if (!object.registerObject()) {
        Logger::error("애플리케이션 객체 등록 실패");
        return false;
    }
    
    Logger::info("GATT 애플리케이션 D-Bus 인터페이스 설정 완료");
    return true;
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("null 서비스를 추가할 수 없음");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
    // BlueZ에 이미 등록된 경우 서비스를 추가할 수 없음
    if (registered) {
        Logger::error("이미 등록된 애플리케이션에 서비스 추가 불가: " + uuidStr);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 중복 검사
        for (const auto& existingService : services) {
            if (existingService->getUuid().toString() == uuidStr) {
                Logger::warn("서비스가 이미 존재함: " + uuidStr);
                return false;
            }
        }
        
        // 목록에 추가
        services.push_back(service);
    }
    
    Logger::info("애플리케이션에 서비스 추가됨: " + uuidStr);
    return true;
}

bool GattApplication::removeService(const GattUuid& uuid) {
    std::string uuidStr = uuid.toString();
    
    // BlueZ에 이미 등록된 경우 서비스를 제거할 수 없음
    if (registered) {
        Logger::error("이미 등록된 애플리케이션에서 서비스 제거 불가: " + uuidStr);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (auto it = services.begin(); it != services.end(); ++it) {
        if ((*it)->getUuid().toString() == uuidStr) {
            services.erase(it);
            Logger::info("애플리케이션에서 서비스 제거됨: " + uuidStr);
            return true;
        }
    }
    
    Logger::warn("서비스를 찾을 수 없음: " + uuidStr);
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

bool GattApplication::finishAllRegistrations() {
    Logger::info("모든 GATT 객체 등록 완료 중");
    
    // 1. 애플리케이션 등록
    if (!object.isRegistered() && !setupDBusInterfaces()) {
        Logger::error("애플리케이션 객체 등록 실패");
        return false;
    }
    
    // 2. 모든 서비스, 특성, 설명자 등록
    bool success = true;
    
    // 2.1 서비스 등록
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        for (auto& service : services) {
            if (!service->isRegistered()) {
                if (!service->setupDBusInterfaces() || !service->finishRegistration()) {
                    Logger::error("서비스 등록 실패: " + service->getUuid().toString());
                    success = false;
                }
            }
            
            // 2.2 특성 등록과 2.3 설명자 등록은 각 서비스/특성 클래스 내부에서 처리됨
        }
    }
    
    return success;
}

bool GattApplication::registerWithBlueZ() {
    try {
        Logger::info("BlueZ에 GATT 애플리케이션 등록 중");
        
        // 1. 모든 D-Bus 객체가 등록되었는지 확인
        if (!finishAllRegistrations()) {
            Logger::error("D-Bus 객체 등록 실패");
            return false;
        }

        // 2. 이미 등록된 경우 성공 반환
        if (registered) {
            Logger::info("애플리케이션이 이미 BlueZ에 등록됨");
            return true;
        }
        
        // 3. BlueZ GattManager 프록시 생성
        auto proxy = connection.createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            BlueZConstants::ADAPTER_PATH
        );
        
        if (!proxy) {
            Logger::error("BlueZ GattManager 프록시 생성 실패");
            return false;
        }
        
        // 4. 빈 옵션 딕셔너리 생성
        std::map<std::string, sdbus::Variant> options;
        
        // 5. RegisterApplication 메서드 호출
        try {
            // RegisterApplication(ObjectPath, Dict of {String, Variant})
            proxy->callMethod("RegisterApplication")
                .onInterface(BlueZConstants::GATT_MANAGER_INTERFACE)
                .withArguments(sdbus::ObjectPath(object.getPath()), options);
            
            registered = true;
            Logger::info("BlueZ에 GATT 애플리케이션 등록 성공");
            return true;
        } catch (const sdbus::Error& e) {
            std::string errorName = e.getName();
            
            // "AlreadyExists" 오류는 성공으로 간주
            if (errorName.find("AlreadyExists") != std::string::npos) {
                Logger::info("애플리케이션이 이미 BlueZ에 등록됨 (AlreadyExists)");
                registered = true;
                return true;
            }
            
            Logger::error("BlueZ 애플리케이션 등록 실패: " + errorName + " - " + e.getMessage());
            
            // 먼저 등록 해제 시도
            try {
                unregisterFromBlueZ();
                
                // 잠시 대기
                usleep(500000); // 500ms
                
                // 다시 등록 시도
                proxy->callMethod("RegisterApplication")
                    .onInterface(BlueZConstants::GATT_MANAGER_INTERFACE)
                    .withArguments(sdbus::ObjectPath(object.getPath()), options);
                
                registered = true;
                Logger::info("재시도 후 BlueZ에 GATT 애플리케이션 등록 성공");
                return true;
            } catch (const sdbus::Error& retryEx) {
                Logger::error("재시도 시 BlueZ 등록 실패: " + std::string(retryEx.what()));
                return false;
            }
        }
    } catch (const std::exception& e) {
        Logger::error("registerWithBlueZ 예외: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    // 등록되지 않은 경우 할 일 없음
    if (!registered) {
        Logger::debug("애플리케이션이 BlueZ에 등록되지 않음, 해제할 것 없음");
        return true;
    }
    
    try {
        // BlueZ GattManager 프록시 생성
        auto proxy = connection.createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            BlueZConstants::ADAPTER_PATH
        );
        
        if (!proxy) {
            Logger::error("BlueZ GattManager 프록시 생성 실패");
            registered = false; // 재시도 방지를 위해 상태 업데이트
            return false;
        }
        
        // UnregisterApplication 호출
        try {
            proxy->callMethod("UnregisterApplication")
                .onInterface(BlueZConstants::GATT_MANAGER_INTERFACE)
                .withArguments(sdbus::ObjectPath(object.getPath()));
            
            Logger::info("BlueZ에서 애플리케이션 등록 해제 성공");
        } catch (const sdbus::Error& e) {
            // DoesNotExist 오류는 괜찮음
            std::string error = e.getName();
            if (error.find("DoesNotExist") != std::string::npos) {
                Logger::info("애플리케이션이 이미 BlueZ에서 등록 해제됨");
            } else {
                Logger::warn("BlueZ에서 등록 해제 실패: " + error);
            }
        }
        
        registered = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("unregisterFromBlueZ 예외: " + std::string(e.what()));
        registered = false;  // 상태 업데이트
        return false;
    }
}

// ManagedObjects 딕셔너리 생성 메서드
std::map<sdbus::ObjectPath, 
       std::map<std::string, 
       std::map<std::string, sdbus::Variant>>> GattApplication::handleGetManagedObjects() {
    return createManagedObjectsDict();
}

std::map<sdbus::ObjectPath, 
       std::map<std::string, 
       std::map<std::string, sdbus::Variant>>> GattApplication::createManagedObjectsDict() {
    std::map<sdbus::ObjectPath, 
           std::map<std::string, 
           std::map<std::string, sdbus::Variant>>> result;
    
    try {
        // 서비스 락
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        // 1. 서비스 추가
        for (const auto& service : services) {
            if (!service) continue;
            
            std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces;
            std::map<std::string, sdbus::Variant> properties;
            
            // UUID 속성
            properties["UUID"] = sdbus::Variant(service->getUuid().toBlueZFormat());
            // Primary 속성
            properties["Primary"] = sdbus::Variant(service->isPrimary());
            
            // 인터페이스에 속성 추가
            interfaces[BlueZConstants::GATT_SERVICE_INTERFACE] = properties;
            
            // 결과에 서비스 추가
            result[sdbus::ObjectPath(service->getPath())] = interfaces;
            
            // 2. 이 서비스의 특성 추가
            for (const auto& charPair : service->getCharacteristics()) {
                auto characteristic = charPair.second;
                if (!characteristic) continue;
                
                std::map<std::string, std::map<std::string, sdbus::Variant>> charInterfaces;
                std::map<std::string, sdbus::Variant> charProperties;
                
                // UUID 속성
                charProperties["UUID"] = sdbus::Variant(characteristic->getUuid().toBlueZFormat());
                
                // Service 속성
                charProperties["Service"] = sdbus::Variant(sdbus::ObjectPath(service->getPath()));
                
                // Flags 속성 (특성 속성)
                std::vector<std::string> flags;
                
                uint8_t props = characteristic->getProperties();
                if (props & GattProperty::PROP_BROADCAST)
                    flags.push_back("broadcast");
                if (props & GattProperty::PROP_READ)
                    flags.push_back("read");
                if (props & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
                    flags.push_back("write-without-response");
                if (props & GattProperty::PROP_WRITE)
                    flags.push_back("write");
                if (props & GattProperty::PROP_NOTIFY)
                    flags.push_back("notify");
                if (props & GattProperty::PROP_INDICATE)
                    flags.push_back("indicate");
                if (props & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
                    flags.push_back("authenticated-signed-writes");
                
                charProperties["Flags"] = sdbus::Variant(flags);
                
                // 인터페이스에 속성 추가
                charInterfaces[BlueZConstants::GATT_CHARACTERISTIC_INTERFACE] = charProperties;
                
                // 결과에 특성 추가
                result[sdbus::ObjectPath(characteristic->getPath())] = charInterfaces;
                
                // 3. 이 특성의 설명자 추가
                for (const auto& descPair : characteristic->getDescriptors()) {
                    auto descriptor = descPair.second;
                    if (!descriptor) continue;
                    
                    std::map<std::string, std::map<std::string, sdbus::Variant>> descInterfaces;
                    std::map<std::string, sdbus::Variant> descProperties;
                    
                    // UUID 속성
                    descProperties["UUID"] = sdbus::Variant(descriptor->getUuid().toBlueZFormat());
                    
                    // Characteristic 속성
                    descProperties["Characteristic"] = sdbus::Variant(sdbus::ObjectPath(characteristic->getPath()));
                    
                    // 인터페이스에 속성 추가
                    descInterfaces[BlueZConstants::GATT_DESCRIPTOR_INTERFACE] = descProperties;
                    
                    // 결과에 설명자 추가
                    result[sdbus::ObjectPath(descriptor->getPath())] = descInterfaces;
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("createManagedObjectsDict 예외: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace ggk