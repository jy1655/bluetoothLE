// BleConstants.h
#pragma once

#include <string>

namespace ble {

// GATT property flags
enum GattProperty {
    PROP_BROADCAST                      = 0x01,
    PROP_READ                           = 0x02,
    PROP_WRITE_WITHOUT_RESPONSE         = 0x04,
    PROP_WRITE                          = 0x08,
    PROP_NOTIFY                         = 0x10,
    PROP_INDICATE                       = 0x20,
    PROP_AUTHENTICATED_SIGNED_WRITES    = 0x40,
    PROP_EXTENDED_PROPERTIES            = 0x80
};

// GATT permission flags
enum GattPermission {
    PERM_READ                 = 0x01,
    PERM_WRITE                = 0x02,
    PERM_READ_ENCRYPTED       = 0x04,
    PERM_WRITE_ENCRYPTED      = 0x08,
    PERM_READ_AUTHENTICATED   = 0x10,
    PERM_WRITE_AUTHENTICATED  = 0x20
};

// BlueZ interface names
namespace BlueZ {
    // Core interfaces
    [[maybe_unused]] static const char* GATT_MANAGER_INTERFACE      = "org.bluez.GattManager1";
    [[maybe_unused]] static const char* ADVERTISING_MANAGER         = "org.bluez.LEAdvertisingManager1";
    [[maybe_unused]] static const char* OBJECT_MANAGER_INTERFACE    = "org.freedesktop.DBus.ObjectManager";
    
    // Object interfaces
    [[maybe_unused]] static const char* GATT_SERVICE_INTERFACE      = "org.bluez.GattService1";
    [[maybe_unused]] static const char* GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
    [[maybe_unused]] static const char* GATT_DESCRIPTOR_INTERFACE   = "org.bluez.GattDescriptor1";
    [[maybe_unused]] static const char* ADVERTISEMENT_INTERFACE     = "org.bluez.LEAdvertisement1";
}

// UUID constants for BLE services and characteristics
namespace BleConstants {
    // Standard service UUIDs
    static const std::string BATTERY_SERVICE_UUID      = "0000180f-0000-1000-8000-00805f9b34fb";
    static const std::string DEVICE_INFO_SERVICE_UUID  = "0000180a-0000-1000-8000-00805f9b34fb";
    
    // Standard characteristic UUIDs
    static const std::string BATTERY_LEVEL_UUID        = "00002a19-0000-1000-8000-00805f9b34fb";
    static const std::string DEVICE_NAME_UUID          = "00002a00-0000-1000-8000-00805f9b34fb";
    
    // Standard descriptor UUIDs
    static const std::string CCCD_UUID                 = "00002902-0000-1000-8000-00805f9b34fb";
    
    // Custom service and characteristic UUIDs
    static const std::string CUSTOM_SERVICE_UUID       = "11111111-2222-3333-4444-555555555555";
    static const std::string CUSTOM_CHAR_UUID          = "11111111-2222-3333-4444-666666666666";
    
    // Helper function to convert short UUID (16-bit) to full UUID (128-bit)
    [[maybe_unused]] static std::string toFullUUID(uint16_t uuid) {
        char buffer[37];
        snprintf(buffer, sizeof(buffer), "0000%04x-0000-1000-8000-00805f9b34fb", uuid);
        return std::string(buffer);
    }
}

} // namespace ble