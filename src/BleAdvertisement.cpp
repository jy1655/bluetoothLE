// BleAdvertisement.cpp
#include "BleAdvertisement.h"
#include <iostream>
#include <algorithm>

namespace ble {

BleAdvertisement::BleAdvertisement(sdbus::IConnection& connection,
                                 const std::string& path,
                                 const std::string& name)
    : AdaptorInterfaces(connection, sdbus::ObjectPath(path)),
      m_objectPath(path),
      m_localName(name) {
    
    // Set up manufacturer data (example: using Nordic Semiconductor's ID)
    std::vector<uint8_t> mfgData = {0x01, 0x02, 0x03, 0x04};
    m_manufacturerData[0x0059] = sdbus::Variant(mfgData);  // 0x0059 is Nordic Semiconductor
    
    // Register the adaptor
    registerAdaptor();
    
    // Emit the InterfacesAdded signal for this object
    getObject().emitInterfacesAddedSignal({sdbus::InterfaceName{org::bluez::LEAdvertisement1_adaptor::INTERFACE_NAME}});
    
    std::cout << "BleAdvertisement created: " << m_objectPath << " (Name: " << name << ")" << std::endl;
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

std::string BleAdvertisement::SecondaryChannel() {
    // Secondary advertising channel (not used)
    // Can be "1M" (default), "2M", or "Coded"
    return "";
}

bool BleAdvertisement::Discoverable() {
    // Must be true for BlueZ 5.82+
    return true;
}

uint16_t BleAdvertisement::DiscoverableTimeout() {
    // Discoverable timeout in seconds (0 = no timeout)
    return 0;
}

uint32_t BleAdvertisement::MinInterval() {
    // Minimum advertising interval in microseconds (0 = system default)
    return 0;
}

uint32_t BleAdvertisement::MaxInterval() {
    // Maximum advertising interval in microseconds (0 = system default)
    return 0;
}

int16_t BleAdvertisement::TxPower() {
    // TX power in dBm (-127 to +20, -127 = undefined)
    return -127;
}

void BleAdvertisement::addServiceUUID(const std::string& uuid) {
    // Check for duplicates
    if (std::find(m_serviceUUIDs.begin(), m_serviceUUIDs.end(), uuid) != m_serviceUUIDs.end()) {
        return;
    }
    
    m_serviceUUIDs.push_back(uuid);
    std::cout << "Service UUID added to advertisement: " << uuid << std::endl;
    
    // Add "service-uuids" to Includes if not already there
    if (std::find(m_includes.begin(), m_includes.end(), "service-uuids") == m_includes.end()) {
        m_includes.push_back("service-uuids");
    }
}

} // namespace ble