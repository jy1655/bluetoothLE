// main.cpp
#include <sdbus-c++/sdbus-c++.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "BleApplication.h"
#include "BluetoothAdapter.h"  // Include the header, not the cpp

static std::shared_ptr<sdbus::IConnection> g_connection;
static std::shared_ptr<ble::BleApplication> g_app;
static std::shared_ptr<ble::BluetoothAdapter> g_adapter;
static sdbus::ServiceName serviceName{"com.example.ble"};

// Signal handler for clean shutdown
static void signalHandler(int sig) {
    std::cout << "Signal received: " << sig << std::endl;
    
    if (g_app) {
        g_app->unregisterFromBlueZ();
    }
    
    if (g_connection) {
        g_connection->releaseName(serviceName);
        g_connection->leaveEventLoop();
    }
    
    exit(0);
}

// This functionality has been moved to the DataSimulator class

int main() {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "Starting BLE Peripheral application..." << std::endl;
        
        // Create connection to the system bus
        g_connection = sdbus::createSystemBusConnection();
        g_connection->requestName(serviceName);
        g_connection->enterEventLoopAsync();
        
        // Initialize the Bluetooth adapter
        g_adapter = std::make_shared<ble::BluetoothAdapter>(*g_connection);
        
        // Setup permissive BLE settings
        if (!g_adapter->setupPermissions()) {
            std::cerr << "Warning: Failed to set up Bluetooth permissions" << std::endl;
            // Continue anyway, it might still work
        }
        
        // Initialize and power on the adapter
        if (!g_adapter->initialize()) {
            std::cerr << "Failed to initialize Bluetooth adapter" << std::endl;
            return 1;
        }
        
        if (!g_adapter->powerOn()) {
            std::cerr << "Failed to power on Bluetooth adapter" << std::endl;
            return 1;
        }
        
        if (!g_adapter->startAdvertising()) {
            std::cerr << "Failed to configure Bluetooth advertising" << std::endl;
            return 1;
        }
        
        // Create GATT application
        g_app = std::make_shared<ble::BleApplication>(*g_connection, "/com/ble/peripheral");
        
        // Set up BLE services and characteristics
        if (!g_app->setupApplication()) {
            std::cerr << "Failed to set up application" << std::endl;
            return 1;
        }
        
        // Set up advertisement
        if (!g_app->setupAdvertisement("BLE Peripheral")) {
            std::cerr << "Failed to set up advertisement" << std::endl;
            return 1;
        }
        
        // Register with BlueZ (only after everything is set up)
        if (!g_app->registerWithBlueZ()) {
            std::cerr << "Failed to register with BlueZ" << std::endl;
            return 1;
        }
        
        std::cout << "BLE peripheral running successfully!" << std::endl;
        std::cout << "Battery service and custom service are now available." << std::endl;
        std::cout << "Press Ctrl+C to exit." << std::endl;
        
        // Start data simulation
        g_app->startDataSimulation();
        
        // Main thread sleeps until signal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus error: " << e.getName() << " with message: " << e.getMessage() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}