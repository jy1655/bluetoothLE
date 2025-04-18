// src/GattApplication.cpp
#include "GattApplication.h"
#include "Logger.h"
#include "BlueZConstants.h"
#include <unistd.h>

// 중요: 이 파일에서는 GattService.cpp의 구현을 포함하면 안 됩니다!
// #include "GattService.cpp" 같은 코드가 있다면 제거해야 합니다.

namespace ggk {

GattApplication::GattApplication(SDBusConnection& connection, const std::string& path)
    : connection(connection),
      object(connection, path),
      registered(false),
      interfaceSetup(false),
      boundToBlueZ(false) {
    
    Logger::info("GATT 애플리케이션 생성됨: " + path);
}

GattApplication::~GattApplication() {
    if (boundToBlueZ) {
        unbindFromBlueZ();
    }
    
    // 먼저 서비스 등록 해제
    for (auto& service : services) {
        if (service) {
            service->unregisterObject();
        }
    }
    
    // 그 다음 애플리케이션 등록 해제
    object.unregisterObject();
}

bool GattApplication::setupInterfaces() {
    if (interfaceSetup) return true;
    
    try {
        Logger::info("애플리케이션 인터페이스 설정 시작: " + object.getPath());
        
        // 애플리케이션 객체에는 ObjectManager 인터페이스만 설정
        // GetManagedObjects 메서드 설정 - 실제 객체 요청 시 호출됨
        if (!object.registerObjectManager([this]() -> ManagedObjectsDict {
            return this->createManagedObjectsDict();
        })) {
            Logger::error("ObjectManager 인터페이스 등록 실패");
            return false;
        }
        
        // ObjectManager 인터페이스가 설정된 애플리케이션 객체 등록
        if (!object.registerObject()) {
            Logger::error("애플리케이션 객체 등록 실패");
            return false;
        }
        
        // 설정 완료 표시
        interfaceSetup = true;
        Logger::info("애플리케이션 인터페이스 설정 완료: " + object.getPath());
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("애플리케이션 인터페이스 설정 실패: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::bindToBlueZ() {
    if (boundToBlueZ) return true;
    
    try {
        Logger::info("BlueZ에 GATT 애플리케이션 바인딩 중");
        
        // 인터페이스가 설정되지 않은 경우 먼저 설정
        if (!interfaceSetup) {
            if (!setupInterfaces()) {
                Logger::error("인터페이스 설정 실패로 BlueZ 바인딩 불가");
                return false;
            }
        }

        // BlueZ GattManager 프록시 생성
        auto proxy = connection.createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
        );
        
        if (!proxy) {
            Logger::error("BlueZ GattManager 프록시 생성 실패");
            return false;
        }
        
        // 빈 옵션 딕셔너리 생성
        std::map<std::string, sdbus::Variant> options;
        
        // RegisterApplication 메서드 호출
        try {
            proxy->callMethod("RegisterApplication")
                .onInterface(sdbus::InterfaceName{BlueZConstants::GATT_MANAGER_INTERFACE})
                .withArguments(sdbus::ObjectPath{object.getPath()}, options);
            
            // 애플리케이션이 등록된 후 모든 서비스 개별 등록
            if (!registerServices()) {
                Logger::error("서비스 등록 실패");
                return false;
            }
            
            boundToBlueZ = true;
            Logger::info("BlueZ에 GATT 애플리케이션 바인딩 성공");
            return true;
        } catch (const sdbus::Error& e) {
            std::string errorName = e.getName();
            
            // "AlreadyExists" 오류는 성공으로 간주
            if (errorName.find("AlreadyExists") != std::string::npos) {
                Logger::info("애플리케이션이 이미 BlueZ에 바인딩됨 (AlreadyExists)");
                
                // 이미 등록되어 있어도 서비스는 등록
                if (!registerServices()) {
                    Logger::error("서비스 등록 실패");
                    return false;
                }
                
                boundToBlueZ = true;
                return true;
            }
            
            Logger::error("BlueZ 애플리케이션 바인딩 실패: " + errorName + " - " + e.getMessage());
            
            // 먼저 바인딩 해제 시도 후 재시도
            try {
                unbindFromBlueZ();
                
                // 잠시 대기
                usleep(500000); // 500ms
                
                // 다시 바인딩 시도
                proxy->callMethod("RegisterApplication")
                    .onInterface(sdbus::InterfaceName{BlueZConstants::GATT_MANAGER_INTERFACE})
                    .withArguments(sdbus::ObjectPath{object.getPath()}, options);
                
                // 애플리케이션 등록 후 서비스 등록
                if (!registerServices()) {
                    Logger::error("서비스 등록 실패");
                    return false;
                }
                
                boundToBlueZ = true;
                Logger::info("재시도 후 BlueZ에 GATT 애플리케이션 바인딩 성공");
                return true;
            } catch (const sdbus::Error& retryEx) {
                Logger::error("재시도 시 BlueZ 바인딩 실패: " + std::string(retryEx.what()));
                return false;
            }
        }
    } catch (const std::exception& e) {
        Logger::error("bindToBlueZ 예외: " + std::string(e.what()));
        return false;
    }
}

bool GattApplication::registerServices() {
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    bool success = true;
    for (auto& service : services) {
        if (!service) continue;
        
        // 각 서비스에 대해 인터페이스 설정
        if (!service->isInterfaceSetup()) {
            if (!service->setupInterfaces()) {
                Logger::error("서비스 인터페이스 설정 실패: " + service->getUuid().toString());
                success = false;
                continue;
            }
        }
        
        // 서비스 객체를 D-Bus에 등록
        if (!service->registerObject()) {
            Logger::error("서비스 객체 등록 실패: " + service->getUuid().toString());
            success = false;
            continue;
        }
        
        // InterfacesAdded 시그널 발생
        sdbus::ObjectPath servicePath(service->getPath());
        std::map<std::string, std::map<std::string, sdbus::Variant>> serviceInterfaces;
        
        // 서비스 인터페이스 정보 생성
        std::map<std::string, sdbus::Variant> properties;
        properties["UUID"] = sdbus::Variant(service->getUuid().toBlueZFormat());
        properties["Primary"] = sdbus::Variant(service->isPrimary());
        
        serviceInterfaces[BlueZConstants::GATT_SERVICE_INTERFACE] = properties;
        
        // InterfacesAdded 시그널 발생
        object.emitSignal(
            sdbus::SignalName{"InterfacesAdded"},
            sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
            servicePath,
            serviceInterfaces
        );
        
        Logger::info("서비스 등록 성공: " + service->getUuid().toString());
    }
    
    return success;
}

bool GattApplication::unbindFromBlueZ() {
    // 바인딩되지 않은 경우 할 일 없음
    if (!boundToBlueZ) {
        Logger::debug("애플리케이션이 BlueZ에 바인딩되지 않음, 해제할 것 없음");
        return true;
    }
    
    try {
        // 서비스 언레지스터
        unregisterServices();
        
        // BlueZ GattManager 프록시 생성
        auto proxy = connection.createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
        );
        
        if (!proxy) {
            Logger::error("BlueZ GattManager 프록시 생성 실패");
            boundToBlueZ = false; // 재시도 방지를 위해 상태 업데이트
            return false;
        }
        
        // UnregisterApplication 호출
        try {
            proxy->callMethod("UnregisterApplication")
                .onInterface(sdbus::InterfaceName{BlueZConstants::GATT_MANAGER_INTERFACE})
                .withArguments(sdbus::ObjectPath{object.getPath()});
            
            Logger::info("BlueZ에서 애플리케이션 바인딩 해제 성공");
        } catch (const sdbus::Error& e) {
            // DoesNotExist 오류는 괜찮음
            std::string error = e.getName();
            if (error.find("DoesNotExist") != std::string::npos) {
                Logger::info("애플리케이션이 이미 BlueZ에서 바인딩 해제됨");
            } else {
                Logger::warn("BlueZ에서 바인딩 해제 실패: " + error);
            }
        }
        
        boundToBlueZ = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("unbindFromBlueZ 예외: " + std::string(e.what()));
        boundToBlueZ = false;  // 상태 업데이트
        return false;
    }
}

void GattApplication::unregisterServices() {
    std::lock_guard<std::mutex> lock(servicesMutex);
    
    for (auto& service : services) {
        if (!service) continue;
        
        // InterfacesRemoved 시그널 발생
        sdbus::ObjectPath servicePath(service->getPath());
        std::vector<sdbus::InterfaceName> interfaceNames = {
            sdbus::InterfaceName{BlueZConstants::GATT_SERVICE_INTERFACE}
        };
        
        // D-Bus에서 객체 등록 해제
        service->unregisterObject();
        
        // InterfacesRemoved 시그널 발생
        object.emitSignal(
            sdbus::SignalName{"InterfacesRemoved"},
            sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
            servicePath,
            interfaceNames
        );
        
        Logger::info("서비스 등록 해제 완료: " + service->getUuid().toString());
    }
}

bool GattApplication::addService(GattServicePtr service) {
    if (!service) {
        Logger::error("null 서비스를 추가할 수 없음");
        return false;
    }
    
    std::string uuidStr = service->getUuid().toString();
    
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
    
    // 이미 BlueZ에 바인딩된 상태인 경우, 즉시 서비스 등록
    if (boundToBlueZ && interfaceSetup) {
        // 서비스 인터페이스 설정
        if (!service->setupInterfaces()) {
            Logger::error("서비스 인터페이스 설정 실패: " + uuidStr);
            return false;
        }
        
        // 서비스 객체 등록
        if (!service->registerObject()) {
            Logger::error("서비스 객체 등록 실패: " + uuidStr);
            return false;
        }
        
        // InterfacesAdded 시그널 발생
        sdbus::ObjectPath servicePath(service->getPath());
        std::map<std::string, std::map<std::string, sdbus::Variant>> serviceInterfaces;
        
        // 서비스 인터페이스 정보 생성
        std::map<std::string, sdbus::Variant> properties;
        properties["UUID"] = sdbus::Variant(service->getUuid().toBlueZFormat());
        properties["Primary"] = sdbus::Variant(service->isPrimary());
        
        serviceInterfaces[BlueZConstants::GATT_SERVICE_INTERFACE] = properties;
        
        // InterfacesAdded 시그널 발생
        object.emitSignal(
            sdbus::SignalName{"InterfacesAdded"},
            sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
            servicePath,
            serviceInterfaces
        );
    }
    
    Logger::info("애플리케이션에 서비스 추가됨: " + uuidStr);
    return true;
}

bool GattApplication::removeService(const GattUuid& uuid) {
    std::string uuidStr = uuid.toString();
    GattServicePtr serviceToRemove;
    
    {
        std::lock_guard<std::mutex> lock(servicesMutex);
        
        for (auto it = services.begin(); it != services.end(); ++it) {
            if ((*it)->getUuid().toString() == uuidStr) {
                serviceToRemove = *it; // 서비스 저장
                services.erase(it);     // 목록에서 제거
                break;
            }
        }
        
        if (!serviceToRemove) {
            Logger::warn("서비스를 찾을 수 없음: " + uuidStr);
            return false;
        }
    }
    
    // 바인딩된 상태인 경우 서비스 등록 해제
    if (boundToBlueZ && interfaceSetup && serviceToRemove) {
        // InterfacesRemoved 시그널 발생
        sdbus::ObjectPath servicePath(serviceToRemove->getPath());
        std::vector<sdbus::InterfaceName> interfaceNames = {
            sdbus::InterfaceName{BlueZConstants::GATT_SERVICE_INTERFACE}
        };
        
        // D-Bus에서 객체 등록 해제
        serviceToRemove->unregisterObject();
        
        // InterfacesRemoved 시그널 발생
        object.emitSignal(
            sdbus::SignalName{"InterfacesRemoved"},
            sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
            servicePath,
            interfaceNames
        );
    }
    
    Logger::info("애플리케이션에서 서비스 제거됨: " + uuidStr);
    return true;
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

bool GattApplication::registerObject() {
    return object.registerObject();
}

bool GattApplication::unregisterObject() {
    return object.unregisterObject();
}

ManagedObjectsDict GattApplication::createManagedObjectsDict() {
    ManagedObjectsDict result;
    
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