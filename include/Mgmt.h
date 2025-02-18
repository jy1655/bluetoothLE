#pragma once

#include <memory>
#include <string>
#include "HciAdapter.h"
#include "Utils.h"

namespace ggk {

class Mgmt {
public:
    // 상수 정의
    static const int kMaxAdvertisingNameLength = 248;
    static const int kMaxAdvertisingShortNameLength = 10;
    static const uint16_t kDefaultControllerIndex = 0;

    // 생성자에서 HciAdapter 인스턴스를 받도록 수정
    Mgmt(HciAdapter& adapter, uint16_t controllerIndex = kDefaultControllerIndex);

    // 기본 BLE 설정 메서드들
    bool setName(std::string name, std::string shortName);
    bool setDiscoverable(uint8_t disc, uint16_t timeout);
    bool setPowered(bool newState);
    bool setBredr(bool newState);
    bool setSecureConnections(uint8_t newState);
    bool setBondable(bool newState);
    bool setConnectable(bool newState);
    bool setLE(bool newState);
    bool setAdvertising(uint8_t newState);

    // 유틸리티 메서드
    static std::string truncateName(const std::string& name);
    static std::string truncateShortName(const std::string& name);

private:
    bool setState(uint16_t commandCode, uint16_t controllerId, uint8_t newState);
    
    HciAdapter& hciAdapter;  // unique_ptr 대신 참조로 변경
    uint16_t controllerIndex;
};

} // namespace ggk