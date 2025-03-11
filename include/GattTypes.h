// GattTypes.h
#pragma once
#include <string>
#include <vector>
#include <functional>

namespace ggk {

// UUID 표현 클래스
class GattUuid {
public:
    // 128비트 UUID 생성
    explicit GattUuid(const std::string& uuid);
    
    // 16비트 UUID에서 변환 (표준 BT UUID)
    static GattUuid fromShortUuid(uint16_t uuid);
    
    // 문자열 변환
    std::string toString() const;
    
    // BlueZ에서 사용하는 형식으로 반환
    std::string toBlueZFormat() const;
    std::string toBlueZShortFormat() const;
    
private:
    std::string uuid;
    bool validate();
};

// GATT 권한 플래그
enum class GattPermission {
    READ = 0x01,
    WRITE = 0x02,
    READ_ENCRYPTED = 0x04,
    WRITE_ENCRYPTED = 0x08,
    READ_AUTHENTICATED = 0x10,
    WRITE_AUTHENTICATED = 0x20
};

// GATT 특성 속성 플래그
enum class GattProperty {
    BROADCAST = 0x01,
    READ = 0x02,
    WRITE_WITHOUT_RESPONSE = 0x04,
    WRITE = 0x08,
    NOTIFY = 0x10,
    INDICATE = 0x20,
    AUTHENTICATED_SIGNED_WRITES = 0x40,
    EXTENDED_PROPERTIES = 0x80
};

// GATT 설명자 타입 상수
struct GattDescriptorType {
    static const std::string CHARACTERISTIC_EXTENDED_PROPERTIES;
    static const std::string CHARACTERISTIC_USER_DESCRIPTION;
    static const std::string CLIENT_CHARACTERISTIC_CONFIGURATION;
    static const std::string SERVER_CHARACTERISTIC_CONFIGURATION;
    static const std::string CHARACTERISTIC_PRESENTATION_FORMAT;
    static const std::string CHARACTERISTIC_AGGREGATE_FORMAT;
    static const std::string VALID_RANGE;
    static const std::string EXTERNAL_REPORT_REFERENCE;
    static const std::string REPORT_REFERENCE;
};

// 기본 데이터 타입 관련 정의
using GattData = std::vector<uint8_t>;

} // namespace ggk