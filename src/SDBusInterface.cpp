#include "SDBusInterface.h"
#include "Logger.h"

namespace ggk {

// 싱글톤 인스턴스
static std::unique_ptr<SDBusConnection> g_connection;

SDBusConnection& getSDBusConnection() {
    if (!g_connection) {
        g_connection = std::make_unique<SDBusConnection>(true); // 시스템 버스 사용
        g_connection->connect();
    }
    return *g_connection;
}

SDBusConnection::SDBusConnection(bool useSystemBus)
    : connected(false) {
    try {
        // 적절한 버스 유형에 맞는 연결 생성
        if (useSystemBus) {
            connection = sdbus::createSystemBusConnection();
        } else {
            connection = sdbus::createSessionBusConnection();
        }
        
        Logger::info("SDBusConnection 생성됨");
    }
    catch (const sdbus::Error& e) {
        Logger::error("D-Bus 연결 생성 실패: " + std::string(e.what()));
        connection = nullptr;
    }
}

SDBusConnection::~SDBusConnection() {
    disconnect();
    Logger::info("SDBusConnection 소멸됨");
}

bool SDBusConnection::connect() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (connected) {
        return true;
    }
    
    if (!connection) {
        Logger::error("연결할 수 없음: 연결 객체가 null임");
        return false;
    }
    
    try {
        // 별도 스레드에서 D-Bus 메시지 처리 시작
        connection->enterEventLoopAsync();
        connected = true;
        Logger::info("D-Bus에 연결됨");
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("D-Bus 연결 실패: " + std::string(e.what()));
        return false;
    }
}

bool SDBusConnection::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!connected || !connection) {
        return true;
    }
    
    try {
        // 메시지 처리 중지
        connection->leaveEventLoop();
        connected = false;
        Logger::info("D-Bus 연결 해제됨");
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("연결 해제 중 오류: " + std::string(e.what()));
        return false;
    }
}

bool SDBusConnection::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex);
    return connected && connection != nullptr;
}

bool SDBusConnection::requestName(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!connection) {
        Logger::error("이름을 요청할 수 없음: 연결 객체가 null임");
        return false;
    }
    
    try {
        connection->requestName(name);
        Logger::info("서비스 이름 획득 성공: " + name);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("이름 요청 실패: " + std::string(e.what()));
        return false;
    }
}

bool SDBusConnection::releaseName(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!connection) {
        Logger::error("이름을 해제할 수 없음: 연결 객체가 null임");
        return false;
    }
    
    try {
        connection->releaseName(name);
        Logger::info("서비스 이름 해제 성공: " + name);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("이름 해제 실패: " + std::string(e.what()));
        return false;
    }
}

std::unique_ptr<sdbus::IObject> SDBusConnection::createObject(const std::string& objectPath) {
    if (!connection) {
        Logger::error("객체를 생성할 수 없음: 연결 객체가 null임");
        return nullptr;
    }
    
    try {
        return sdbus::createObject(*connection, objectPath);
    }
    catch (const sdbus::Error& e) {
        Logger::error("객체 생성 실패: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<sdbus::IProxy> SDBusConnection::createProxy(
    const std::string& destination, 
    const std::string& objectPath) {
    if (!connection) {
        Logger::error("프록시를 생성할 수 없음: 연결 객체가 null임");
        return nullptr;
    }
    
    try {
        return sdbus::createProxy(*connection, destination, objectPath);
    }
    catch (const sdbus::Error& e) {
        Logger::error("프록시 생성 실패: " + std::string(e.what()));
        return nullptr;
    }
}

sdbus::IConnection& SDBusConnection::getConnection() {
    if (!connection) {
        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "연결 객체가 null임");
    }
    
    return *connection;
}

} // namespace ggk