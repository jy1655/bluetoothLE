// src/GattAdvertisement.cpp
#include "GattAdvertisement.h"
#include "Logger.h"
#include <unistd.h>

namespace ggk {

GattAdvertisement::GattAdvertisement(
    SDBusConnection& connection,
    const std::string& path,
    AdvertisementType type)
    : connection(connection),
      object(connection, path),
      type(type),
      appearance(0),
      duration(0),
      includeTxPower(false),
      discoverable(true), 
      interfaceSetup(false),
      boundToBlueZ(false) {
}

void GattAdvertisement::addServiceUUID(const GattUuid& uuid) {
    if (!uuid.toString().empty()) {
        for (const auto& existingUuid : serviceUUIDs) {
            if (existingUuid.toString() == uuid.toString()) {
                return; // UUID 이미 존재함
            }
        }
        serviceUUIDs.push_back(uuid);
        Logger::debug("광고에 서비스 UUID 추가됨: " + uuid.toString());
        
        // BlueZ 5.82 호환성: 서비스 UUID 추가 시 includes에도 추가
        addInclude("service-uuids");
    }
}

void GattAdvertisement::addServiceUUIDs(const std::vector<GattUuid>& uuids) {
    for (const auto& uuid : uuids) {
        addServiceUUID(uuid);
    }
}

void GattAdvertisement::setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data) {
    manufacturerData[manufacturerId] = data;
    Logger::debug("ID 0x" + std::to_string(manufacturerId) + "에 대한 제조사 데이터 설정됨");
    
    // BlueZ 5.82 호환성: 제조사 데이터 추가 시 includes에도 추가
    addInclude("manufacturer-data");
}

void GattAdvertisement::setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data) {
    if (!serviceUuid.toString().empty()) {
        serviceData[serviceUuid] = data;
        Logger::debug("UUID에 대한 서비스 데이터 설정됨: " + serviceUuid.toString());
        
        // BlueZ 5.82 호환성: 서비스 데이터 추가 시 includes에도 추가
        addInclude("service-data");
    }
}

void GattAdvertisement::setLocalName(const std::string& name) {
    localName = name;
    Logger::debug("로컬 이름 설정됨: " + name);
    
    // BlueZ 5.82 호환성: LocalName 설정 시 includes에도 추가
    if (!name.empty()) {
        addInclude("local-name");
    }
}

void GattAdvertisement::setDiscoverable(bool value) {
    discoverable = value;
    Logger::debug("발견 가능 설정됨: " + std::string(value ? "true" : "false"));
}

void GattAdvertisement::setAppearance(uint16_t value) {
    appearance = value;
    Logger::debug("외관 설정됨: 0x" + std::to_string(value));
    
    // BlueZ 5.82 호환성: Appearance 설정 시 includes에도 추가
    if (value != 0) {
        addInclude("appearance");
    }
}

void GattAdvertisement::setDuration(uint16_t value) {
    duration = value;
    Logger::debug("광고 지속 시간 설정됨: " + std::to_string(value) + " 초");
}

void GattAdvertisement::setIncludeTxPower(bool include) {
    includeTxPower = include;
    Logger::debug("TX 전력 포함 설정됨: " + std::string(include ? "true" : "false"));
    
    // BlueZ 5.82 호환성: TX Power 설정 시 includes에도 추가
    if (include) {
        addInclude("tx-power");
    }
}

void GattAdvertisement::addInclude(const std::string& item) {
    // 이미 포함되어 있는지 확인
    for (const auto& existing : includes) {
        if (existing == item) {
            return; // 중복 방지
        }
    }
    
    includes.push_back(item);
    Logger::debug("include 항목 추가됨: " + item);
}

void GattAdvertisement::setIncludes(const std::vector<std::string>& items) {
    includes.clear();
    for (const auto& item : items) {
        addInclude(item);
    }
    Logger::debug(std::to_string(includes.size()) + " 항목으로 includes 배열 설정됨");
}

bool GattAdvertisement::setupInterfaces() {
    if (interfaceSetup) return true;
    
    Logger::info("광고 인터페이스 설정 - 경로: " + object.getPath());
    
    // BlueZ 5.82 호환성 확보
    ensureBlueZ582Compatibility();
    
    try {
        auto& sdbusObj = object.getSdbusObject();
        sdbus::InterfaceName interfaceName{BlueZConstants::LE_ADVERTISEMENT_INTERFACE};
        
        // Type 속성 vtable
        auto typeVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_TYPE})
                            .withGetter([this](){ return getTypeProperty(); });
        
        // ServiceUUIDs 속성 vtable (있는 경우)
        auto serviceUUIDsVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_SERVICE_UUIDS})
                                   .withGetter([this](){ return getServiceUUIDsProperty(); });
        
        // ManufacturerData 속성 vtable (있는 경우)
        auto manufacturerDataVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_MANUFACTURER_DATA})
                                       .withGetter([this](){ return getManufacturerDataProperty(); });
        
        // ServiceData 속성 vtable (있는 경우)
        auto serviceDataVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_SERVICE_DATA})
                                  .withGetter([this](){ return getServiceDataProperty(); });
        
        // LocalName 속성 vtable (있는 경우)
        auto localNameVTable = !localName.empty() ? 
                               sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_LOCAL_NAME})
                                 .withGetter([this](){ return localName; }) : 
                               sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_LOCAL_NAME})
                                 .withGetter([](){ return std::string(); });
        
        // Appearance 속성 vtable (있는 경우)
        auto appearanceVTable = appearance != 0 ? 
                                sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_APPEARANCE})
                                  .withGetter([this](){ return appearance; }) : 
                                sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_APPEARANCE})
                                  .withGetter([](){ return uint16_t(0); });
        
        // Duration 속성 vtable (있는 경우)
        auto durationVTable = duration != 0 ? 
                              sdbus::registerProperty(sdbus::PropertyName{"Duration"})
                                .withGetter([this](){ return duration; }) : 
                              sdbus::registerProperty(sdbus::PropertyName{"Duration"})
                                .withGetter([](){ return uint16_t(0); });
        
        // IncludeTxPower 속성 vtable
        auto includeTxPowerVTable = sdbus::registerProperty(sdbus::PropertyName{BlueZConstants::PROPERTY_INCLUDE_TX_POWER})
                                     .withGetter([this](){ return includeTxPower; });
        
        // Discoverable 속성 vtable (BlueZ 5.82에서는 필수)
        auto discoverableVTable = sdbus::registerProperty(sdbus::PropertyName{"Discoverable"})
                                   .withGetter([this](){ return discoverable; });
        
        // Includes 속성 vtable (BlueZ 5.82에서는 중요)
        auto includesVTable = sdbus::registerProperty(sdbus::PropertyName{"Includes"})
                               .withGetter([this](){ return includes; });
        
        // Release 메서드 vtable
        auto releaseVTable = sdbus::registerMethod(sdbus::MethodName{"Release"})
                              .implementedAs([this](){ handleRelease(); });
        
        // 모든 vtable 등록
        sdbusObj.addVTable(
            typeVTable, serviceUUIDsVTable, manufacturerDataVTable, serviceDataVTable,
            localNameVTable, appearanceVTable, durationVTable, includeTxPowerVTable,
            discoverableVTable, includesVTable, releaseVTable
        ).forInterface(interfaceName);
        
        // 설정 완료 표시
        interfaceSetup = true;
        
        // 객체 직접 등록 (BlueZ 5.82에서는 수동으로 각 객체 등록)
        if (!object.registerObject()) {
            Logger::error("광고 객체 D-Bus 등록 실패");
            return false;
        }
        
        Logger::info("광고 인터페이스 설정 완료");
        return true;
    } catch (const std::exception& e) {
        Logger::error("광고 인터페이스 설정 실패: " + std::string(e.what()));
        return false;
    }
}

bool GattAdvertisement::bindToBlueZ() {
    if (boundToBlueZ) return true;
    
    try {
        Logger::info("BlueZ에 GATT 광고 바인딩 중");
        
        // 인터페이스가 설정되지 않은 경우 먼저 설정
        if (!interfaceSetup) {
            if (!setupInterfaces()) {
                Logger::error("인터페이스 설정 실패로 BlueZ 바인딩 불가");
                return false;
            }
        }
        
        // BlueZ LEAdvertisingManager 프록시 생성
        auto proxy = connection.createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
        );
        
        if (!proxy) {
            Logger::error("BlueZ LEAdvertisingManager 프록시 생성 실패");
            return false;
        }
        
        // 빈 옵션 딕셔너리 생성
        std::map<std::string, sdbus::Variant> options;
        
        // RegisterAdvertisement 호출
        try {
            proxy->callMethod("RegisterAdvertisement")
                .onInterface(sdbus::InterfaceName{BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE})
                .withArguments(sdbus::ObjectPath{object.getPath()}, options);
            
            boundToBlueZ = true;
            Logger::info("BlueZ에 광고 등록 성공");
        } catch (const sdbus::Error& e) {
            std::string errorName = e.getName();
            std::string errorMsg = e.getMessage();
            
            Logger::error("BlueZ 광고 등록 실패: " + errorName + " - " + errorMsg);
            
            // 대체 광고 방법 시도
            if (tryAlternativeAdvertisingMethods()) {
                boundToBlueZ = true;
                Logger::info("대체 방법으로 광고 활성화됨");
                return true;
            }
            
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("bindToBlueZ 예외: " + std::string(e.what()));
        
        // 대체 광고 방법 시도
        if (tryAlternativeAdvertisingMethods()) {
            boundToBlueZ = true;
            Logger::info("예외 발생 후 대체 방법으로 광고 활성화됨");
            return true;
        }
        
        return false;
    }
}

bool GattAdvertisement::unbindFromBlueZ() {
    // 바인딩되지 않은 경우 할 일 없음
    if (!boundToBlueZ) {
        Logger::debug("광고가 BlueZ에 바인딩되지 않음, 해제할 것 없음");
        return true;
    }
    
    try {
        // BlueZ LEAdvertisingManager 프록시 생성
        auto proxy = connection.createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
        );
        
        if (!proxy) {
            Logger::error("BlueZ LEAdvertisingManager 프록시 생성 실패");
            boundToBlueZ = false; // 재시도 방지를 위해 상태 업데이트
            return false;
        }
        
        // UnregisterAdvertisement 호출
        try {
            proxy->callMethod("UnregisterAdvertisement")
                .onInterface(sdbus::InterfaceName{BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE})
                .withArguments(sdbus::ObjectPath{object.getPath()});
            
            Logger::info("BlueZ에서 광고 등록 해제 성공");
        } catch (const sdbus::Error& e) {
            // 오류는 무시해도 됨 (이미 해제되었거나 존재하지 않는 경우)
            Logger::warn("BlueZ에서 광고 등록 해제 중 오류: " + std::string(e.what()));
        }
        
        // 객체 등록 해제
        object.unregisterObject();
        
        // 대체 메서드로 광고 비활성화 시도 (안전을 위해)
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        
        boundToBlueZ = false;
        Logger::info("BlueZ에서 광고 바인딩 해제 성공");
        return true;
    } catch (const std::exception& e) {
        Logger::error("unbindFromBlueZ 예외: " + std::string(e.what()));
        boundToBlueZ = false;  // 안전을 위해 상태 업데이트
        return false;
    }
}

void GattAdvertisement::handleRelease() {
    Logger::info("BlueZ가 광고에 대해 Release 호출함: " + object.getPath());
    
    // 등록 상태 업데이트
    boundToBlueZ = false;
}

void GattAdvertisement::ensureBlueZ582Compatibility() {
    Logger::info("광고에 대한 BlueZ 5.82 호환성 확보 중...");

    // 필수 Includes 항목 확인 - BlueZ 5.82에서는 이 속성이 중요함
    // 이 속성은 광고에 포함할 항목을 명시적으로 지정함
    std::vector<std::string> requiredIncludes;
    
    // TX Power 확인
    if (includeTxPower && std::find(includes.begin(), includes.end(), "tx-power") == includes.end()) {
        requiredIncludes.push_back("tx-power");
    }
    
    // 로컬 이름 확인
    if (!localName.empty() && std::find(includes.begin(), includes.end(), "local-name") == includes.end()) {
        requiredIncludes.push_back("local-name");
    }
    
    // 외관 확인
    if (appearance != 0 && std::find(includes.begin(), includes.end(), "appearance") == includes.end()) {
        requiredIncludes.push_back("appearance");
    }
    
    // 서비스 UUID 확인
    if (!serviceUUIDs.empty() && std::find(includes.begin(), includes.end(), "service-uuids") == includes.end()) {
        requiredIncludes.push_back("service-uuids");
    }
    
    // 제조사 데이터 확인
    if (!manufacturerData.empty() && std::find(includes.begin(), includes.end(), "manufacturer-data") == includes.end()) {
        requiredIncludes.push_back("manufacturer-data");
    }
    
    // 서비스 데이터 확인
    if (!serviceData.empty() && std::find(includes.begin(), includes.end(), "service-data") == includes.end()) {
        requiredIncludes.push_back("service-data");
    }
    
    // 필요한 항목 추가
    for (const auto& item : requiredIncludes) {
        includes.push_back(item);
        Logger::debug("BlueZ 5.82용 필수 include 항목 추가됨: " + item);
    }
    
    // BlueZ 5.82에서는 Discoverable 속성이 필수
    discoverable = true;
    
    // 널 UUID 제거
    serviceUUIDs.erase(
        std::remove_if(serviceUUIDs.begin(), serviceUUIDs.end(), 
                      [](const GattUuid& uuid) { return uuid.toString().empty(); }),
        serviceUUIDs.end()
    );
    
    Logger::info("BlueZ 5.82 호환성 확보됨. Includes: " + std::to_string(includes.size()) + " 항목");
}

bool GattAdvertisement::tryAlternativeAdvertisingMethods() {
    Logger::info("대체 광고 방법 시도 중...");
    
    // 트래킹 성공 여부
    struct {
        bool bluetoothctl = false;
        bool hciconfig = false;
        bool btmgmt = false;
        bool direct_dbus = false;
    } methods;
    
    // 방법 1: bluetoothctl 사용 (가장 안정적)
    Logger::debug("방법 1: bluetoothctl을 통한 광고 활성화");
    try {
        methods.bluetoothctl = system("echo -e 'menu advertise\\non\\nback\\n' | bluetoothctl > /dev/null 2>&1") == 0;
        
        if (methods.bluetoothctl) {
            Logger::info("bluetoothctl을 통한 광고 활성화 성공");
            return true;
        }
    } catch (...) {
        Logger::warn("bluetoothctl 방법이 예외로 실패함");
    }
    
    // 방법 2: hciconfig 사용
    Logger::debug("방법 2: hciconfig를 통한 광고 활성화");
    try {
        methods.hciconfig = system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0;
        
        if (methods.hciconfig) {
            Logger::info("hciconfig를 통한 광고 활성화 성공");
            return true;
        }
    } catch (...) {
        Logger::warn("hciconfig 방법이 예외로 실패함");
    }
    
    // 방법 3: btmgmt 사용
    Logger::debug("방법 3: btmgmt를 통한 광고 활성화");
    try {
        int btmgmtResult = system("sudo btmgmt --index 0 power on > /dev/null 2>&1 && "
                             "sudo btmgmt --index 0 connectable on > /dev/null 2>&1 && "
                             "sudo btmgmt --index 0 discov on > /dev/null 2>&1 && "
                             "sudo btmgmt --index 0 advertising on > /dev/null 2>&1");
        methods.btmgmt = (btmgmtResult == 0);
        
        if (methods.btmgmt) {
            Logger::info("btmgmt를 통한 광고 활성화 성공");
            return true;
        }
    } catch (...) {
        Logger::warn("btmgmt 방법이 예외로 실패함");
    }
    
    // 방법 4: D-Bus 속성 직접 설정 (BlueZ 5.82 호환)
    Logger::debug("방법 4: D-Bus 속성을 통한 광고 활성화 시도");
    try {
        auto adapterProxy = connection.createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
        );
        
        if (adapterProxy) {
            // 어댑터 속성 설정
            adapterProxy->callMethod(sdbus::MethodName{"Set"})
                .onInterface(sdbus::InterfaceName{BlueZConstants::PROPERTIES_INTERFACE})
                .withArguments(std::string(BlueZConstants::ADAPTER_INTERFACE), 
                               std::string("Discoverable"), 
                               sdbus::Variant(true));
            
            usleep(200000); // 200ms
            
            // BlueZ 5.82에서 광고를 시작하기 위한 DiscoverableTimeout 설정
            adapterProxy->callMethod(sdbus::MethodName{"Set"})
                .onInterface(sdbus::InterfaceName{BlueZConstants::PROPERTIES_INTERFACE})
                .withArguments(std::string(BlueZConstants::ADAPTER_INTERFACE), 
                               std::string("DiscoverableTimeout"), 
                               sdbus::Variant(static_cast<uint16_t>(0)));
            
            methods.direct_dbus = true;
            Logger::info("D-Bus 속성을 통한 광고 활성화 성공");
            return true;
        }
    } catch (const std::exception& e) {
        Logger::warn("D-Bus 속성 방법이 예외로 실패함: " + std::string(e.what()));
    }
    
    // 모든 실패 로깅
    Logger::error("모든 광고 방법 실패: " +
                 std::string(methods.bluetoothctl ? "" : "bluetoothctl, ") +
                 std::string(methods.hciconfig ? "" : "hciconfig, ") +
                 std::string(methods.btmgmt ? "" : "btmgmt, ") +
                 std::string(methods.direct_dbus ? "" : "D-Bus 속성"));
    
    return false;
}

std::string GattAdvertisement::getTypeProperty() const {
    return (type == AdvertisementType::BROADCAST) ? 
            BlueZConstants::VALUE_BROADCAST : 
            BlueZConstants::VALUE_PERIPHERAL;
}

std::vector<std::string> GattAdvertisement::getServiceUUIDsProperty() const {
    std::vector<std::string> uuidStrings;
    for (const auto& uuid : serviceUUIDs) {
        uuidStrings.push_back(uuid.toBlueZFormat());
    }
    return uuidStrings;
}

std::map<uint16_t, sdbus::Variant> GattAdvertisement::getManufacturerDataProperty() const {
    std::map<uint16_t, sdbus::Variant> result;
    
    for (const auto& pair : manufacturerData) {
        uint16_t id = pair.first;
        const std::vector<uint8_t>& data = pair.second;
        
        result[id] = sdbus::Variant(data);
    }
    
    return result;
}

std::map<std::string, sdbus::Variant> GattAdvertisement::getServiceDataProperty() const {
    std::map<std::string, sdbus::Variant> result;
    
    for (const auto& pair : serviceData) {
        const GattUuid& uuid = pair.first;
        const std::vector<uint8_t>& data = pair.second;
        
        result[uuid.toBlueZFormat()] = sdbus::Variant(data);
    }
    
    return result;
}

} // namespace ggk