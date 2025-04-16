// include/HciAdapter.h
#pragma once

#include <atomic>
#include <string>
#include "SDBusInterface.h"
#include "Logger.h"

namespace ggk {

/**
 * @brief BlueZ 5.82+를 위한 현대적인 HCI 어댑터 클래스
 * 
 * 이 클래스는 직접 HCI 제어 대신 현대적인 BlueZ D-Bus API를 사용하여
 * Bluetooth HCI 어댑터를 관리합니다. 모든 통신은 소켓이 아닌 D-Bus를 통해 
 * 이루어집니다.
 */
class HciAdapter {
public:
    /**
     * @brief 기본 생성자
     */
    HciAdapter();
    
    /**
     * @brief 소멸자
     */
    ~HciAdapter();
    
    /**
     * @brief 어댑터 초기화
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterName 어댑터 이름으로 설정할 이름
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 초기화 성공 여부
     */
    bool initialize(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& adapterName = "BluetoothDevice",
        const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief 중지 및 정리
     */
    void stop();
    
    /**
     * @brief 어댑터가 초기화되었는지 확인
     * 
     * @return 어댑터가 초기화된 경우 true
     */
    bool isInitialized() const;
    
    /**
     * @brief 어댑터 경로 가져오기
     * 
     * @return 어댑터 경로 (예: /org/bluez/hci0)
     */
    const std::string& getAdapterPath() const;
    
    /**
     * @brief 어댑터 이름 설정
     * 
     * @param name 어댑터의 새 이름
     * @return 성공 여부
     */
    bool setName(const std::string& name);
    
    /**
     * @brief 어댑터 전원 상태 설정
     * 
     * @param powered 전원 켜기(true), 끄기(false)
     * @return 성공 여부
     */
    bool setPowered(bool powered);
    
    /**
     * @brief 어댑터 검색 가능 상태 설정
     * 
     * @param discoverable 검색 가능하게 설정(true), 아니면 false
     * @param timeout 타임아웃(초) (0 = 타임아웃 없음)
     * @return 성공 여부
     */
    bool setDiscoverable(bool discoverable, uint16_t timeout = 0);
    
    /**
     * @brief 어댑터를 기본 상태로 재설정
     * 
     * @return 성공 여부
     */
    bool reset();
    
    /**
     * @brief 광고 활성화
     * 
     * @return 성공 여부
     */
    bool enableAdvertising();
    
    /**
     * @brief 광고 비활성화
     * 
     * @return 성공 여부
     */
    bool disableAdvertising();
    
    /**
     * @brief 어댑터가 광고를 지원하는지 확인
     * 
     * @return 광고가 지원되는 경우 true
     */
    bool isAdvertisingSupported() const;
    
    /**
     * @brief D-Bus 연결 가져오기
     * 
     * @return D-Bus 연결 참조
     */
    std::shared_ptr<SDBusConnection> getConnection();
    
private:
    // D-Bus 연결
    std::shared_ptr<SDBusConnection> connection;
    
    // 어댑터 상태
    std::string adapterPath;
    std::string adapterName;
    bool initialized;
    bool advertising;
    
    // 도우미 메서드
    bool verifyAdapterExists();
    bool runBluetoothCtlCommand(const std::vector<std::string>& commands);
};

} // namespace ggk