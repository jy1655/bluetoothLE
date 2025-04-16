// include/BlueZUtils.h
#pragma once

#include <string>
#include <vector>
#include <tuple>
#include "SDBusInterface.h"
#include "GattTypes.h"

namespace ggk {

/**
 * @brief BlueZ 작업을 위한 유틸리티 클래스
 * 
 * D-Bus API를 사용하여 hciconfig와 같은 명령줄 도구 대신
 * 일반적인 BlueZ 작업을 수행하기 위한 도우미 메서드를 제공합니다.
 */
class BlueZUtils {
public:
    /**
     * @brief BlueZ 버전 가져오기
     * 
     * @return std::tuple<int, int, int> 버전 (major, minor, patch)
     */
    static std::tuple<int, int, int> getBlueZVersion();
    
    /**
     * @brief 필요한 BlueZ 패키지가 설치되어 있는지 확인
     * 
     * @return 모든 필요한 패키지가 설치되어 있는 경우 true
     */
    static bool checkRequiredPackages();
    
    /**
     * @brief BlueZ 버전이 충분한지 확인
     * 
     * @return BlueZ 버전이 최소 요구 사항을 충족하는 경우 true
     */
    static bool checkBlueZVersion();
    
    /**
     * @brief BlueZ 서비스 재시작
     * 
     * @return 서비스가 성공적으로 재시작된 경우 true
     */
    static bool restartBlueZService();
    
    /**
     * @brief 사용 가능한 Bluetooth 어댑터 목록 가져오기
     * 
     * @param connection 사용할 D-Bus 연결
     * @return std::vector<std::string> 어댑터 경로 목록
     */
    static std::vector<std::string> getAdapters(std::shared_ptr<SDBusConnection> connection);
    
    /**
     * @brief 기본 어댑터 경로 가져오기
     * 
     * @param connection 사용할 D-Bus 연결
     * @return std::string 기본 어댑터 경로 (일반적으로 /org/bluez/hci0)
     */
    static std::string getDefaultAdapter(std::shared_ptr<SDBusConnection> connection);
    
    /**
     * @brief 어댑터 전원이 켜져 있는지 확인
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 어댑터 전원이 켜져 있는 경우 true
     */
    static bool isAdapterPowered(std::shared_ptr<SDBusConnection> connection, 
                                const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief 어댑터 전원 켜기/끄기
     * 
     * @param connection 사용할 D-Bus 연결
     * @param powered 전원 켜기(true), 끄기(false)
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 작업이 성공한 경우 true
     */
    static bool setAdapterPower(std::shared_ptr<SDBusConnection> connection, 
                               bool powered, 
                               const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief 어댑터 검색 가능 상태 설정
     * 
     * @param connection 사용할 D-Bus 연결
     * @param discoverable 검색 가능하게 설정(true), 아니면 false
     * @param timeout 검색 가능 타임아웃(초) (0 = 타임아웃 없음)
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 작업이 성공한 경우 true
     */
    static bool setAdapterDiscoverable(std::shared_ptr<SDBusConnection> connection, 
                                      bool discoverable,
                                      uint16_t timeout = 0,
                                      const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief 어댑터 이름 설정
     * 
     * @param connection 사용할 D-Bus 연결
     * @param name 새 어댑터 이름
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 작업이 성공한 경우 true
     */
    static bool setAdapterName(std::shared_ptr<SDBusConnection> connection, 
                              const std::string& name,
                              const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief 어댑터를 기본 상태로 재설정
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 작업이 성공한 경우 true
     */
    static bool resetAdapter(std::shared_ptr<SDBusConnection> connection,
                            const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief BLE 주변 장치 모드용 어댑터 초기화
     * 
     * @param connection 사용할 D-Bus 연결
     * @param name 어댑터 이름으로 설정할 이름
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 초기화 성공 여부
     */
    static bool initializeAdapter(std::shared_ptr<SDBusConnection> connection,
                                 const std::string& name,
                                 const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief bluetoothctl용 BlueZ 스크립트 작성
     * 
     * bluetoothctl 명령을 실행하기 위한 셸 스크립트 생성
     * 
     * @param commands bluetoothctl 명령 목록
     * @return std::string 스크립트 내용
     */
    static std::string createBluetoothCtlScript(const std::vector<std::string>& commands);
    
    /**
     * @brief bluetoothctl 명령 실행
     * 
     * @param commands bluetoothctl 명령 목록
     * @return 모든 명령이 성공적으로 실행된 경우 true
     */
    static bool runBluetoothCtlCommands(const std::vector<std::string>& commands);

    /**
     * @brief 연결된 장치 가져오기
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return std::vector<std::string> 연결된 장치 경로 목록
     */
    static std::vector<std::string> getConnectedDevices(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief 광고가 지원되는지 확인
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 광고가 지원되는 경우 true
     */
    static bool isAdvertisingSupported(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief 다양한 방법을 사용하여 광고 활성화 시도
     * 
     * 여러 방법을 시도합니다:
     * 1. D-Bus API
     * 2. bluetoothctl
     * 3. D-Bus 속성 직접 조작
     * 
     * @param connection 사용할 D-Bus 연결
     * @param adapterPath 어댑터 경로 (기본값: /org/bluez/hci0)
     * @return 광고가 활성화된 경우 true
     */
    static bool tryEnableAdvertising(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief 필요한 BlueZ 기능이 사용 가능한지 확인
     * 
     * @param connection 사용할 D-Bus 연결
     * @return 모든 필요한 기능이 사용 가능한 경우 true
     */
    static bool checkBlueZFeatures(std::shared_ptr<SDBusConnection> connection);

    // 어댑터 속성 가져오기 도우미
    static sdbus::Variant getAdapterProperty(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& property,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    // 어댑터 속성 설정 도우미
    static bool setAdapterProperty(
        std::shared_ptr<SDBusConnection> connection,
        const std::string& property,
        const sdbus::Variant& value,
        const std::string& adapterPath = "/org/bluez/hci0");
};

} // namespace ggk