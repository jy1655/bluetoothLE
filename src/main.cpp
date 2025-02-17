#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

#include "BLEPeripheralManager.h"

static volatile bool running = true;

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        running = false;
    }
}

class SimpleLogger {
public:
    static void log(const char* message) {
        std::cout << "[BLE] " << message << std::endl;
    }
};

int main() {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Set up logging
    ggk::Logger::registerDebugReceiver([](const char* msg) { SimpleLogger::log(msg); });
    ggk::Logger::registerInfoReceiver([](const char* msg) { SimpleLogger::log(msg); });
    ggk::Logger::registerErrorReceiver([](const char* msg) { 
        std::cerr << "[BLE ERROR] " << msg << std::endl; 
    });

    // Create and initialize BLE peripheral
    ggk::BLEPeripheralManager peripheral;
    
    if (!peripheral.initialize()) {
        std::cerr << "Failed to initialize BLE peripheral" << std::endl;
        return 1;
    }

    // Set up advertisement data
    ggk::BLEPeripheralManager::AdvertisementData advData;
    advData.name = "TestDevice";
    advData.serviceUUIDs = {"180D"}; // Heart Rate Service
    
    if (!peripheral.setAdvertisementData(advData)) {
        std::cerr << "Failed to set advertisement data" << std::endl;
        return 1;
    }

    // Start advertising
    if (!peripheral.startAdvertising()) {
        std::cerr << "Failed to start advertising" << std::endl;
        return 1;
    }

    std::cout << "BLE peripheral is running. Press Ctrl+C to stop." << std::endl;

    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down..." << std::endl;
    return 0;
}