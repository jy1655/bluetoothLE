// main.cpp
#include "Server.h"
#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattTypes.h"
#include "Logger.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

using namespace ggk;

// 전역 변수 - 종료 플래그
static bool g_running = true;

// 시그널 핸들러
void signalHandler(int sig) {
    g_running = false;
}

// 콘솔 로그 핸들러
void consoleLogger(const char* text) {
    std::cout << text << std::endl;
}

// Battery Service 상수
constexpr uint16_t BATTERY_SERVICE_UUID = 0x180F;
constexpr uint16_t BATTERY_LEVEL_CHAR_UUID = 0x2A19;

// 배터리 레벨 콜백
std::vector<uint8_t> getBatteryLevel() {
    // 매번 랜덤 값으로 배터리 레벨 반환 (테스트용)
    static uint8_t level = 100;
    level = (level > 10) ? level - 1 : 100; // 1씩 감소하고 10 이하면 100으로 리셋
    return {level};
}

int main() {
    // 로그 핸들러 등록
    Logger::registerInfoReceiver(&consoleLogger);
    Logger::registerDebugReceiver(&consoleLogger);
    Logger::registerErrorReceiver(&consoleLogger);
    Logger::registerWarnReceiver(&consoleLogger);
    
    // 시그널 핸들러 등록
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // D-Bus 연결 생성
        DBusConnection connection;
        if (!connection.connect(DBusConnection::BUS_SYSTEM)) {
            Logger::error("Failed to connect to D-Bus system bus");
            return 1;
        }
        
        // 1. BLE 서버 생성 및 초기화
        BleServer server(connection);
        if (!server.initialize()) {
            Logger::error("Failed to initialize BLE server");
            return 1;
        }
        
        // 2. GATT 애플리케이션 생성
        GattApplication app(connection, DBusObjectPath("/com/example/bleserver"));
        if (!app.setupDBusInterfaces()) {
            Logger::error("Failed to setup GATT application");
            return 1;
        }
        
        // 3. Battery Service 생성
        GattUuid batteryServiceUuid = GattUuid::fromShortUuid(BATTERY_SERVICE_UUID);
        GattServicePtr batteryService = app.createService(batteryServiceUuid, true);
        if (!batteryService) {
            Logger::error("Failed to create Battery Service");
            return 1;
        }
        
        // 4. Battery Level 특성 생성
        GattUuid batteryLevelCharUuid = GattUuid::fromShortUuid(BATTERY_LEVEL_CHAR_UUID);
        GattCharacteristicPtr batteryLevelChar = batteryService->createCharacteristic(
            batteryLevelCharUuid,
            static_cast<uint8_t>(GattProperty::READ) | static_cast<uint8_t>(GattProperty::NOTIFY),
            static_cast<uint8_t>(GattPermission::READ)
        );
        
        if (!batteryLevelChar) {
            Logger::error("Failed to create Battery Level characteristic");
            return 1;
        }
        
        // 5. 배터리 읽기 콜백 설정
        batteryLevelChar->setReadCallback(getBatteryLevel);
        
        // 6. 초기 값 설정
        batteryLevelChar->setValue({100});
        
        // 7. 애플리케이션 등록
        if (!app.registerWithBluez(DBusObjectPath("/org/bluez/hci0"))) {
            Logger::error("Failed to register GATT application with BlueZ");
            return 1;
        }
        
        // 8. 광고 시작
        BleServer::AdvertisingData advData;
        advData.localName = "Jetson BLE Device";
        advData.serviceUuids.push_back(batteryServiceUuid);
        
        if (!server.startAdvertising(advData)) {
            Logger::error("Failed to start advertising");
            return 1;
        }
        
        Logger::info("BLE server running. Press Ctrl+C to exit.");
        
        // 메인 루프
        while (g_running) {
            // 주기적으로 배터리 값 업데이트 (알림 발생)
            batteryLevelChar->setValue(getBatteryLevel());
            sleep(5);
        }
        
        // 종료 처리
        server.stopAdvertising();
        app.unregisterFromBluez(DBusObjectPath("/org/bluez/hci0"));
        server.shutdown();
        
    } catch (const std::exception& e) {
        Logger::error(std::string("Exception: ") + e.what());
        return 1;
    }
    
    Logger::info("BLE server stopped.");
    return 0;
}