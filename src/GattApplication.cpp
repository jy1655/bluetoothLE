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
        // 여기서는 아직 addObjectManager()를 호출하지 않음
        // 모든 서비스, 특성, 설명자를 먼저 생성한 후에 호출
        
        // Battery 서비스 생성
        if (!createBatteryService()) {
            std::cerr << "배터리 서비스 생성 실패" << std::endl;
            return false;
        }
        
        // 이제 모든 객체가 생성된 후 ObjectManager 추가
        m_appObject->addObjectManager();

        
        
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
    
    // 배터리 레벨 특성 UUID (0x2A19)
    GattUuid batteryLevelUuid = GattUuid::fromShortUuid(0x2A19);
    
    // 1. 배터리 서비스 객체 생성
    std::string servicePath = m_path + "/service1";
    auto service = std::make_shared<GattService>(
        m_connection,
        servicePath,
        batteryServiceUuid,
        true  // Primary
    );
    
    // 2. 배터리 레벨 특성 생성
    std::string charPath = servicePath + "/char1";
    auto characteristic = std::make_shared<GattCharacteristic>(
        m_connection,
        charPath,
        batteryLevelUuid,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        servicePath
    );
    
    // 초기 배터리 값 설정
    characteristic->setValue({80}); // 80% 배터리 레벨
    
    // 컨테이너에 저장
    m_services.push_back(service);
    m_characteristics.push_back(characteristic);
    
    std::cout << "배터리 서비스 생성됨: " << servicePath << std::endl;
    return true;
}

bool GattApplication::setupAdvertisement(const std::string& name) {
    // 광고 객체 생성
    std::string advPath = m_path + "/advertisement";
    auto advertisement = std::make_shared<LEAdvertisement>(
        m_connection,
        advPath,
        name
    );
    
    // 서비스 UUID 추가
    for (const auto& service : m_services) {
        auto gattService = std::static_pointer_cast<GattService>(service);
        advertisement->addServiceUUID(gattService->getUuid());
    }
    
    m_advertisement = advertisement;
    
    std::cout << "광고 설정됨: " << advPath << std::endl;
    return true;
}

bool GattApplication::registerWithBlueZ() {
    if (m_registered) return true;
    
    try {
        // 1. GATT 애플리케이션 등록
        auto proxy = sdbus::createProxy(m_connection, 
                                       sdbus::ServiceName("org.bluez"), 
                                       sdbus::ObjectPath("/org/bluez/hci0"));
        
        std::map<std::string, sdbus::Variant> options;
        
        // 경로를 문자열 대신 ObjectPath로 전달 (중요 수정 부분)
        proxy->callMethod("RegisterApplication")
             .onInterface("org.bluez.GattManager1")
             .withArguments(sdbus::ObjectPath(m_path), options);
        
        // 2. LE 광고 등록 (여기도 수정 필요)
        if (m_advertisement) {
            auto advProxy = sdbus::createProxy(m_connection, 
                                              sdbus::ServiceName("org.bluez"), 
                                              sdbus::ObjectPath("/org/bluez/hci0"));
            
            auto advertisement = std::static_pointer_cast<LEAdvertisement>(m_advertisement);
            std::map<std::string, sdbus::Variant> advOptions;
            
            // 여기도 ObjectPath로 전달
            advProxy->callMethod("RegisterAdvertisement")
                    .onInterface("org.bluez.LEAdvertisingManager1")
                    .withArguments(sdbus::ObjectPath(advertisement->getPath()), advOptions);
        }
        
        m_registered = true;
        std::cout << "BlueZ에 등록 완료" << std::endl;
        return true;
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
            
            // ObjectPath로 전달
            advProxy->callMethod("UnregisterAdvertisement")
                    .onInterface("org.bluez.LEAdvertisingManager1")
                    .withArguments(sdbus::ObjectPath(advertisement->getPath()));
        }
        
        // 2. GATT 애플리케이션 등록 해제
        auto proxy = sdbus::createProxy(m_connection, 
                                        sdbus::ServiceName("org.bluez"), 
                                        sdbus::ObjectPath("/org/bluez/hci0"));
        
        // ObjectPath로 전달
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