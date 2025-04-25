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

// main.cpp 수정
int main() {
    // 신호 처리기 등록
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "Starting BLE Peripheral application..." << std::endl;
        
        // 시스템 버스에 연결 생성
        g_connection = sdbus::createSystemBusConnection();
        g_connection->requestName(serviceName);
        g_connection->enterEventLoopAsync();
        
        // 블루투스 어댑터 초기화
        g_adapter = std::make_shared<ble::BluetoothAdapter>(*g_connection);
        
        // 이 부분은 사용자가 사전에 수동으로 설정하도록 안내
        std::cout << "Note: Ensure Bluetooth permissions are properly configured" << std::endl;
        std::cout << "If needed, run: 'sudo systemctl restart bluetooth'" << std::endl;
        
        // 어댑터 초기화 및 전원 켜기
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
        
        // GATT 애플리케이션 생성
        g_app = std::make_shared<ble::BleApplication>(*g_connection, "/com/example/ble");
        
        // BLE 서비스 및 특성 설정
        if (!g_app->setupApplication()) {
            std::cerr << "Failed to set up application" << std::endl;
            // 정리하고 종료
            g_app.reset();
            g_adapter.reset();
            g_connection->releaseName(serviceName);
            g_connection->leaveEventLoop();
            return 1;
        }
        
        // 광고 설정
        if (!g_app->setupAdvertisement("BLE Peripheral")) {
            std::cerr << "Failed to set up advertisement" << std::endl;
            // 정리하고 종료
            g_app.reset();
            g_adapter.reset();
            g_connection->releaseName(serviceName);
            g_connection->leaveEventLoop();
            return 1;
        }
        
        // BlueZ에 등록 (모든 것이 설정된 후)
        if (!g_app->registerWithBlueZ()) {
            std::cerr << "Failed to register with BlueZ" << std::endl;
            // 정리하고 종료
            g_app.reset();
            g_adapter.reset();
            g_connection->releaseName(serviceName);
            g_connection->leaveEventLoop();
            return 1;
        }
        
        std::cout << "BLE peripheral running successfully!" << std::endl;
        std::cout << "Battery service and custom service are now available." << std::endl;
        std::cout << "Press Ctrl+C to exit." << std::endl;
        
        // 데이터 시뮬레이션 시작
        g_app->startDataSimulation();
        
        // 메인 스레드는 신호가 올 때까지 대기
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus error: " << e.getName() << " with message: " << e.getMessage() << std::endl;
        // 정리
        if (g_app) g_app.reset();
        if (g_adapter) g_adapter.reset();
        if (g_connection) {
            g_connection->releaseName(serviceName);
            g_connection->leaveEventLoop();
        }
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // 정리
        if (g_app) g_app.reset();
        if (g_adapter) g_adapter.reset();
        if (g_connection) {
            g_connection->releaseName(serviceName);
            g_connection->leaveEventLoop();
        }
        return 1;
    }
    
    return 0;
}