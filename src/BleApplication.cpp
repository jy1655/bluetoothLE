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
    // 데이터 시뮬레이션 중지
    stopDataSimulation();
    
    try {
        // BlueZ에서 등록 해제
        if (m_registered) {
            unregisterFromBlueZ();
        }
        
        // 광고 정리
        if (m_advertisement) {
            m_advertisement.reset();
        }
        
        // 순서대로 정리: 먼저 디스크립터, 그 다음 특성, 마지막으로 서비스
        m_descriptors.clear();
        m_characteristics.clear();
        m_services.clear();
        
    } catch (const std::exception& e) {
        std::cerr << "Error during cleanup: " << e.what() << std::endl;
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
    
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 1000;
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        try {
            std::cout << "Registering GATT application with BlueZ (attempt " << attempt << "/" << MAX_RETRIES << ")..." << std::endl;
            
            // 1. 먼저 BlueZ가 올바르게 실행 중인지 확인
            auto gattManagerProxy = sdbus::createProxy(
                m_connection, 
                sdbus::ServiceName("org.bluez"), 
                sdbus::ObjectPath("/org/bluez/hci0")
            );
            
            // 인터페이스가 존재하는지 확인
            bool interfaceExists = false;
            try {
                std::map<std::string, sdbus::Variant> props;
                gattManagerProxy->callMethod("GetAll")
                    .onInterface("org.freedesktop.DBus.Properties")
                    .withArguments("org.bluez.GattManager1")
                    .storeResultsTo(props);
                interfaceExists = true;
            } catch (const sdbus::Error& e) {
                if (std::string(e.getName()) == "org.freedesktop.DBus.Error.UnknownInterface") {
                    std::cerr << "GattManager1 interface not found - BlueZ might be outdated or not running" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    continue;
                }
                throw;
            }
            
            if (!interfaceExists) {
                std::cerr << "GattManager interface not found, retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                continue;
            }
            
            // Empty options map
            std::map<std::string, sdbus::Variant> options;
            
            // 2. GATT 애플리케이션 등록
            gattManagerProxy->callMethod("RegisterApplication")
                .onInterface("org.bluez.GattManager1")
                .withArguments(sdbus::ObjectPath(m_path), options);
            
            std::cout << "GATT application registered successfully" << std::endl;
            
            // 3. 광고 등록
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
            std::cerr << "Failed to register with BlueZ (attempt " << attempt << "): " 
                      << e.getName() << " - " << e.getMessage() << std::endl;
            
            if (attempt < MAX_RETRIES) {
                std::cerr << "Retrying in " << (RETRY_DELAY_MS/1000.0) << " seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to register with BlueZ (attempt " << attempt << "): " 
                      << e.what() << std::endl;
                      
            if (attempt < MAX_RETRIES) {
                std::cerr << "Retrying in " << (RETRY_DELAY_MS/1000.0) << " seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }
        }
    }
    
    std::cerr << "All registration attempts failed" << std::endl;
    return false;
}

bool BleApplication::unregisterFromBlueZ() {
    if (!m_registered) return true;
    
    bool success = true;
    
    try {
        // 광고 등록 해제
        if (m_advertisement) {
            try {
                auto advManagerProxy = sdbus::createProxy(
                    m_connection, 
                    sdbus::ServiceName("org.bluez"), 
                    sdbus::ObjectPath("/org/bluez/hci0")
                );
                
                advManagerProxy->callMethod("UnregisterAdvertisement")
                    .onInterface("org.bluez.LEAdvertisingManager1")
                    .withArguments(sdbus::ObjectPath(m_advertisement->getPath()));
                
                std::cout << "Advertisement unregistered" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to unregister advertisement: " << e.what() << std::endl;
                success = false;
                // 계속 진행
            }
        }
        
        // GATT 애플리케이션 등록 해제
        try {
            auto gattManagerProxy = sdbus::createProxy(
                m_connection, 
                sdbus::ServiceName("org.bluez"), 
                sdbus::ObjectPath("/org/bluez/hci0")
            );
            
            gattManagerProxy->callMethod("UnregisterApplication")
                .onInterface("org.bluez.GattManager1")
                .withArguments(sdbus::ObjectPath(m_path));
            
            std::cout << "BLE Application unregistered from BlueZ" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to unregister application: " << e.what() << std::endl;
            success = false;
        }
        
        m_registered = false;
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Failed to unregister from BlueZ: " << e.what() << std::endl;
        m_registered = false;  // 어쨌든 등록 해제된 것으로 간주
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