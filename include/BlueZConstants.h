// BlueZConstants.h
#pragma once
#include <string>

namespace ggk {
namespace BlueZConstants {

// Version information
constexpr int BLUEZ_MINIMUM_VERSION = 5;
constexpr int BLUEZ_MINIMUM_MINOR_VERSION = 50;
const std::string BLUEZ_VERSION_REQUIREMENT = "5.50+";

// Service name
const std::string BLUEZ_SERVICE = "org.bluez";

// Common paths
const std::string ROOT_PATH = "/";
const std::string ADAPTER_PATH = "/org/bluez/hci0";

// Interfaces
const std::string OBJECT_MANAGER_INTERFACE = "org.freedesktop.DBus.ObjectManager";
const std::string PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
const std::string ADAPTER_INTERFACE = "org.bluez.Adapter1";
const std::string DEVICE_INTERFACE = "org.bluez.Device1";
const std::string GATT_MANAGER_INTERFACE = "org.bluez.GattManager1";
const std::string LE_ADVERTISING_MANAGER_INTERFACE = "org.bluez.LEAdvertisingManager1";
const std::string GATT_SERVICE_INTERFACE = "org.bluez.GattService1";
const std::string GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
const std::string GATT_DESCRIPTOR_INTERFACE = "org.bluez.GattDescriptor1";
const std::string LE_ADVERTISEMENT_INTERFACE = "org.bluez.LEAdvertisement1";
const std::string AGENT_MANAGER_INTERFACE = "org.bluez.AgentManager1";
const std::string AGENT_INTERFACE = "org.bluez.Agent1";
const std::string PROFILE_MANAGER_INTERFACE = "org.bluez.ProfileManager1";
const std::string PROFILE_INTERFACE = "org.bluez.Profile1";
const std::string BATTERY_INTERFACE = "org.bluez.Battery1";

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
const std::string SET_DISCOVERY_FILTER = "SetDiscoveryFilter";
const std::string START_DISCOVERY = "StartDiscovery";
const std::string STOP_DISCOVERY = "StopDiscovery";
const std::string REMOVE_DEVICE = "RemoveDevice";
const std::string CONNECT_DEVICE = "Connect";
const std::string DISCONNECT_DEVICE = "Disconnect";
const std::string PAIR_DEVICE = "Pair";
const std::string CANCEL_PAIRING = "CancelPairing";

// Property names
const std::string PROPERTY_UUID = "UUID";
const std::string PROPERTY_SERVICE_UUIDS = "ServiceUUIDs";
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
const std::string PROPERTY_TRUSTED = "Trusted";
const std::string PROPERTY_PAIRED = "Paired";
const std::string PROPERTY_BLOCKED = "Blocked";
const std::string PROPERTY_ALIAS = "Alias";
const std::string PROPERTY_ADDRESS = "Address";
const std::string PROPERTY_ADDRESS_TYPE = "AddressType";
const std::string PROPERTY_RSSI = "RSSI";
const std::string PROPERTY_TX_POWER = "TxPower";
const std::string PROPERTY_MANUFACTURER_DATA = "ManufacturerData";
const std::string PROPERTY_SERVICE_DATA = "ServiceData";
const std::string PROPERTY_LOCAL_NAME = "LocalName";
const std::string PROPERTY_INCLUDE_TX_POWER = "IncludeTxPower";
const std::string PROPERTY_POWERED = "Powered";
const std::string PROPERTY_DISCOVERABLE = "Discoverable";
const std::string PROPERTY_PAIRABLE = "Pairable";
const std::string PROPERTY_DISCOVERING = "Discovering";
const std::string PROPERTY_NAME = "Name";
const std::string PROPERTY_CLASS = "Class";
const std::string PROPERTY_APPEARANCE = "Appearance";
const std::string PROPERTY_ICON = "Icon";
const std::string PROPERTY_MTU = "MTU";
const std::string PROPERTY_PERCENTAGE = "Percentage";

// Signals
const std::string SIGNAL_PROPERTIES_CHANGED = "PropertiesChanged";
const std::string SIGNAL_INTERFACES_ADDED = "InterfacesAdded";
const std::string SIGNAL_INTERFACES_REMOVED = "InterfacesRemoved";

// Adapter properties
const std::string ADAPTER_PROPERTY_POWERED = "Powered";
const std::string ADAPTER_PROPERTY_DISCOVERABLE = "Discoverable";
const std::string ADAPTER_PROPERTY_DISCOVERABLE_TIMEOUT = "DiscoverableTimeout";
const std::string ADAPTER_PROPERTY_PAIRABLE = "Pairable";
const std::string ADAPTER_PROPERTY_PAIRABLE_TIMEOUT = "PairableTimeout";
const std::string ADAPTER_PROPERTY_DISCOVERING = "Discovering";
const std::string ADAPTER_PROPERTY_NAME = "Name";

// Common values
const std::string VALUE_PUBLIC = "public";
const std::string VALUE_RANDOM = "random";
const std::string VALUE_PERIPHERAL = "peripheral";
const std::string VALUE_BROADCAST = "broadcast";

// Characteristic flags
const std::string FLAG_BROADCAST = "broadcast";
const std::string FLAG_READ = "read";
const std::string FLAG_WRITE_WITHOUT_RESPONSE = "write-without-response";
const std::string FLAG_WRITE = "write";
const std::string FLAG_NOTIFY = "notify";
const std::string FLAG_INDICATE = "indicate";
const std::string FLAG_AUTHENTICATED_SIGNED_WRITES = "authenticated-signed-writes";
const std::string FLAG_EXTENDED_PROPERTIES = "extended-properties";
const std::string FLAG_RELIABLE_WRITE = "reliable-write";
const std::string FLAG_WRITABLE_AUXILIARIES = "writable-auxiliaries";
const std::string FLAG_ENCRYPT_READ = "encrypt-read";
const std::string FLAG_ENCRYPT_WRITE = "encrypt-write";
const std::string FLAG_ENCRYPT_AUTHENTICATED_READ = "encrypt-authenticated-read";
const std::string FLAG_ENCRYPT_AUTHENTICATED_WRITE = "encrypt-authenticated-write";

// Standard UUIDs
namespace UUID {
    // Core Bluetooth UUIDs
    const std::string GENERIC_ACCESS_SERVICE = "00001800-0000-1000-8000-00805f9b34fb";
    const std::string GENERIC_ATTRIBUTE_SERVICE = "00001801-0000-1000-8000-00805f9b34fb";
    const std::string DEVICE_INFORMATION_SERVICE = "0000180a-0000-1000-8000-00805f9b34fb";
    const std::string BATTERY_SERVICE = "0000180f-0000-1000-8000-00805f9b34fb";
    
    // Characteristic UUIDs
    const std::string DEVICE_NAME = "00002a00-0000-1000-8000-00805f9b34fb";
    const std::string APPEARANCE = "00002a01-0000-1000-8000-00805f9b34fb";
    const std::string MANUFACTURER_NAME = "00002a29-0000-1000-8000-00805f9b34fb";
    const std::string MODEL_NUMBER = "00002a24-0000-1000-8000-00805f9b34fb";
    const std::string SERIAL_NUMBER = "00002a25-0000-1000-8000-00805f9b34fb";
    const std::string FIRMWARE_REVISION = "00002a26-0000-1000-8000-00805f9b34fb";
    const std::string HARDWARE_REVISION = "00002a27-0000-1000-8000-00805f9b34fb";
    const std::string SOFTWARE_REVISION = "00002a28-0000-1000-8000-00805f9b34fb";
    const std::string BATTERY_LEVEL = "00002a19-0000-1000-8000-00805f9b34fb";
    
    // Descriptor UUIDs
    const std::string CHARACTERISTIC_EXTENDED_PROPERTIES = "00002900-0000-1000-8000-00805f9b34fb";
    const std::string CHARACTERISTIC_USER_DESCRIPTION = "00002901-0000-1000-8000-00805f9b34fb";
    const std::string CLIENT_CHARACTERISTIC_CONFIG = "00002902-0000-1000-8000-00805f9b34fb";
    const std::string SERVER_CHARACTERISTIC_CONFIG = "00002903-0000-1000-8000-00805f9b34fb";
    const std::string CHARACTERISTIC_PRESENTATION_FORMAT = "00002904-0000-1000-8000-00805f9b34fb";
    const std::string CHARACTERISTIC_AGGREGATE_FORMAT = "00002905-0000-1000-8000-00805f9b34fb";
}

} // namespace BlueZConstants
} // namespace ggk