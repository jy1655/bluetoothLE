// include/ConnectionManager.h
#pragma once

#include "SDBusInterface.h"
#include "DBusObjectPath.h"  // 이 클래스도 SDBus 기반으로 업데이트해야 합니다
#include "BlueZConstants.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>

namespace ggk {

/**
 * @brief BLE 장치 연결 관리 클래스
 * 
 * BlueZ D-Bus 신호를 통해 연결된 장치를 관리합니다
 */
class ConnectionManager {
public:
    // 콜백 타입
    using ConnectionCallback = std::function<void(const std::string& deviceAddress)>;
    using PropertyChangedCallback = std::function<void(const std::string& interface, 
                                                      const std::string& property, 
                                                      const sdbus::Variant& value)>;

    /**
     * @brief 싱글톤 인스턴스 가져오기
     * 
     * @return 싱글톤 인스턴스 참조
     */
    static ConnectionManager& getInstance();

    /**
     * @brief 초기화
     * 
     * @param connection D-Bus 연결
     * @return 성공 여부
     */
    bool initialize(std::shared_ptr<SDBusConnection> connection);
    
    /**
     * @brief 종료
     */
    void shutdown();

    /**
     * @brief 연결 콜백 설정
     * 
     * @param callback 연결 시 호출할 함수
     */
    void setOnConnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief 연결 해제 콜백 설정
     * 
     * @param callback 연결 해제 시 호출할 함수
     */
    void setOnDisconnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief 속성 변경 콜백 설정
     * 
     * @param callback 속성 변경 시 호출할 함수
     */
    void setOnPropertyChangedCallback(PropertyChangedCallback callback);

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
    
    /**
     * @brief 초기화 상태 확인
     * 
     * @return 초기화 상태
     */
    bool isInitialized() const;

private:
    ConnectionManager();
    ~ConnectionManager();

    // 복사 방지
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 신호 처리
    void registerSignalHandlers();
    void handleInterfacesAddedSignal(const std::string& signalName, const sdbus::Variant& parameters);
    void handleInterfacesRemovedSignal(const std::string& signalName, const sdbus::Variant& parameters);
    void handlePropertiesChangedSignal(const std::string& signalName, const sdbus::Variant& parameters);

    // 연결 데이터
    std::map<std::string, std::string> connectedDevices;  // 객체 경로를 문자열로 처리
    mutable std::mutex devicesMutex;

    // 콜백
    ConnectionCallback onConnectionCallback;
    ConnectionCallback onDisconnectionCallback;
    PropertyChangedCallback onPropertyChangedCallback;

    // 신호 처리
    std::shared_ptr<SDBusConnection> connection;
    std::vector<uint32_t> signalHandlerIds;
    bool initialized;
};

} // namespace ggk