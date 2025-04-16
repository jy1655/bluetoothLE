// src/DBusName.cpp
#include "DBusName.h"
#include "Logger.h"

namespace ggk {

DBusName& DBusName::getInstance() {
    static DBusName instance;
    return instance;
}

DBusName::DBusName() 
    : useSystemBus(true), // 실제 하드웨어는 시스템 버스 사용
      initialized(false), 
      busNameAcquired(false) {
}

DBusName::~DBusName() {
    shutdown();
}

#ifdef TESTING
void DBusName::setBusType(bool systemBus) {
    if (initialized) {
        // 이미 초기화된 경우 먼저 연결 해제
        shutdown();
    }
    
    useSystemBus = systemBus;
    connection = std::make_shared<SDBusConnection>(useSystemBus);
}
#endif

bool DBusName::initialize(const std::string& name) {
    if (initialized) {
        return busNameAcquired;
    }
    
    busName = name;
    
    // 연결 생성
    connection = std::make_shared<SDBusConnection>(useSystemBus);
    
    if (!connection || !connection->connect()) {
        Logger::error("D-Bus에 연결 실패");
        return false;
    }
    
    // 버스 네임 요청
    try {
        if (connection->requestName(busName)) {
            Logger::info("버스 이름 획득 성공: " + busName);
            busNameAcquired = true;
        } else {
            Logger::error("버스 이름 획득 실패: " + busName);
            busNameAcquired = false;
        }
    } catch (const std::exception& e) {
        Logger::error("버스 이름 요청 실패: " + std::string(e.what()));
        busNameAcquired = false;
    }
    
    initialized = connection && connection->isConnected();
    return busNameAcquired;
}

void DBusName::shutdown() {
    if (busNameAcquired && connection) {
        try {
            connection->releaseName(busName);
        } catch (...) {
            // 종료 과정에서 예외가 발생해도 무시
        }
        busNameAcquired = false;
    }
    
    if (connection) {
        connection->disconnect();
    }
    connection = nullptr;
    initialized = false;
}

std::shared_ptr<SDBusConnection> DBusName::getConnection() {
    if (!connection) {
        // 연결이 없으면 생성
        connection = std::make_shared<SDBusConnection>(useSystemBus);
        if (!connection->isConnected()) {
            connection->connect();
        }
    }
    return connection;
}

} // namespace ggk