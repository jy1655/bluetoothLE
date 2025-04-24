// BleAdvertisement.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>
#include <map>
#include "xml/LEAdvertisement1.h"

namespace ble {

class BleAdvertisement : public sdbus::AdaptorInterfaces<org::bluez::LEAdvertisement1_adaptor> {
public:
    BleAdvertisement(sdbus::IConnection& connection,
                    const std::string& path,
                    const std::string& name);
    
    virtual ~BleAdvertisement();
    
    // LEAdvertisement1_adaptor required methods
    void Release() override;
    
    // Property getters
    std::string Type() override;
    std::vector<std::string> ServiceUUIDs() override;
    std::map<uint16_t, sdbus::Variant> ManufacturerData() override;
    std::vector<std::string> SolicitUUIDs() override;
    std::map<std::string, sdbus::Variant> ServiceData() override;
    std::map<uint8_t, sdbus::Variant> Data() override;
    
    // BlueZ 5.82 scan response properties
    std::vector<std::string> ScanResponseServiceUUIDs() override;
    std::map<uint16_t, sdbus::Variant> ScanResponseManufacturerData() override;
    std::vector<std::string> ScanResponseSolicitUUIDs() override;
    std::map<std::string, sdbus::Variant> ScanResponseServiceData() override;
    std::map<uint8_t, sdbus::Variant> ScanResponseData() override;
    
    // Other properties
    std::vector<std::string> Includes() override;
    std::string LocalName() override;
    uint16_t Appearance() override;
    uint16_t Duration() override;
    uint16_t Timeout() override;
    std::string SecondaryChannel() override;
    bool Discoverable() override;
    uint16_t DiscoverableTimeout() override;
    uint32_t MinInterval() override;
    uint32_t MaxInterval() override;
    int16_t TxPower() override;
    
    // Service UUID management
    void addServiceUUID(const std::string& uuid);
    
    // Utility methods
    std::string getPath() const { return m_objectPath; }
    
private:
    std::string m_objectPath;
    std::string m_localName;
    std::vector<std::string> m_serviceUUIDs;
    std::vector<std::string> m_includes = {"tx-power", "local-name"};
    std::map<uint16_t, sdbus::Variant> m_manufacturerData;
};

} // namespace ble