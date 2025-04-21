// include/GattApplication.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <memory>
#include <string>
#include "GattTypes.h"

namespace ggk {

class GattApplication {
public:
    // 생성자는 이미 생성된 연결과 객체를 받음
    GattApplication(sdbus::IConnection& connection, 
                    const std::string& path = "/com/example/ble");
    ~GattApplication();
    
    // 애플리케이션 설정하기
    bool setupApplication();
    
    // 서비스/특성/설명자 생성 메소드
    bool createBatteryService();
    
    // 광고 설정
    bool setupAdvertisement(const std::string& name = "BleBatteryDevice");
    
    // BlueZ에 등록
    bool registerWithBlueZ();
    
    // BlueZ에서 등록 해제
    bool unregisterFromBlueZ();
    
    // 이벤트 루프 실행
    void run();
    
private:
    sdbus::IConnection& m_connection;
    std::string m_path;
    std::shared_ptr<sdbus::IObject> m_appObject;
    
    // 서비스, 특성, 설명자 컨테이너
    std::vector<std::shared_ptr<void>> m_services;
    std::vector<std::shared_ptr<void>> m_characteristics;
    std::vector<std::shared_ptr<void>> m_descriptors;
    
    // 광고 객체
    std::shared_ptr<void> m_advertisement;
    
    bool m_registered = false;
};

} // namespace ggk