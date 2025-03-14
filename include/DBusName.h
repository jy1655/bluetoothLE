// include/DBusName.h
#pragma once
#include "DBusConnection.h"
#include "Logger.h"
#include <string>
#include <memory>

namespace ggk {

class DBusName {
public:
    static DBusName& getInstance();
    
    // 초기화 - 실제 앱 실행과 테스트에서 모두 호출
    bool initialize(const std::string& busName = "com.example.ble");
    void shutdown();
    
    // 상태 확인
    DBusConnection& getConnection();
    const std::string& getBusName() const { return busName; }
    bool isInitialized() const { return initialized && connection && connection->isConnected(); }
    bool hasBusName() const { return busNameAcquired; }

#ifdef TESTING
    // 테스트용 메서드
    void reset() {
        shutdown();
        // 인스턴스 상태 초기화
        initialized = false;
        busNameAcquired = false;
    }
    
    // 버스 타입 설정 (시스템 버스 vs 세션 버스)
    void setBusType(GBusType type);
#endif

private:
    DBusName();
    ~DBusName();
    
    // 복사 방지
    DBusName(const DBusName&) = delete;
    DBusName& operator=(const DBusName&) = delete;
    
    std::unique_ptr<DBusConnection> connection;
    GBusType busType;
    std::string busName;
    bool initialized;
    bool busNameAcquired;
};

} // namespace ggk