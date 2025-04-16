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

// 테스트용 UUID 정의
const GattUuid BATTERY_SERVICE_UUID = GattUuid::fromShortUuid(0x180F);  // 표준 배터리 서비스
const GattUuid BATTERY_LEVEL_UUID = GattUuid::fromShortUuid(0x2A19);    // 표준 배터리 레벨 특성

// 사용자 정의 서비스 및 특성 UUID
const GattUuid CUSTOM_SERVICE_UUID("0193d852-eba5-7d28-9abe-e30a67d39d72");
const GattUuid CUSTOM_READ_CHAR_UUID("944ecf35-cdc3-4b74-b477-5bcfe548c98e");
const GattUuid CUSTOM_WRITE_CHAR_UUID("92da1d1a-e24f-4270-8890-8bfcf74b3398");
const GattUuid CUSTOM_NOTIFY_CHAR_UUID("4393fc59-4d51-43ce-a284-cdce8f5fcc7d");

// 테스트용 전역 변수
static std::atomic<bool> g_running(true);

// 시그널 핸들러
static void signalHandler(int signal) {
    std::cout << "받은 시그널: " << signal << std::endl;
    g_running = false;
}

// 배터리 서비스 설정 도우미 함수
GattServicePtr setupBatteryService(Server& server) {
    // 서비스 생성
    auto batteryService = server.createService(BATTERY_SERVICE_UUID);
    if (!batteryService) {
        Logger::error("배터리 서비스 생성 실패");
        return nullptr;
    }
    
    // 배터리 레벨 특성 생성
    auto batteryLevel = batteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    // 초기값 설정
    std::vector<uint8_t> initialValue = {80};  // 80% 배터리
    batteryLevel->setValue(initialValue);
    
    // 읽기 콜백 설정
    batteryLevel->setReadCallback([]() -> std::vector<uint8_t> {
        return {80};  // 항상 80% 반환
    });
    
    // 서버에 서비스 추가
    if (!server.addService(batteryService)) {
        Logger::error("서버에 배터리 서비스 추가 실패");
        return nullptr;
    }
    
    return batteryService;
}

// 사용자 정의 서비스 설정 도우미 함수
GattServicePtr setupCustomService(Server& server) {
    // 서비스 생성
    auto customService = server.createService(CUSTOM_SERVICE_UUID);
    if (!customService) {
        Logger::error("사용자 정의 서비스 생성 실패");
        return nullptr;
    }
    
    // 읽기 특성 설정
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
    
    // 쓰기 특성 설정
    auto writeChar = customService->createCharacteristic(
        CUSTOM_WRITE_CHAR_UUID,
        GattProperty::PROP_WRITE,
        GattPermission::PERM_WRITE
    );
    
    if (writeChar) {
        writeChar->setWriteCallback([](const std::vector<uint8_t>& value) -> bool {
            std::string data(value.begin(), value.end());
            Logger::info("받은 데이터: " + data);
            return true;
        });
    }
    
    // 알림 특성 설정
    auto notifyChar = customService->createCharacteristic(
        CUSTOM_NOTIFY_CHAR_UUID,
        GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );
    
    // 서버에 서비스 추가
    if (!server.addService(customService)) {
        Logger::error("서버에 사용자 정의 서비스 추가 실패");
        return nullptr;
    }
    
    return customService;
}

int main(int argc, char** argv) {
    // 시그널 핸들러 설정
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // BLE 서버 생성 및 초기화
    Server server;
    if (!server.initialize("BLETestDevice")) {
        std::cerr << "BLE 서버 초기화 실패" << std::endl;
        return 1;
    }
    
    std::cout << "BLE 서버 초기화 완료" << std::endl;
    
    // 배터리 서비스 설정
    auto batteryService = setupBatteryService(server);
    if (!batteryService) {
        std::cerr << "배터리 서비스 설정 실패" << std::endl;
        return 1;
    }
    
    std::cout << "배터리 서비스 설정 완료" << std::endl;
    
    // 사용자 정의 서비스 설정
    auto customService = setupCustomService(server);
    if (!customService) {
        std::cerr << "사용자 정의 서비스 설정 실패" << std::endl;
        return 1;
    }
    
    std::cout << "사용자 정의 서비스 설정 완료" << std::endl;
    
    // 광고 설정
    server.configureAdvertisement(
        "BLETestDev",           // 장치 이름
        {},                     // 서비스 UUID (자동으로 추가됨)
        0x0059,                 // 제조사 ID (예: 0x0059 = Nordic Semiconductor)
        {0x01, 0x02, 0x03, 0x04}, // 제조사 데이터
        true,                   // TX 파워 포함
        0                       // 타임아웃 없음
    );
    
    std::cout << "광고 설정 완료" << std::endl;
    
    // 연결 콜백 설정
    server.setConnectionCallback([](const std::string& deviceAddress) {
        std::cout << "클라이언트 연결됨: " << deviceAddress << std::endl;
    });
    
    // 연결 해제 콜백 설정
    server.setDisconnectionCallback([](const std::string& deviceAddress) {
        std::cout << "클라이언트 연결 해제됨: " << deviceAddress << std::endl;
    });
    
    // 서버 시작
    if (!server.start()) {
        std::cerr << "BLE 서버 시작 실패" << std::endl;
        return 1;
    }
    
    std::cout << "BLE 서버 시작 완료 - Ctrl+C로 종료" << std::endl;
    
    // 알림 테스트용 특성 가져오기
    auto notifyChar = customService->getCharacteristic(CUSTOM_NOTIFY_CHAR_UUID);
    auto batteryChar = batteryService->getCharacteristic(BATTERY_LEVEL_UUID);
    
    // 메인 루프 - 주기적으로 값 업데이트
    int count = 0;
    std::vector<uint8_t> batteryValue = {80};
    
    while (g_running) {
        // 배터리 레벨 변경 시뮬레이션 (감소)
        if (batteryChar) {
            batteryValue[0] = 80 - (count % 50);  // 80%에서 31%까지 범위
            batteryChar->setValue(batteryValue);
            std::cout << "배터리 레벨 업데이트: " << static_cast<int>(batteryValue[0]) << "%" << std::endl;
        }
        
        // 주기적 알림 전송
        if (notifyChar) {
            std::string countStr = "Count: " + std::to_string(count);
            std::vector<uint8_t> notifyData(countStr.begin(), countStr.end());
            notifyChar->setValue(notifyData);
            std::cout << "알림 전송: " << countStr << std::endl;
        }
        
        count++;
        
        // 1초 대기
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "종료 중..." << std::endl;
    
    // 서버 중지
    server.stop();
    
    std::cout << "BLE 서버 중지 완료" << std::endl;
    return 0;
}