// src/main.cpp
#include "Server.h"
#include "GattTypes.h"
#include "GattCharacteristic.h"
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
const GattUuid CUSTOM_SERVICE_UUID("0193d852-eba5-7d28-9abe-e30a67d39d72");
const GattUuid CUSTOM_READ_CHAR_UUID("944ecf35-cdc3-4b74-b477-5bcfe548c98e");
const GattUuid CUSTOM_WRITE_CHAR_UUID("92da1d1a-e24f-4270-8890-8bfcf74b3398");
const GattUuid CUSTOM_NOTIFY_CHAR_UUID("4393fc59-4d51-43ce-a284-cdce8f5fcc7d");

// Signal handling is managed by the Server class
// We still need this for the main loop
static std::atomic<bool> running(true);

// Helper function to setup a battery service
GattServicePtr setupBatteryService(Server& server) {
    auto batteryService = server.createService(BATTERY_SERVICE_UUID);
    if (!batteryService) {
        Logger::error("Failed to create Battery Service");
        return nullptr;
    }
    
    // Create battery level characteristic
    auto batteryLevel = batteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    // Important: CCCD descriptor is now automatically created by the characteristic
    // when it has NOTIFY/INDICATE properties
    
    // Set initial value
    std::vector<uint8_t> initialValue = {80};
    batteryLevel->setValue(initialValue);
    
    // Set read callback
    batteryLevel->setReadCallback([]() -> std::vector<uint8_t> {
        return {80};
    });
    
    // Add service to server
    if (!server.addService(batteryService)) {
        Logger::error("Failed to add Battery Service to server");
        return nullptr;
    }
    
    return batteryService;
}

// Helper function to setup a custom service
GattServicePtr setupCustomService(Server& server) {
    auto customService = server.createService(CUSTOM_SERVICE_UUID);
    if (!customService) {
        Logger::error("Failed to create Custom Service");
        return nullptr;
    }
    
    // Read characteristic setup
    auto readChar = customService->createCharacteristic(
        CUSTOM_READ_CHAR_UUID,
        GattProperty::PROP_READ,
        GattPermission::PERM_READ
    );
    
    if (readChar) {
        std::vector<uint8_t> initialData = {'H', 'e', 'l', 'l', 'o'};
        readChar->setValue(initialData);
        readChar->setReadCallback([]() -> std::vector<uint8_t> {
            return {'H', 'e', 'l', 'l', 'o'};
        });
    }
    
    // Write characteristic setup
    auto writeChar = customService->createCharacteristic(
        CUSTOM_WRITE_CHAR_UUID,
        GattProperty::PROP_WRITE,
        GattPermission::PERM_WRITE
    );
    
    if (writeChar) {
        writeChar->setWriteCallback([](const std::vector<uint8_t>& value) -> bool {
            std::string data(value.begin(), value.end());
            Logger::info("Received data: " + data);
            return true;
        });
    }
    
    // Notify characteristic setup
    auto notifyChar = customService->createCharacteristic(
        CUSTOM_NOTIFY_CHAR_UUID,
        GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    // Add service to server
    if (!server.addService(customService)) {
        Logger::error("Failed to add Custom Service to server");
        return nullptr;
    }
    
    return customService;
}

int main(int argc, char** argv) {
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
        "JetsonBLE",                // 로컬 이름
        {},                         // 서비스 UUID (빈 배열 = 모든 서비스 사용)
        0x0059,                     // 제조사 ID (0x0059 = Jetson)
        {0x01, 0x02, 0x03, 0x04},   // 제조사 데이터
        true                        // TX Power 포함
    );

    // 수동으로 128비트 UUID 및 세부 구성이 필요한 경우의 예시
    // 아래와 같이 advertisement 객체를 직접 구성할 수도 있습니다
    /*
    auto& adv = server.getAdvertisement();
    adv.setLocalName("JetsonBLE");
    adv.setDiscoverable(true);

    // BlueZ 5.82 스타일 Includes 배열 설정
    adv.addInclude("tx-power");
    adv.addInclude("appearance");

    // 필요시 appearance 코드 설정 (예: 0x0340 = Generic 센서)
    adv.setAppearance(0x0340);

    // 128비트 UUID 직접 추가
    adv.addServiceUUID(GattUuid("0193d852-eba5-7d28-9abe-e30a67d39d72"));

    // 16비트 UUID도 함께 추가 가능
    adv.addServiceUUID(GattUuid::fromShortUuid(0x180F)); // Battery Service

    // 제조사 데이터 설정
    std::vector<uint8_t> mfgData = {0x01, 0x02, 0x03, 0x04};
    adv.setManufacturerData(0x0059, mfgData);
    */
    
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