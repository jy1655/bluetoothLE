// BlueZConstants.h
#pragma once
#include <string>

namespace ggk {
namespace BlueZConstants {

// Service name
const std::string BLUEZ_SERVICE = "org.bluez";

// Common paths
const std::string ROOT_PATH = "/";
const std::string ADAPTER_PATH = "/org/bluez/hci0";

// Interfaces
const std::string OBJECT_MANAGER_INTERFACE = "org.freedesktop.DBus.ObjectManager";
const std::string PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
const std::string GATT_MANAGER_INTERFACE = "org.bluez.GattManager1";
const std::string LE_ADVERTISING_MANAGER_INTERFACE = "org.bluez.LEAdvertisingManager1";
const std::string GATT_SERVICE_INTERFACE = "org.bluez.GattService1";
const std::string GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
const std::string GATT_DESCRIPTOR_INTERFACE = "org.bluez.GattDescriptor1";
const std::string LE_ADVERTISEMENT_INTERFACE = "org.bluez.LEAdvertisement1";
const std::string DEVICE_INTERFACE = "org.bluez.Device1";

// Methods
const std::string REGISTER_APPLICATION = "RegisterApplication";
const std::string UNREGISTER_APPLICATION = "UnregisterApplication";
const std::string REGISTER_ADVERTISEMENT = "RegisterAdvertisement";
const std::string UNREGISTER_ADVERTISEMENT = "UnregisterAdvertisement";
const std::string GET_MANAGED_OBJECTS = "GetManagedObjects";
const std::string READ_VALUE = "ReadValue";
const std::string WRITE_VALUE = "WriteValue";
const std::string START_NOTIFY = "StartNotify";
const std::string STOP_NOTIFY = "StopNotify";

// Property names
const std::string PROPERTY_UUID = "UUID";
const std::string PROPERTY_TYPE = "Type";
const std::string PROPERTY_SERVICE = "Service";
const std::string PROPERTY_CHARACTERISTIC = "Characteristic";
const std::string PROPERTY_DESCRIPTOR = "Descriptor";
const std::string PROPERTY_VALUE = "Value";
const std::string PROPERTY_FLAGS = "Flags";
const std::string PROPERTY_NOTIFYING = "Notifying";
const std::string PROPERTY_PRIMARY = "Primary";
const std::string PROPERTY_DEVICE = "Device";
const std::string PROPERTY_CONNECTED = "Connected";
const std::string PROPERTY_SERVICE_UUIDS = "ServiceUUIDs";
const std::string PROPERTY_MANUFACTURER_DATA = "ManufacturerData";
const std::string PROPERTY_SERVICE_DATA = "ServiceData";
const std::string PROPERTY_LOCAL_NAME = "LocalName";
const std::string PROPERTY_INCLUDE_TX_POWER = "IncludeTxPower";

} // namespace BlueZConstants
} // namespace ggk