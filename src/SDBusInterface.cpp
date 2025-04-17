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
    std::lock_guard<std::mutex> lock(connectionMutex);
    
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
    std::lock_guard<std::mutex> lock(connectionMutex);
    
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
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connected && connection != nullptr;
}

bool SDBusConnection::requestName(const std::string& name) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    if (!connection) {
        Logger::error("이름을 요청할 수 없음: 연결 객체가 null임");
        return false;
    }
    
    try {
        // sdbus-c++ 2.1.0에서는 BusName 타입으로 변환 필요
        connection->requestName(sdbus::BusName(name));
        Logger::info("서비스 이름 획득 성공: " + name);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("이름 요청 실패: " + std::string(e.what()));
        return false;
    }
}

bool SDBusConnection::releaseName(const std::string& name) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    if (!connection) {
        Logger::error("이름을 해제할 수 없음: 연결 객체가 null임");
        return false;
    }
    
    try {
        // sdbus-c++ 2.1.0에서는 BusName 타입으로 변환 필요
        connection->releaseName(sdbus::BusName(name));
        Logger::info("서비스 이름 해제 성공: " + name);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("이름 해제 실패: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<sdbus::IObject> SDBusConnection::createObject(const std::string& objectPath) {
    if (!connection) {
        Logger::error("객체를 생성할 수 없음: 연결 객체가 null임");
        return nullptr;
    }
    
    try {
        // sdbus-c++ 2.1.0에서는 ObjectPath 타입으로 변환 필요
        // 또한 std::shared_ptr로 반환
        return sdbus::createObject(*connection, sdbus::ObjectPath(objectPath));
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
        // sdbus-c++ 2.1.0에서는 BusName과 ObjectPath 타입으로 변환 필요
        return sdbus::createProxy(*connection, sdbus::BusName(destination), sdbus::ObjectPath(objectPath));
    }
    catch (const sdbus::Error& e) {
        Logger::error("프록시 생성 실패: " + std::string(e.what()));
        return nullptr;
    }
}

sdbus::IConnection& SDBusConnection::getConnection() {
    if (!connection) {
        // sdbus-c++ 2.1.0에서는 Error 생성자가 변경됨
        throw sdbus::Error(sdbus::Error::Name("org.freedesktop.DBus.Error.Failed"), "연결 객체가 null임");
    }
    
    return *connection;
}

void SDBusConnection::enterEventLoop() {
    if (!connection) {
        Logger::error("이벤트 루프를 시작할 수 없음: 연결 객체가 null임");
        return;
    }
    
    try {
        connection->enterEventLoop();
    }
    catch (const sdbus::Error& e) {
        Logger::error("이벤트 루프 실행 중 오류: " + std::string(e.what()));
    }
}

void SDBusConnection::leaveEventLoop() {
    if (!connection) {
        Logger::error("이벤트 루프를 종료할 수 없음: 연결 객체가 null임");
        return;
    }
    
    try {
        connection->leaveEventLoop();
    }
    catch (const sdbus::Error& e) {
        Logger::error("이벤트 루프 종료 중 오류: " + std::string(e.what()));
    }
}

} // namespace ggk