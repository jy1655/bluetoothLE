// include/GattTypes.h
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace ggk {

/**
 * @brief GATT 속성 플래그
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
 * @brief UUID 관리 클래스
 */
class GattUuid {
public:
    explicit GattUuid(const std::string& uuid) : m_uuid(uuid) {}
    
    static GattUuid fromShortUuid(uint16_t uuid) {
        char buffer[37];
        snprintf(buffer, sizeof(buffer), "0000%04x-0000-1000-8000-00805f9b34fb", uuid);
        return GattUuid(buffer);
    }
    
    std::string toString() const {
        return m_uuid;
    }
    
    std::string toBlueZFormat() const {
        // BlueZ는 하이픈 없는 형식을 사용하기도 하고, 하이픈 있는 형식을 사용하기도 함
        // 최신 버전은 하이픈 있는 형식을 선호
        return m_uuid;
    }
    
private:
    std::string m_uuid;
};

} // namespace ggk