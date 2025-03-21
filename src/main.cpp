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
const GattUuid CUSTOM_SERVICE_UUID("0193d852-eba5-7d28-9abe-e30a67d39d72");
const GattUuid CUSTOM_READ_CHAR_UUID("944ecf35-cdc3-4b74-b477-5bcfe548c98e");
const GattUuid CUSTOM_WRITE_CHAR_UUID("92da1d1a-e24f-4270-8890-8bfcf74b3398");
const GattUuid CUSTOM_NOTIFY_CHAR_UUID("4393fc59-4d51-43ce-a284-cdce8f5fcc7d");

// Signal handling will be done by the Server class
// We still need this for the main loop though
static std::atomic<bool> running(true);

// Helper function to setup a battery service
GattServicePtr setupBatteryService(Server& server) {
    // 기존 코드는 그대로 유지
    auto batteryService = server.createService(BATTERY_SERVICE_UUID);
    if (!batteryService) {
        Logger::error("Failed to create Battery Service");
        return nullptr;
    }
    
    // 배터리 레벨 특성 생성
    auto batteryLevel = batteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    // 중요: NOTIFY 플래그가 있으므로 CCCD 설명자 추가
    auto cccd = batteryLevel->createDescriptor(
        GattUuid::fromShortUuid(0x2902), // CCCD UUID
        GattPermission::PERM_READ | GattPermission::PERM_WRITE
    );
    
    // CCCD 초기값 설정 (알림 비활성화 상태)
    std::vector<uint8_t> initialCccdValue = {0x00, 0x00};
    cccd->setValue(initialCccdValue);
    
    // 초기값 설정
    std::vector<uint8_t> initialValue = {80};
    batteryLevel->setValue(initialValue);
    
    // 기존 코드는 그대로 유지
    batteryLevel->setReadCallback([]() -> std::vector<uint8_t> {
        return {80};
    });
    
    // 서비스 추가
    if (!server.addService(batteryService)) {
        Logger::error("Failed to add Battery Service to server");
        return nullptr;
    }
    
    return batteryService;
}

// Helper function to setup a custom service
GattServicePtr setupCustomService(Server& server) {
    // 기존 코드는 그대로 유지
    auto customService = server.createService(CUSTOM_SERVICE_UUID);
    if (!customService) {
        Logger::error("Failed to create Custom Service");
        return nullptr;
    }
    
    // 읽기 특성 설정 - 변경 없음
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
    
    // 쓰기 특성 설정 - 변경 없음
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
    
    // 알림 특성 설정 - CCCD 추가
    auto notifyChar = customService->createCharacteristic(
        CUSTOM_NOTIFY_CHAR_UUID,
        GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    if (notifyChar) {
        // 중요: CCCD 설명자 추가
        auto cccd = notifyChar->createDescriptor(
            GattUuid::fromShortUuid(0x2902), // CCCD UUID
            GattPermission::PERM_READ | GattPermission::PERM_WRITE
        );
        
        // CCCD 초기값 설정 (알림 비활성화 상태)
        std::vector<uint8_t> initialCccdValue = {0x00, 0x00};
        cccd->setValue(initialCccdValue);
        
        // 빈 초기값 설정
        notifyChar->setValue({});
        
        // 전역 참조 저장
        static GattCharacteristicPtr notifyCharPtr = notifyChar;
    }
    
    // 서비스 추가
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