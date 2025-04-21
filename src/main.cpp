// src/main.cpp
#include <sdbus-c++/sdbus-c++.h>
#include <iostream>
#include <csignal>
#include "GattApplication.h"

static std::shared_ptr<sdbus::IConnection> g_connection;
static std::shared_ptr<ggk::GattApplication> g_app;
sdbus::ServiceName serviceName{"com.example.ble"};

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
    // 시그널 핸들러 등록
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // 1. D-Bus 연결 생성
        g_connection = sdbus::createSystemBusConnection();
        g_connection->requestName(serviceName);

        // 2. GATT 애플리케이션 생성
        g_app = std::make_unique<ggk::GattApplication>(*g_connection, sdbus::ObjectPath{"/com/example/ble"});
        g_app->run();
        
        // 3. 서비스, 특성, 설명자 설정
        if (!g_app->setupApplication()) {
            std::cerr << "애플리케이션 설정 실패" << std::endl;
            return 1;
        }
        
        // 4. 광고 설정
        if (!g_app->setupAdvertisement("Battery Service")) {
            std::cerr << "광고 설정 실패" << std::endl;
            return 1;
        }

        
        
        // 5. BlueZ에 등록
        if (!g_app->registerWithBlueZ()) {
            std::cerr << "BlueZ 등록 실패" << std::endl;
            return 1;
        }
    
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "예외 발생: " << e.what() << std::endl;
        return 1;
    }
}