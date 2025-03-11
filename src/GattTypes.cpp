#include "GattTypes.h"
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cstdio>

namespace ggk {

// GattDescriptorType 상수 정의
const std::string GattDescriptorType::CHARACTERISTIC_EXTENDED_PROPERTIES = "2900";
const std::string GattDescriptorType::CHARACTERISTIC_USER_DESCRIPTION = "2901";
const std::string GattDescriptorType::CLIENT_CHARACTERISTIC_CONFIGURATION = "2902";
const std::string GattDescriptorType::SERVER_CHARACTERISTIC_CONFIGURATION = "2903";
const std::string GattDescriptorType::CHARACTERISTIC_PRESENTATION_FORMAT = "2904";
const std::string GattDescriptorType::CHARACTERISTIC_AGGREGATE_FORMAT = "2905";
const std::string GattDescriptorType::VALID_RANGE = "2906";
const std::string GattDescriptorType::EXTERNAL_REPORT_REFERENCE = "2907";
const std::string GattDescriptorType::REPORT_REFERENCE = "2908";

// GattUuid 구현
GattUuid::GattUuid(const std::string& uuid) : uuid(uuid) {
    if (!validate()) {
        throw std::invalid_argument("Invalid UUID format: " + uuid);
    }
}

GattUuid GattUuid::fromShortUuid(uint16_t shortUuid) {
    // 블루투스 기본 UUID: 0000xxxx-0000-1000-8000-00805F9B34FB
    // xxxx는 16비트 UUID
    char buffer[37];
    snprintf(buffer, sizeof(buffer), "0000%04x-0000-1000-8000-00805f9b34fb", shortUuid);
    
    return GattUuid(buffer);
}

std::string GattUuid::toString() const {
    return uuid;
}

std::string GattUuid::toBlueZFormat() const {
    // BlueZ는 하이픈이 없는 형식을 사용
    std::string result = uuid;
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    
    // 모두 소문자로 변환 (일관성을 위해)
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    return result;
}

std::string GattUuid::toBlueZShortFormat() const {
    // BlueZ는 하이픈이 없는 형식을 사용
    std::string result = uuid;
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    
    // 모두 소문자로 변환
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    // 짧은 형식으로 반환 (처음 8자리)
    if (result.length() >= 8) {
        return result.substr(0, 8);
    }
    
    return result;
}



bool GattUuid::validate() {
    // 기본 UUID 유효성 검사 (간단한 구현)
    if (uuid.length() == 36 && 
        uuid[8] == '-' && uuid[13] == '-' && 
        uuid[18] == '-' && uuid[23] == '-') {
        return true;
    }
    
    // 하이픈 없는 32자 형식도 허용
    if (uuid.length() == 32) {
        // 유효한 16진수 문자만 포함되어 있는지 확인
        for (char c : uuid) {
            if (!isxdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    }
    
    return false;
}

} // namespace ggk