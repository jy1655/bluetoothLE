// BleApplication.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <memory>
#include <string>
#include <vector>
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "BleAdvertisement.h"
#include "BleConstants.h"
#include "DataSimulator.h"

namespace ble {

class BleApplication {
public:
    // Constructor takes a connection and a base path
    BleApplication(sdbus::IConnection& connection, 
                  const std::string& path = "/com/ble/peripheral");
    
    // Destructor
    ~BleApplication();
    
    // Application setup methods
    bool setupApplication();
    
    // Service creation methods
    bool createBatteryService();
    bool createCustomService();
    
    // Advertisement setup
    bool setupAdvertisement(const std::string& name = "BLE Peripheral");
    
    // BlueZ registration
    bool registerWithBlueZ();
    bool unregisterFromBlueZ();
    
    // Run the event loop (normally not needed as we use async)
    void run();
    
    // Update methods for characteristics
    void updateBatteryLevel(uint8_t level);
    void updateCustomValue(const std::vector<uint8_t>& value);
    
    // Start simulating data changes
    void startDataSimulation();
    
    // Stop simulating data changes
    void stopDataSimulation();
    
private:
    sdbus::IConnection& m_connection;
    std::string m_path;
    std::shared_ptr<sdbus::IObject> m_appObject;
    
    // Services, characteristics, and descriptors
    std::vector<std::shared_ptr<GattService>> m_services;
    std::vector<std::shared_ptr<GattCharacteristic>> m_characteristics;
    std::vector<std::shared_ptr<GattDescriptor>> m_descriptors;
    
    // Advertisement
    std::shared_ptr<BleAdvertisement> m_advertisement;
    
    // Registration status
    bool m_registered = false;
    
    // Data simulator
    std::unique_ptr<DataSimulator> m_dataSimulator;
};

} // namespace ble