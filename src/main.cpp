#include "Server.h"
#include "GattTypes.h"
#include "Logger.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace ggk;

// Battery Service UUID and Characteristic UUIDs
const GattUuid BATTERY_SERVICE_UUID = GattUuid::fromShortUuid(0x180F); // Standard Battery Service
const GattUuid BATTERY_LEVEL_UUID = GattUuid::fromShortUuid(0x2A19);   // Standard Battery Level Characteristic

// Custom Service and Characteristic UUIDs
const GattUuid CUSTOM_SERVICE_UUID("12345678-1234-5678-1234-56789ABCDEF0");
const GattUuid CUSTOM_READ_CHAR_UUID("12345678-1234-5678-1234-56789ABCDEF1");
const GattUuid CUSTOM_WRITE_CHAR_UUID("12345678-1234-5678-1234-56789ABCDEF2");
const GattUuid CUSTOM_NOTIFY_CHAR_UUID("12345678-1234-5678-1234-56789ABCDEF3");

// Signal handling will be done by the Server class
// We still need this for the main loop though
static std::atomic<bool> running(true);

// Helper function to setup a battery service
GattServicePtr setupBatteryService(Server& server) {
    // Create Battery Service
    auto batteryService = server.createService(BATTERY_SERVICE_UUID);
    if (!batteryService) {
        Logger::error("Failed to create Battery Service");
        return nullptr;
    }
    
    // Create Battery Level Characteristic
    auto batteryLevel = batteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    if (!batteryLevel) {
        Logger::error("Failed to create Battery Level Characteristic");
        return nullptr;
    }
    
    // Set initial battery level value (80%)
    std::vector<uint8_t> initialLevel = {80};
    batteryLevel->setValue(initialLevel);
    
    // Add read callback
    batteryLevel->setReadCallback([]() -> std::vector<uint8_t> {
        // In a real application, read the actual battery level
        return {80};
    });
    
    // Add the service to the server
    if (!server.addService(batteryService)) {
        Logger::error("Failed to add Battery Service to server");
        return nullptr;
    }
    
    return batteryService;
}

// Helper function to setup a custom service
GattServicePtr setupCustomService(Server& server) {
    // Create Custom Service
    auto customService = server.createService(CUSTOM_SERVICE_UUID);
    if (!customService) {
        Logger::error("Failed to create Custom Service");
        return nullptr;
    }
    
    // Create a readable characteristic
    auto readChar = customService->createCharacteristic(
        CUSTOM_READ_CHAR_UUID,
        GattProperty::PROP_READ,
        GattPermission::PERM_READ
    );
    
    if (readChar) {
        // Set initial value (MUST be done before server starts)
        std::vector<uint8_t> initialData = {'H', 'e', 'l', 'l', 'o'};
        readChar->setValue(initialData);
        
        // Add read callback (this is called during runtime)
        readChar->setReadCallback([]() -> std::vector<uint8_t> {
            return {'H', 'e', 'l', 'l', 'o'};
        });
    }
    
    // Create a writable characteristic
    auto writeChar = customService->createCharacteristic(
        CUSTOM_WRITE_CHAR_UUID,
        GattProperty::PROP_WRITE,
        GattPermission::PERM_WRITE
    );
    
    if (writeChar) {
        // Add write callback
        writeChar->setWriteCallback([](const std::vector<uint8_t>& value) -> bool {
            // Print received data
            std::string data(value.begin(), value.end());
            Logger::info("Received data: " + data);
            return true;
        });
    }
    
    // Create a notify characteristic
    auto notifyChar = customService->createCharacteristic(
        CUSTOM_NOTIFY_CHAR_UUID,
        GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    if (notifyChar) {
        // Empty initial value
        notifyChar->setValue({});
        
        // Store this for notifications (can be accessed from elsewhere)
        // In a real app, you'd use a better way to share this reference
        static GattCharacteristicPtr notifyCharPtr = notifyChar;
    }
    
    // Add the service to the server
    if (!server.addService(customService)) {
        Logger::error("Failed to add Custom Service to server");
        return nullptr;
    }
    
    return customService;
}

int main(int argc, char** argv) {
    // We don't need to register signal handlers here
    // The Server class will handle this
    
    // Create and initialize the BLE server
    Server server;
    if (!server.initialize("JetsonBLE")) {
        std::cerr << "Failed to initialize BLE server" << std::endl;
        return 1;
    }
    
    // Setup Battery Service
    auto batteryService = setupBatteryService(server);
    if (!batteryService) {
        std::cerr << "Failed to setup Battery Service" << std::endl;
        return 1;
    }
    
    // Setup Custom Service
    auto customService = setupCustomService(server);
    if (!customService) {
        std::cerr << "Failed to setup Custom Service" << std::endl;
        return 1;
    }
    
    // Configure advertisement (optional - defaults are already set in initialize())
    server.configureAdvertisement(
        "JetsonBLE",               // Local name
        {},                         // Service UUIDs (empty = use all)
        0x0059,                    // Manufacturer ID (0x0059 = Jetson)
        {0x01, 0x02, 0x03, 0x04},  // Manufacturer data
        true                        // Include TX power
    );
    
    // Set connection callback
    server.setConnectionCallback([](const std::string& deviceAddress) {
        std::cout << "Client connected: " << deviceAddress << std::endl;
    });
    
    // Set disconnection callback
    server.setDisconnectionCallback([](const std::string& deviceAddress) {
        std::cout << "Client disconnected: " << deviceAddress << std::endl;
    });
    
    // Start the server (non-blocking)
    if (!server.start()) {
        std::cerr << "Failed to start BLE server" << std::endl;
        return 1;
    }
    
    std::cout << "BLE Server started. Press Ctrl+C to exit." << std::endl;
    
    // Get the characteristic for notifications (in a real app, you'd use a better design)
    auto notifyChar = customService->getCharacteristic(CUSTOM_NOTIFY_CHAR_UUID);
    
    // Main loop - update values periodically
    int count = 0;
    std::vector<uint8_t> batteryValue = {80};
    
    while (running) {
        // Simulate battery level changes (decreasing)
        auto batteryChar = batteryService->getCharacteristic(BATTERY_LEVEL_UUID);
        if (batteryChar) {
            batteryValue[0] = 80 - (count % 50);  // Range from 80% down to 31%
            batteryChar->setValue(batteryValue);
        }
        
        // Send periodic notifications
        if (notifyChar) {
            std::string countStr = "Count: " + std::to_string(count);
            std::vector<uint8_t> notifyData(countStr.begin(), countStr.end());
            notifyChar->setValue(notifyData);
        }
        
        count++;
        
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "Shutting down..." << std::endl;
    
    // Stop the server
    server.stop();
    
    std::cout << "BLE Server stopped." << std::endl;
    return 0;
}