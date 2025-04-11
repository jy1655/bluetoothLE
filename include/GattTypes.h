// GattTypes.h
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ggk {

/**
 * @brief UUID 표현 클래스
 */
class GattUuid {
public:
    /**
     * @brief 128비트 UUID 생성
     * @param uuid UUID 문자열 (형식: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
     */
    explicit GattUuid(const std::string& uuid);
    
    /**
     * @brief 16비트 UUID에서 변환 (표준 BT UUID)
     * @param uuid 16비트 UUID 값
     * @return 변환된 GattUuid 객체
     */
    static GattUuid fromShortUuid(uint16_t uuid);
    
    /**
     * @brief 문자열로 변환
     * @return 표준 형식 UUID 문자열
     */
    std::string toString() const;
    
    /**
     * @brief BlueZ에서 사용하는 형식으로 반환 (하이픈 없음)
     * @return BlueZ 형식 UUID 문자열
     */
    std::string toBlueZFormat() const;
    
    /**
     * @brief BlueZ 짧은 형식으로 반환 (16비트 UUID의 경우)
     * @return BlueZ 짧은 형식 UUID 문자열
     */
    std::string toBlueZShortFormat() const;
    
    /**
     * @brief 비교 연산자
     */
    bool operator<(const GattUuid& other) const;
    bool operator==(const GattUuid& other) const;
    bool operator!=(const GattUuid& other) const;
    
private:
    std::string uuid;
    bool validate();
};

/**
 * @brief GATT 권한 플래그
 */
enum GattPermission {
    PERM_READ = 0x01,
    PERM_WRITE = 0x02,
    PERM_READ_ENCRYPTED = 0x04,
    PERM_WRITE_ENCRYPTED = 0x08,
    PERM_READ_AUTHENTICATED = 0x10,
    PERM_WRITE_AUTHENTICATED = 0x20
};

/**
 * @brief GATT 특성 속성 플래그
 */
enum GattProperty {
    PROP_BROADCAST = 0x01,
    PROP_READ = 0x02,
    PROP_WRITE_WITHOUT_RESPONSE = 0x04,
    PROP_WRITE = 0x08,
    PROP_NOTIFY = 0x10,
    PROP_INDICATE = 0x20,
    PROP_AUTHENTICATED_SIGNED_WRITES = 0x40,
    PROP_EXTENDED_PROPERTIES = 0x80
};

/**
 * @brief GATT 설명자 타입 상수
 */
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

/**
 * @brief GATT 데이터 타입
 */
using GattData = std::vector<uint8_t>;

} // namespace ggk