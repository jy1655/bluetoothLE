// GattTypes.h
#pragma once

#include <string>
#include <map>
#include <gio/gio.h>

namespace ggk {

// BlueZ GATT 인터페이스 상수
struct GattInterface {
    static constexpr const char* SERVICE = "org.bluez.GattService1";
    static constexpr const char* CHARACTERISTIC = "org.bluez.GattCharacteristic1";
    static constexpr const char* DESCRIPTOR = "org.bluez.GattDescriptor1";
};

// GATT 특성 플래그 (Characteristic Properties)
struct GattFlags {
    static constexpr uint32_t BROADCAST = 0x01;
    static constexpr uint32_t READ = 0x02;
    static constexpr uint32_t WRITE_WITHOUT_RESPONSE = 0x04;
    static constexpr uint32_t WRITE = 0x08;
    static constexpr uint32_t NOTIFY = 0x10;
    static constexpr uint32_t INDICATE = 0x20;
    static constexpr uint32_t SIGNED_WRITE = 0x40;
    static constexpr uint32_t EXTENDED_PROPERTIES = 0x80;

    // 보안 관련 플래그
    static constexpr uint32_t ENCRYPT_READ = 0x100;
    static constexpr uint32_t ENCRYPT_WRITE = 0x200;
    static constexpr uint32_t ENCRYPT_AUTHENTICATED_READ = 0x400;
    static constexpr uint32_t ENCRYPT_AUTHENTICATED_WRITE = 0x800;
    static constexpr uint32_t SECURE_READ = 0x1000;
    static constexpr uint32_t SECURE_WRITE = 0x2000;
    static constexpr uint32_t AUTHORIZE = 0x4000;
};

// GATT 서비스 정보
struct GattService {
    std::string uuid;
    bool isPrimary;
    std::string path;
    std::map<std::string, GVariant*> options;
};

// GATT 특성 정보
struct GattCharacteristic {
    std::string uuid;
    uint32_t flags;
    std::string path;
    std::string serviceUUID;
    std::map<std::string, GVariant*> options;
};

// GATT 디스크립터 정보
struct GattDescriptor {
    std::string uuid;
    uint32_t flags;
    std::string path;
    std::string characteristicUUID;
    std::map<std::string, GVariant*> options;
};

// GATT 작업 관련 구조체들
struct GattReadRequest {
    uint16_t offset;
    uint16_t mtu;
    std::string sender;
    std::map<std::string, GVariant*> options;
};

struct GattWriteRequest {
    std::vector<uint8_t> value;
    uint16_t offset;
    bool withResponse;
    std::string sender;
    std::map<std::string, GVariant*> options;
};

struct GattNotification {
    std::vector<uint8_t> value;
    bool indicate;  // true for indication, false for notification
    std::map<std::string, GVariant*> options;
};

} // namespace ggk