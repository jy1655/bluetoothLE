// include/Server.h
#pragma once

#include "GattApplication.h"
#include "GattAdvertisement.h"
#include "DBusName.h"
#include "BlueZConstants.h"
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <string>
#include <mutex>

namespace ggk {

/**
 * @brief BLE 주변장치 서버 클래스
 * 
 * BlueZ를 통해 BLE 주변장치를 구현하는 서버 클래스입니다.
 * GATT 서비스, 특성, 광고를 관리합니다.
 */
class Server {
public:
    /**
     * @brief 생성자
     */
    Server();
    
    /**
     * @brief 소멸자
     */
    ~Server();
    
    /**
     * @brief BLE 스택 초기화
     * 
     * @param deviceName 장치 이름
     * @return 초기화 성공 여부
     */
    bool initialize(const std::string& deviceName = "BLEDevice");
    
    /**
     * @brief BLE 서버 시작
     * 
     * @param secureMode 보안 연결 사용 여부
     * @return 시작 성공 여부
     */
    bool start(bool secureMode = false);
    
    /**
     * @brief BLE 서버 중지
     */
    void stop();
    
    /**
     * @brief 서비스 추가
     * 
     * @param service 추가할 서비스
     * @return 추가 성공 여부
     */
    bool addService(GattServicePtr service);
    
    /**
     * @brief 서비스 생성
     * 
     * @param uuid 서비스 UUID
     * @param isPrimary 주 서비스 여부
     * @return 생성된 서비스
     */
    GattServicePtr createService(const GattUuid& uuid, bool isPrimary = true);
    
    /**
     * @brief 광고 설정
     * 
     * @param name 광고할 이름
     * @param serviceUuids 광고할 서비스 UUID 목록
     * @param manufacturerId 제조사 ID
     * @param manufacturerData 제조사 데이터
     * @param includeTxPower TX 파워 포함 여부
     * @param timeout 광고 제한 시간(초)
     */
    void configureAdvertisement(
        const std::string& name = "",
        const std::vector<GattUuid>& serviceUuids = {},
        uint16_t manufacturerId = 0,
        const std::vector<uint8_t>& manufacturerData = {},
        bool includeTxPower = true,
        uint16_t timeout = 0
    );
    
    /**
     * @brief 이벤트 루프 실행 (차단)
     */
    void run();
    
    /**
     * @brief 이벤트 루프 비동기 실행
     */
    void startAsync();
    
    /**
     * @brief 실행 상태 확인
     * 
     * @return 실행 중 여부
     */
    bool isRunning() const { return running; }
    
    /**
     * @brief GATT 애플리케이션 가져오기
     * 
     * @return GATT 애플리케이션 참조
     */
    GattApplication& getApplication() { return *application; }
    
    /**
     * @brief 광고 가져오기
     * 
     * @return 광고 참조
     */
    GattAdvertisement& getAdvertisement() { return *advertisement; }
    
    /**
     * @brief 장치 이름 가져오기
     * 
     * @return 장치 이름
     */
    const std::string& getDeviceName() const { return deviceName; }
    
    /**
     * @brief 연결 콜백 설정
     * 
     * @param callback 연결 시 호출할 함수
     */
    void setConnectionCallback(std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        connectionCallback = callback;
    }
    
    /**
     * @brief 연결 해제 콜백 설정
     * 
     * @param callback 연결 해제 시 호출할 함수
     */
    void setDisconnectionCallback(std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        disconnectionCallback = callback;
    }
    
    /**
     * @brief 연결된 장치 목록 가져오기
     * 
     * @return 연결된 장치 주소 목록
     */
    std::vector<std::string> getConnectedDevices() const;
    
    /**
     * @brief 장치 연결 상태 확인
     * 
     * @param deviceAddress 확인할 장치 주소
     * @return 연결 상태
     */
    bool isDeviceConnected(const std::string& deviceAddress) const;

private:
    // BLE 구성 요소
    std::unique_ptr<GattApplication> application;
    std::unique_ptr<GattAdvertisement> advertisement;
    
    // 상태 관리
    std::atomic<bool> running;
    std::atomic<bool> initialized;
    std::thread eventThread;
    std::string deviceName;
    uint16_t advTimeout;
    
    // 콜백 함수
    std::function<void(const std::string&)> connectionCallback;
    std::function<void(const std::string&)> disconnectionCallback;
    mutable std::mutex callbacksMutex;
    
    // 내부 메서드
    void eventLoop();
    void setupSignalHandlers();
    void handleConnectionEvent(const std::string& deviceAddress);
    void handleDisconnectionEvent(const std::string& deviceAddress);
    bool setupBlueZInterface();
    bool enableAdvertisingFallback();
    static void registerShutdownHandler(Server* server);

};

} // namespace ggk