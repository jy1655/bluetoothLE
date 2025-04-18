// src/GattService.cpp
#include "GattService.h"
#include "Logger.h"

namespace ggk {

GattService::GattService(
    SDBusConnection& connection,
    const std::string& path,
    const GattUuid& uuid,
    bool isPrimary)
    : connection(connection),
      object(connection, path),
      uuid(uuid),
      primary(isPrimary),
      interfaceSetup(false),
      objectRegistered(false) {
}

GattCharacteristicPtr GattService::createCharacteristic(
    const GattUuid& uuid,
    uint8_t properties,
    uint8_t permissions)
{
    if (uuid.toString().empty()) {
        Logger::error("빈 UUID로 특성을 생성할 수 없음");
        return nullptr;
    }
    
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    // 이미 존재하는지 확인
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end()) {
        if (!it->second) {
            Logger::error("UUID에 대한 null 특성 항목 발견: " + uuidStr);
            characteristics.erase(it);
        } else {
            return it->second;
        }
    }
    
    try {
        // 표준화된 객체 경로 생성
        // 형식: <service_path>/char<uuid_short>
        std::string uuidShort = "/char" + uuid.toBlueZShortFormat().substr(0, 8);
        std::string charPath = getPath() + uuidShort;
        
        // 특성 생성 - this로 자신에 대한 포인터 전달 (약한 참조)
        auto characteristic = std::make_shared<GattCharacteristic>(
            connection,
            charPath,
            uuid,
            this,  // 부모 서비스 포인터
            properties,
            permissions
        );
        
        if (!characteristic) {
            Logger::error("UUID에 대한 특성 생성 실패: " + uuidStr);
            return nullptr;
        }
        
        // 맵에 추가
        characteristics[uuidStr] = characteristic;
        
        // 서비스가 이미 등록된 상태면 특성도 인터페이스 설정 및 등록
        if (objectRegistered) {
            if (!characteristic->setupInterfaces()) {
                Logger::error("특성 인터페이스 설정 실패: " + uuidStr);
                return nullptr;
            }
            
            if (!characteristic->registerObject()) {
                Logger::error("특성 객체 등록 실패: " + uuidStr);
                return nullptr;
            }
            
            // InterfacesAdded 시그널 발생 (부모 객체를 통해 전달)
            emitInterfacesAddedForCharacteristic(characteristic);
        }
        
        Logger::info("특성 생성됨: " + uuidStr + ", 경로: " + charPath);
        return characteristic;
    } catch (const std::exception& e) {
        Logger::error("특성 생성 중 예외: " + std::string(e.what()));
        return nullptr;
    }
}

GattCharacteristicPtr GattService::getCharacteristic(const GattUuid& uuid) const {
    std::string uuidStr = uuid.toString();
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    auto it = characteristics.find(uuidStr);
    if (it != characteristics.end() && it->second) {
        return it->second;
    }
    
    return nullptr;
}

bool GattService::setupInterfaces() {
    if (interfaceSetup) return true;
    
    try {
        auto& sdbusObj = object.getSdbusObject();
        sdbus::InterfaceName interfaceName{BlueZConstants::GATT_SERVICE_INTERFACE};
        
        // Primary 속성 등록
        auto primaryVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_PRIMARY})
                               .withGetter([this](){ return this->getPrimaryProperty(); });
        
        // UUID 속성 등록
        auto uuidVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_UUID})
                           .withGetter([this](){ return this->getUuidProperty(); });
        
        // Characteristics 속성 등록
        auto charsVTable = sdbus::registerProperty(sdbus::PropertyName{"Characteristics"})
                            .withGetter([this](){ return this->getCharacteristicsProperty(); });
        
        // 모든 vtable을 한번에 등록
        sdbusObj.addVTable(primaryVTable, uuidVTable, charsVTable)
               .forInterface(interfaceName);
        
        // 설정 완료 표시
        interfaceSetup = true;
        Logger::info("서비스 인터페이스 설정 완료: " + uuid.toString());
        return true;
    } catch (const std::exception& e) {
        Logger::error("서비스 인터페이스 설정 실패: " + std::string(e.what()));
        return false;
    }
}

bool GattService::registerObject() {
    if (objectRegistered) {
        return true;
    }
    
    // 인터페이스가 설정되지 않은 경우 먼저 설정
    if (!interfaceSetup) {
        if (!setupInterfaces()) {
            return false;
        }
    }
    
    // D-Bus에 등록
    if (!object.registerObject()) {
        Logger::error("서비스 객체 등록 실패: " + uuid.toString());
        return false;
    }
    
    objectRegistered = true;
    
    // 모든 특성에 대해 인터페이스 설정 및 등록
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    for (const auto& [uuid, characteristic] : characteristics) {
        if (!characteristic) continue;
        
        // 인터페이스 설정
        if (!characteristic->setupInterfaces()) {
            Logger::error("특성 인터페이스 설정 실패: " + uuid);
            continue;
        }
        
        // 객체 등록
        if (!characteristic->registerObject()) {
            Logger::error("특성 객체 등록 실패: " + uuid);
            continue;
        }
        
        // InterfacesAdded 시그널 발생
        emitInterfacesAddedForCharacteristic(characteristic);
    }
    
    Logger::info("서비스 객체 등록 완료: " + uuid.toString());
    return true;
}

bool GattService::unregisterObject() {
    if (!objectRegistered) {
        return true;
    }
    
    // 모든 특성 먼저 등록 해제
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    for (const auto& [uuid, characteristic] : characteristics) {
        if (!characteristic) continue;
        
        // 특성 등록 해제 전 InterfacesRemoved 시그널 발생
        emitInterfacesRemovedForCharacteristic(characteristic);
        
        // 객체 등록 해제
        characteristic->unregisterObject();
    }
    
    // 서비스 객체 등록 해제
    if (!object.unregisterObject()) {
        Logger::error("서비스 객체 등록 해제 실패: " + uuid.toString());
        return false;
    }
    
    objectRegistered = false;
    Logger::info("서비스 객체 등록 해제 완료: " + uuid.toString());
    return true;
}

void GattService::emitInterfacesAddedForCharacteristic(GattCharacteristicPtr characteristic) {
    if (!characteristic) return;
    
    // 상위 경로 (애플리케이션 경로) 추출
    std::string servicePath = getPath();
    std::string appPath = servicePath.substr(0, servicePath.find_last_of('/'));
    
    // 올바른 경로인지 확인
    if (appPath.empty() || appPath == servicePath) {
        appPath = "/";  // 루트 경로로 기본 설정
    }
    
    // 애플리케이션 객체 생성
    SDBusObject appObject(connection, appPath);
    
    // 특성의 인터페이스 정보 생성
    sdbus::ObjectPath charPath(characteristic->getPath());
    std::map<std::string, std::map<std::string, sdbus::Variant>> charInterfaces;
    std::map<std::string, sdbus::Variant> charProperties;
    
    // UUID 속성
    charProperties["UUID"] = sdbus::Variant(characteristic->getUuid().toBlueZFormat());
    
    // Service 속성
    charProperties["Service"] = sdbus::Variant(sdbus::ObjectPath(getPath()));
    
    // Flags 속성
    std::vector<std::string> flags;
    uint8_t props = characteristic->getProperties();
    if (props & GattProperty::PROP_BROADCAST)
        flags.push_back(BlueZConstants::FLAG_BROADCAST);
    if (props & GattProperty::PROP_READ)
        flags.push_back(BlueZConstants::FLAG_READ);
    if (props & GattProperty::PROP_WRITE_WITHOUT_RESPONSE)
        flags.push_back(BlueZConstants::FLAG_WRITE_WITHOUT_RESPONSE);
    if (props & GattProperty::PROP_WRITE)
        flags.push_back(BlueZConstants::FLAG_WRITE);
    if (props & GattProperty::PROP_NOTIFY)
        flags.push_back(BlueZConstants::FLAG_NOTIFY);
    if (props & GattProperty::PROP_INDICATE)
        flags.push_back(BlueZConstants::FLAG_INDICATE);
    if (props & GattProperty::PROP_AUTHENTICATED_SIGNED_WRITES)
        flags.push_back(BlueZConstants::FLAG_AUTHENTICATED_SIGNED_WRITES);
    
    charProperties["Flags"] = sdbus::Variant(flags);
    
    // 인터페이스에 속성 추가
    charInterfaces[BlueZConstants::GATT_CHARACTERISTIC_INTERFACE] = charProperties;
    
    // InterfacesAdded 시그널 발생
    appObject.emitSignal(
        sdbus::SignalName{"InterfacesAdded"},
        sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
        charPath,
        charInterfaces
    );
}

void GattService::emitInterfacesRemovedForCharacteristic(GattCharacteristicPtr characteristic) {
    if (!characteristic) return;
    
    // 상위 경로 (애플리케이션 경로) 추출
    std::string servicePath = getPath();
    std::string appPath = servicePath.substr(0, servicePath.find_last_of('/'));
    
    // 올바른 경로인지 확인
    if (appPath.empty() || appPath == servicePath) {
        appPath = "/";  // 루트 경로로 기본 설정
    }
    
    // 애플리케이션 객체 생성
    SDBusObject appObject(connection, appPath);
    
    // 특성의 경로와 인터페이스 이름 설정
    sdbus::ObjectPath charPath(characteristic->getPath());
    std::vector<sdbus::InterfaceName> interfaceNames = {
        sdbus::InterfaceName{BlueZConstants::GATT_CHARACTERISTIC_INTERFACE}
    };
    
    // InterfacesRemoved 시그널 발생
    appObject.emitSignal(
        sdbus::SignalName{"InterfacesRemoved"},
        sdbus::InterfaceName{"org.freedesktop.DBus.ObjectManager"},
        charPath,
        interfaceNames
    );
}

std::string GattService::getUuidProperty() const {
    return uuid.toBlueZFormat();
}

bool GattService::getPrimaryProperty() const {
    return primary;
}

std::vector<sdbus::ObjectPath> GattService::getCharacteristicsProperty() const {
    std::vector<sdbus::ObjectPath> paths;
    
    std::lock_guard<std::mutex> lock(characteristicsMutex);
    
    for (const auto& pair : characteristics) {
        if (pair.second) {  // null 체크
            paths.push_back(sdbus::ObjectPath(pair.second->getPath()));
        }
    }
    
    return paths;
}

} // namespace ggk