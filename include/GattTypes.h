#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace {
    // Helper to convert hex character to integer
    uint8_t hexCharToInt(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }
}

/**
 * @brief GATT property flags
 */
enum GattProperty {
    PROP_BROADCAST                  = 0x01,
    PROP_READ                       = 0x02,
    PROP_WRITE_WITHOUT_RESPONSE     = 0x04,
    PROP_WRITE                      = 0x08,
    PROP_NOTIFY                     = 0x10,
    PROP_INDICATE                   = 0x20,
    PROP_AUTHENTICATED_SIGNED_WRITES = 0x40,
    PROP_EXTENDED_PROPERTIES        = 0x80
};

/**
 * @brief GATT permission flags
 */
enum GattPermission {
    PERM_READ                      = 0x01,
    PERM_WRITE                     = 0x02,
    PERM_READ_ENCRYPTED            = 0x04,
    PERM_WRITE_ENCRYPTED           = 0x08,
    PERM_READ_AUTHENTICATED        = 0x10,
    PERM_WRITE_AUTHENTICATED       = 0x20
};

/**
 * @class GattUuid
 * @brief Helper class for Bluetooth UUIDs
 */
class GattUuid {
public:
    /**
     * @brief Construct from full UUID string
     * @param uuid UUID string in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     */
    explicit GattUuid(const std::string& uuid) : m_uuid(uuid) {
        // Convert to lowercase
        for (auto& c : m_uuid) {
            if (c >= 'A' && c <= 'F') {
                c = c - 'A' + 'a';
            }
        }
    }
    
    /**
     * @brief Create a full UUID from a 16-bit UUID
     * @param uuid16 16-bit UUID
     * @return GattUuid instance with full UUID
     */
    static GattUuid fromShortUuid(uint16_t uuid16) {
        char buffer[37];
        snprintf(buffer, sizeof(buffer), "%08x-0000-1000-8000-00805f9b34fb", uuid16);
        return GattUuid(buffer);
    }
    
    /**
     * @brief Get string representation
     * @return UUID as string
     */
    std::string toString() const {
        return m_uuid;
    }
    
    /**
     * @brief Get UUID in format used by BlueZ
     * @return UUID in BlueZ format
     */
    std::string toBlueZFormat() const {
        return m_uuid;
    }
    
    /**
     * @brief Convert to byte array
     * @return UUID as byte array
     */
    std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> bytes;
        bytes.reserve(16);
        
        for (size_t i = 0; i < m_uuid.length(); ++i) {
            char c = m_uuid[i];
            if (c == '-') continue;
            
            uint8_t high = hexCharToInt(c);
            uint8_t low = hexCharToInt(m_uuid[++i]);
            bytes.push_back((high << 4) | low);
        }
        
        return bytes;
    }
    
    /**
     * @brief Check if two UUIDs are equal
     */
    bool operator==(const GattUuid& other) const {
        return m_uuid == other.m_uuid;
    }
    
    /**
     * @brief Check if two UUIDs are not equal
     */
    bool operator!=(const GattUuid& other) const {
        return m_uuid != other.m_uuid;
    }
    
private:
    std::string m_uuid;
};