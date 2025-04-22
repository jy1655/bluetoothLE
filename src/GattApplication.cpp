// src/GattApplication.cpp
#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "GattAdvertisement.h"
#include <iostream>

namespace ggk {

GattApplication::GattApplication(sdbus::IConnection& connection, const std::string& path)
    : m_connection(connection), m_path(path) {
    
    // 애플리케이션 객체 생성
    m_appObject = sdbus::createObject(m_connection, sdbus::ObjectPath(m_path));
    std::cout << "GattApplication 생성됨: " << m_path << std::endl;
}

GattApplication::~GattApplication() {
    if (m_registered) {
        unregisterFromBlueZ();
    }
}

bool GattApplication::setupApplication() {
    if (!m_appObject) return false;

    try {
        // 1. 먼저 ObjectManager 추가
        m_appObject->addObjectManager();
        
        // 2. 모든 서비스, 특성, 설명자 생성
        // (여기서는 emitInterfacesAddedSignal을 그대로 사용)
        if (!createBatteryService()) {
            std::cerr << "배터리 서비스 생성 실패" << std::endl;
            return false;
        }
        
        std::cout << "GattApplication 설정 완료" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "애플리케이션 설정 실패: " << e.what() << std::endl;
        return false;
    }
}


bool GattApplication::createBatteryService() {
    // 배터리 서비스 UUID (0x180F)
    GattUuid batteryServiceUuid = GattUuid::fromShortUuid(0x180F);
    
    // 1. 배터리 서비스 객체 생성
    std::string servicePath = m_path + "/service0";
    auto service = std::make_shared<GattService>(
        m_connection,
        sdbus::ObjectPath(servicePath),
        batteryServiceUuid,
        true  // Primary
    );
    
    // 서비스 인터페이스 추가 신호 발생
    service->getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::GattService1_adaptor::INTERFACE_NAME}});
    
    // 2. 배터리 레벨 특성 (필수) - 단 하나만 생성
    GattUuid batteryLevelUuid = GattUuid::fromShortUuid(0x2A19);
    std::string charPath = servicePath + "/char0";
    auto characteristic = std::make_shared<GattCharacteristic>(
        m_connection,
        sdbus::ObjectPath(charPath),
        batteryLevelUuid,
        GattProperty::PROP_READ,  // 읽기만 가능하도록 단순화
        sdbus::ObjectPath(servicePath)
    );
    
    // 특성 인터페이스 추가 신호 발생
    characteristic->getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME}});
    
    // 초기 배터리 값 설정 (50%)
    characteristic->setValue({50});
    
    // 컨테이너에 저장
    m_services.push_back(service);
    m_characteristics.push_back(characteristic);
    
    std::cout << "최소 배터리 서비스 생성됨: " << servicePath << std::endl;
    return true;
}

bool GattApplication::setupAdvertisement(const std::string& name) {
    // 광고 객체 생성 (최소 구성)
    std::string advPath = m_path + "/adv0";
    auto advertisement = std::make_shared<LEAdvertisement>(
        m_connection,
        sdbus::ObjectPath(advPath),
        name
    );
    
    // 광고 인터페이스 추가 신호 발생
    advertisement->getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::LEAdvertisement1_adaptor::INTERFACE_NAME}});
    
    // 배터리 서비스 UUID만 추가
    if (!m_services.empty()) {
        auto gattService = std::static_pointer_cast<GattService>(m_services[0]);
        advertisement->addServiceUUID(gattService->getUuid());
    }
    
    m_advertisement = advertisement;
    
    std::cout << "기본 광고 설정됨: " << advPath << std::endl;
    return true;
}

bool GattApplication::registerWithBlueZ() {
    if (m_registered) return true;
    
    try {
        // 1. GATT 애플리케이션 등록 - 기본 옵션만 사용
        auto gattManagerProxy = sdbus::createProxy(
            m_connection, 
            sdbus::ServiceName("org.bluez"), 
            sdbus::ObjectPath("/org/bluez/hci0"));
        
        // 빈 옵션 맵으로 기본 등록만 시도
        std::map<std::string, sdbus::Variant> options;
        
        std::cout << "BlueZ에 애플리케이션 등록 시도: " << m_path << std::endl;
        
        gattManagerProxy->callMethod("RegisterApplication")
                        .onInterface("org.bluez.GattManager1")
                        .withArguments(sdbus::ObjectPath(m_path), options);
        
        std::cout << "애플리케이션 등록 성공" << std::endl;
        
        // 2. 광고 등록 - 기본 옵션만 사용
        if (m_advertisement) {
            auto leAdvManagerProxy = sdbus::createProxy(
                m_connection, 
                sdbus::ServiceName("org.bluez"), 
                sdbus::ObjectPath("/org/bluez/hci0"));
            
            auto advertisement = std::static_pointer_cast<LEAdvertisement>(m_advertisement);
            
            // 빈 옵션 맵으로 기본 등록만 시도
            std::map<std::string, sdbus::Variant> advOptions;
            
            std::cout << "BlueZ에 광고 등록 시도: " << advertisement->getPath() << std::endl;
            
            leAdvManagerProxy->callMethod("RegisterAdvertisement")
                             .onInterface("org.bluez.LEAdvertisingManager1")
                             .withArguments(sdbus::ObjectPath(advertisement->getPath()), advOptions);
            
            std::cout << "광고 등록 성공" << std::endl;
        }
        
        m_registered = true;
        std::cout << "BlueZ에 기본 등록 완료" << std::endl;
        return true;
    } catch (const sdbus::Error& e) {
        // 자세한 D-Bus 오류 정보 출력
        std::cerr << "BlueZ 등록 실패 - D-Bus 오류: " << e.getName() << " - " << e.getMessage() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "BlueZ 등록 실패: " << e.what() << std::endl;
        return false;
    }
}

bool GattApplication::unregisterFromBlueZ() {
    if (!m_registered) return true;
    
    try {
        // 1. LE 광고 등록 해제
        if (m_advertisement) {
            auto advProxy = sdbus::createProxy(m_connection, 
                                              sdbus::ServiceName("org.bluez"), 
                                              sdbus::ObjectPath("/org/bluez/hci0"));
            
            auto advertisement = std::static_pointer_cast<LEAdvertisement>(m_advertisement);
            
            advProxy->callMethod("UnregisterAdvertisement")
                    .onInterface("org.bluez.LEAdvertisingManager1")
                    .withArguments(sdbus::ObjectPath(advertisement->getPath()));
        }
        
        // 2. GATT 애플리케이션 등록 해제
        auto proxy = sdbus::createProxy(m_connection, 
                                        sdbus::ServiceName("org.bluez"), 
                                        sdbus::ObjectPath("/org/bluez/hci0"));
        
        proxy->callMethod("UnregisterApplication")
             .onInterface("org.bluez.GattManager1")
             .withArguments(sdbus::ObjectPath(m_path));
        
        m_registered = false;
        std::cout << "BlueZ에서 등록 해제됨" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "BlueZ 등록 해제 실패: " << e.what() << std::endl;
        return false;
    }
}

void GattApplication::run() {
    std::cout << "이벤트 루프 시작..." << std::endl;
    m_connection.enterEventLoopAsync();
}

} // namespace ggk