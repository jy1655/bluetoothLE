// BleApplication.cpp
#include "BleApplication.h"
#include "BluetoothAdapter.h" // Include the header, not the cpp
#include <iostream>
#include <map>

namespace ble {

BleApplication::BleApplication(sdbus::IConnection& connection, const std::string& path)
    : m_connection(connection), m_path(path) {
    
    // Create the application object
    m_appObject = sdbus::createObject(m_connection, sdbus::ObjectPath(m_path));
    
    // Add ObjectManager interface
    m_appObject->addObjectManager();
    
    std::cout << "BLE Application created at path: " << m_path << std::endl;
}

BleApplication::~BleApplication() {
    // Stop data simulation if running
    stopDataSimulation();
    
    // Unregister from BlueZ if registered
    if (m_registered) {
        unregisterFromBlueZ();
    }
}

bool BleApplication::setupApplication() {
    try {
        // Create services
        if (!createBatteryService()) {
            std::cerr << "Failed to create battery service" << std::endl;
            return false;
        }
        
        if (!createCustomService()) {
            std::cerr << "Failed to create custom service" << std::endl;
            return false;
        }
        
        std::cout << "BLE Application setup completed successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to setup application: " << e.what() << std::endl;
        return false;
    }
}

bool BleApplication::createBatteryService() {
    // Create battery service (0x180F is standard battery service UUID)
    auto servicePath = m_path + "/service0";
    auto service = std::make_shared<GattService>(
        m_connection,
        servicePath,
        BleConstants::BATTERY_SERVICE_UUID,
        true  // Primary service
    );
    
    // Create battery level characteristic (0x2A19 is standard battery level UUID)
    auto charPath = servicePath + "/char0";
    auto characteristic = std::make_shared<GattCharacteristic>(
        m_connection,
        charPath,
        BleConstants::BATTERY_LEVEL_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        servicePath
    );
    
    // Set initial battery level to 100%
    characteristic->setValue({100});
    
    // Create the CCCD (Client Characteristic Configuration Descriptor)
    auto descPath = charPath + "/desc0";
    auto descriptor = std::make_shared<GattDescriptor>(
        m_connection,
        descPath,
        BleConstants::CCCD_UUID,
        GattPermission::PERM_READ | GattPermission::PERM_WRITE,
        charPath
    );
    
    // Initialize CCCD value to disabled
    descriptor->setValue({0, 0});
    
    // Store the services, characteristics, and descriptors
    m_services.push_back(service);
    m_characteristics.push_back(characteristic);
    m_descriptors.push_back(descriptor);
    
    std::cout << "Battery service created at: " << servicePath << std::endl;
    return true;
}

bool BleApplication::createCustomService() {
    // Create custom service
    auto servicePath = m_path + "/service1";
    auto service = std::make_shared<GattService>(
        m_connection,
        servicePath,
        BleConstants::CUSTOM_SERVICE_UUID,
        true  // Primary service
    );
    
    // Create a read/write characteristic
    auto charPath = servicePath + "/char0";
    auto characteristic = std::make_shared<GattCharacteristic>(
        m_connection,
        charPath,
        BleConstants::CUSTOM_CHAR_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_WRITE | GattProperty::PROP_NOTIFY,
        servicePath
    );
    
    // Set initial value
    std::vector<uint8_t> initialValue = {0x11, 0x22, 0x33, 0x44};
    characteristic->setValue(initialValue);
    
    // Create the CCCD (Client Characteristic Configuration Descriptor)
    auto descPath = charPath + "/desc0";
    auto descriptor = std::make_shared<GattDescriptor>(
        m_connection,
        descPath,
        BleConstants::CCCD_UUID,
        GattPermission::PERM_READ | GattPermission::PERM_WRITE,
        charPath
    );
    
    // Initialize CCCD value to disabled
    descriptor->setValue({0, 0});
    
    // Store the services, characteristics, and descriptors
    m_services.push_back(service);
    m_characteristics.push_back(characteristic);
    m_descriptors.push_back(descriptor);
    
    std::cout << "Custom service created at: " << servicePath << std::endl;
    return true;
}

bool BleApplication::setupAdvertisement(const std::string& name) {
    // Create advertisement
    auto advPath = m_path + "/advertisement0";
    m_advertisement = std::make_shared<BleAdvertisement>(
        m_connection,
        advPath,
        name
    );
    
    // Add service UUIDs to the advertisement
    for (const auto& service : m_services) {
        m_advertisement->addServiceUUID(service->getUuid());
    }
    
    std::cout << "Advertisement setup completed with name: " << name << std::endl;
    return true;
}

bool BleApplication::registerWithBlueZ() {
    if (m_registered) return true;
    
    try {
        // Register the GATT application
        auto gattManagerProxy = sdbus::createProxy(
            m_connection, 
            sdbus::ServiceName("org.bluez"), 
            sdbus::ObjectPath("/org/bluez/hci0")
        );
        
        // Empty options map
        std::map<std::string, sdbus::Variant> options;
        
        std::cout << "Registering GATT application with BlueZ..." << std::endl;
        
        gattManagerProxy->callMethod("RegisterApplication")
            .onInterface("org.bluez.GattManager1")
            .withArguments(sdbus::ObjectPath(m_path), options);
        
        std::cout << "GATT application registered successfully" << std::endl;
        
        // Register the advertisement
        if (m_advertisement) {
            auto leAdvManagerProxy = sdbus::createProxy(
                m_connection, 
                sdbus::ServiceName("org.bluez"), 
                sdbus::ObjectPath("/org/bluez/hci0")
            );
            
            std::map<std::string, sdbus::Variant> advOptions;
            
            std::cout << "Registering advertisement with BlueZ..." << std::endl;
            
            leAdvManagerProxy->callMethod("RegisterAdvertisement")
                .onInterface("org.bluez.LEAdvertisingManager1")
                .withArguments(sdbus::ObjectPath(m_advertisement->getPath()), advOptions);
            
            std::cout << "Advertisement registered successfully" << std::endl;
        }
        
        m_registered = true;
        std::cout << "BLE Application registered with BlueZ successfully" << std::endl;
        return true;
    } catch (const sdbus::Error& e) {
        std::cerr << "Failed to register with BlueZ: " << e.getName() << " - " << e.getMessage() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Failed to register with BlueZ: " << e.what() << std::endl;
        return false;
    }
}

bool BleApplication::unregisterFromBlueZ() {
    if (!m_registered) return true;
    
    try {
        // Unregister the advertisement
        if (m_advertisement) {
            auto advManagerProxy = sdbus::createProxy(
                m_connection, 
                sdbus::ServiceName("org.bluez"), 
                sdbus::ObjectPath("/org/bluez/hci0")
            );
            
            advManagerProxy->callMethod("UnregisterAdvertisement")
                .onInterface("org.bluez.LEAdvertisingManager1")
                .withArguments(sdbus::ObjectPath(m_advertisement->getPath()));
            
            std::cout << "Advertisement unregistered" << std::endl;
        }
        
        // Unregister the GATT application
        auto gattManagerProxy = sdbus::createProxy(
            m_connection, 
            sdbus::ServiceName("org.bluez"), 
            sdbus::ObjectPath("/org/bluez/hci0")
        );
        
        gattManagerProxy->callMethod("UnregisterApplication")
            .onInterface("org.bluez.GattManager1")
            .withArguments(sdbus::ObjectPath(m_path));
        
        m_registered = false;
        std::cout << "BLE Application unregistered from BlueZ" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to unregister from BlueZ: " << e.what() << std::endl;
        return false;
    }
}

void BleApplication::run() {
    std::cout << "Entering event loop..." << std::endl;
    
    // Start data simulation
    startDataSimulation();
    
    // Enter the event loop
    m_connection.enterEventLoop();
}

void BleApplication::startDataSimulation() {
    if (m_dataSimulator) {
        // Start battery level simulation
        m_dataSimulator->startBatterySimulation([this](uint8_t level) {
            this->updateBatteryLevel(level);
        });
        
        // Start custom data simulation
        m_dataSimulator->startCustomDataSimulation([this](const std::vector<uint8_t>& value) {
            this->updateCustomValue(value);
        });
        
        std::cout << "Data simulation started" << std::endl;
    }
}

void BleApplication::stopDataSimulation() {
    if (m_dataSimulator) {
        m_dataSimulator->stopSimulation();
        std::cout << "Data simulation stopped" << std::endl;
    }
}

void BleApplication::updateBatteryLevel(uint8_t level) {
    // Find the battery level characteristic (first characteristic in our case)
    if (!m_characteristics.empty()) {
        auto batteryChar = m_characteristics[0];
        batteryChar->setValue({level});
        
        // Emit PropertiesChanged signal for Value property
        batteryChar->notifyValueChanged();
        
        std::cout << "Battery level updated to: " << static_cast<int>(level) << "%" << std::endl;
    }
}

void BleApplication::updateCustomValue(const std::vector<uint8_t>& value) {
    // Find the custom characteristic (second characteristic in our list)
    if (m_characteristics.size() > 1) {
        auto customChar = m_characteristics[1];
        customChar->setValue(value);
        
        // Emit PropertiesChanged signal for Value property
        customChar->notifyValueChanged();
        
        std::cout << "Custom value updated" << std::endl;
    }
}

} // namespace ble