// BleAdvertisement.cpp
#include "BleAdvertisement.h"
#include "BleConstants.h"
#include <iostream>
#include <algorithm>

namespace ble {

BleAdvertisement::BleAdvertisement(sdbus::IConnection& connection,
                               const std::string& path,
                               const std::string& name)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_localName(name.substr(0, 8)) { // Limit name length to 8 characters
    
    // We'll only include essential data
    // Either include manufacturer data OR service UUIDs, not both
    
    // Option 1: Keep manufacturer data, skip service UUIDs initially
    // std::vector<uint8_t> mfgData = {0x01, 0x02};  // Make this smaller
    // m_manufacturerData[0x0059] = sdbus::Variant(mfgData);
    
    // Minimal includes
    m_includes = {"tx-power"};  // Keep only essential includes
    
    // Register the adaptor
    registerAdaptor();
    
    // Emit the InterfacesAdded signal for this object
    getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::LEAdvertisement1_adaptor::INTERFACE_NAME}});
    
    std::cout << "BleAdvertisement created: " << m_objectPath << " (Name: " << m_localName << ")" << std::endl;
}

BleAdvertisement::~BleAdvertisement() {
    // Emit the InterfacesRemoved signal when this object is destroyed
    getObject().emitInterfacesRemovedSignal({sdbus::InterfaceName{org::bluez::LEAdvertisement1_adaptor::INTERFACE_NAME}});
    
    // Unregister the adaptor
    unregisterAdaptor();
    
    std::cout << "BleAdvertisement destroyed: " << m_objectPath << std::endl;
}

void BleAdvertisement::Release() {
    std::cout << "Release called on advertisement: " << m_objectPath << std::endl;
    // This method is called when BlueZ removes this advertisement
    // We don't need to do anything here as the object will be destroyed by the application
}

std::string BleAdvertisement::Type() {
    // Can be "peripheral" or "broadcast"
    return "peripheral";
}

std::vector<std::string> BleAdvertisement::ServiceUUIDs() {
    return m_serviceUUIDs;
}

std::map<uint16_t, sdbus::Variant> BleAdvertisement::ManufacturerData() {
    return m_manufacturerData;
}

std::vector<std::string> BleAdvertisement::SolicitUUIDs() {
    // Typically not used
    return {};
}

std::map<std::string, sdbus::Variant> BleAdvertisement::ServiceData() {
    // Typically not used
    return {};
}

std::map<uint8_t, sdbus::Variant> BleAdvertisement::Data() {
    // BlueZ 5.82+ feature: Raw advertising data
    return {};
}

std::vector<std::string> BleAdvertisement::ScanResponseServiceUUIDs() {
    // BlueZ 5.82+ feature: Service UUIDs for scan response
    return {};
}

std::map<uint16_t, sdbus::Variant> BleAdvertisement::ScanResponseManufacturerData() {
    // BlueZ 5.82+ feature: Manufacturer data for scan response
    return {};
}

std::vector<std::string> BleAdvertisement::ScanResponseSolicitUUIDs() {
    // BlueZ 5.82+ feature: Solicit UUIDs for scan response
    return {};
}

std::map<std::string, sdbus::Variant> BleAdvertisement::ScanResponseServiceData() {
    // BlueZ 5.82+ feature: Service data for scan response
    return {};
}

std::map<uint8_t, sdbus::Variant> BleAdvertisement::ScanResponseData() {
    // BlueZ 5.82+ feature: Raw data for scan response
    return {};
}

std::vector<std::string> BleAdvertisement::Includes() {
    return m_includes;
}

std::string BleAdvertisement::LocalName() {
    return m_localName;
}

uint16_t BleAdvertisement::Appearance() {
    // BlueZ 5.82 compatibility: Return 0 for unspecified
    return 0;
}

uint16_t BleAdvertisement::Duration() {
    // Advertising duration in milliseconds (0 = indefinite)
    return 0;
}

uint16_t BleAdvertisement::Timeout() {
    // Advertising timeout in seconds (0 = no timeout)
    return 0;
}

/*
std::string BleAdvertisement::SecondaryChannel() {
    // Secondary advertising channel
    // Valid values: "1M" (default), "2M", or "Coded"
    return "1M";  // Return the default value instead of empty string
}
*/

bool BleAdvertisement::Discoverable() {
    // Must be true for BlueZ 5.82+
    return true;
}

uint16_t BleAdvertisement::DiscoverableTimeout() {
    // Discoverable timeout in seconds (0 = no timeout)
    return 0;
}

uint32_t BleAdvertisement::MinInterval() {
    // Minimum advertising interval in microseconds
    return 200;  // 0.2 seconds
}

uint32_t BleAdvertisement::MaxInterval() {
    // Maximum advertising interval in microseconds
    return 1000;  // 1 second
}

int16_t BleAdvertisement::TxPower() {
    // TX power in dBm (-127 to +20, -127 = undefined)
    return 0;
}

void BleAdvertisement::addServiceUUID(const std::string& uuid) {
    // Check for duplicates
    if (std::find(m_serviceUUIDs.begin(), m_serviceUUIDs.end(), uuid) != m_serviceUUIDs.end()) {
        return;
    }
    
    // Only add essential service UUIDs to save space
    // For standard services, limit to just one or two important ones
    if (m_serviceUUIDs.empty()) {
        m_serviceUUIDs.push_back(uuid);
        std::cout << "Service UUID added to advertisement: " << uuid << std::endl;
        
        // Add "service-uuids" to Includes if not already there
        if (std::find(m_includes.begin(), m_includes.end(), "service-uuids") == m_includes.end()) {
            m_includes.push_back("service-uuids");
        }
    } else if (m_serviceUUIDs.size() < 2) { // Limit to max 2 service UUIDs
        // Only add if it's an important service
        if (uuid == BleConstants::BATTERY_SERVICE_UUID || 
            uuid == BleConstants::DEVICE_INFO_SERVICE_UUID) {
            m_serviceUUIDs.push_back(uuid);
            std::cout << "Service UUID added to advertisement: " << uuid << std::endl;
        } else {
            std::cout << "Skipping non-essential service UUID to save advertisement space: " << uuid << std::endl;
        }
    } else {
        std::cout << "Maximum service UUIDs reached, skipping: " << uuid << std::endl;
    }
}

} // namespace ble