// include/DBusName.h
#pragma once
#include "SDBusInterface.h"
#include "Logger.h"
#include <string>
#include <memory>

namespace ggk {

/**
 * @brief D-Bus 이름 관리 클래스
 * 
 * D-Bus 이름 등록 및 연결을 관리하는 싱글톤 클래스입니다.
 */
class DBusName {
public:
    /**
     * @brief 싱글톤 인스턴스 획득
     * 
     * @return DBusName 싱글톤 인스턴스 참조
     */
    static DBusName& getInstance();
    
    /**
     * @brief 초기화 - 실제 앱 실행과 테스트에서 모두 호출
     * 
     * @param busName D-Bus 이름 (기본값: "com.example.ble")
     * @return 초기화 성공 여부
     */
    bool initialize(const std::string& busName = "com.example.ble");
    
    /**
     * @brief 종료 처리
     */
    void shutdown();
    
    /**
     * @brief 상태 확인
     * 
     * @return D-Bus 연결 객체 참조
     */
    std::shared_ptr<SDBusConnection> getConnection();
    
    /**
     * @brief D-Bus 이름 획득
     * 
     * @return 등록된 D-Bus 이름
     */
    const std::string& getBusName() const { return busName; }
    
    /**
     * @brief 초기화 상태 확인
     * 
     * @return 초기화 상태
     */
    bool isInitialized() const { 
        return initialized && connection && connection->isConnected(); 
    }
    
    /**
     * @brief D-Bus 이름 획득 상태 확인
     * 
     * @return D-Bus 이름 획득 상태
     */
    bool hasBusName() const { return busNameAcquired; }

#ifdef TESTING
    /**
     * @brief 테스트를 위한 초기화 상태 재설정
     */
    void reset() {
        shutdown();
        initialized = false;
        busNameAcquired = false;
    }
    
    /**
     * @brief 버스 타입 설정 (시스템 버스 vs 세션 버스)
     * 
     * @param useSystemBus 시스템 버스 사용 여부
     */
    void setBusType(bool useSystemBus);
#endif

private:
    // 싱글톤 구현
    DBusName();
    ~DBusName();
    
    // 복사 방지
    DBusName(const DBusName&) = delete;
    DBusName& operator=(const DBusName&) = delete;
    
    // 상태 변수
    std::shared_ptr<SDBusConnection> connection;
    bool useSystemBus;
    std::string busName;
    bool initialized;
    bool busNameAcquired;
};

} // namespace ggk