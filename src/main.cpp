// src/main.cpp
#include <sdbus-c++/sdbus-c++.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "GattApplication.h"

static std::shared_ptr<sdbus::IConnection> g_connection;
static std::shared_ptr<ggk::GattApplication> g_app;
static sdbus::ServiceName serviceName{"com.example.ble"};

// 시그널 핸들러
static void signalHandler(int sig) {
    std::cout << "시그널 받음: " << sig << std::endl;
    
    if (g_app) {
        g_app->unregisterFromBlueZ();
    }
    
    if (g_connection) {
        g_connection->releaseName(serviceName);
        g_connection->leaveEventLoop();
    }
    
    exit(0);
}

int main() {
    // 1. Create connection
    auto connection = sdbus::createSystemBusConnection();
    connection->requestName(sdbus::ServiceName("com.example.ble"));
    connection->enterEventLoopAsync();
    
    // 2. Create GATT application
    auto app = std::make_shared<ggk::GattApplication>(*connection, "/com/example/ble");
    
    // 3. Set up all services and characteristics
    if (!app->setupApplication()) {
        std::cerr << "Failed to set up application" << std::endl;
        return 1;
    }
    
    // 4. Set up advertisement
    if (!app->setupAdvertisement("My BLE Device")) {
        std::cerr << "Failed to set up advertisement" << std::endl;
        return 1;
    }
    
    // 5. Register with BlueZ (only after everything is set up)
    if (!app->registerWithBlueZ()) {
        std::cerr << "Failed to register with BlueZ" << std::endl;
        return 1;
    }
    
    // Wait for signals/interrupts
    std::cout << "BLE peripheral running. Press Ctrl+C to exit." << std::endl;
    pause();
    
    // Clean up
    app->unregisterFromBlueZ();
    connection->releaseName(sdbus::ServiceName("com.example.ble"));
    connection->leaveEventLoop();
    
    return 0;
}