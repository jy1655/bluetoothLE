#include "DBusTestEnvironment.h"
#include "Logger.h"

namespace ggk {

// 정적 멤버 변수 정의
std::unique_ptr<GattApplication> DBusTestEnvironment::application;

DBusConnection& DBusTestEnvironment::getConnection() {
    return DBusName::getInstance().getConnection();
}

GattApplication* DBusTestEnvironment::getGattApplication() {
    return application.get();
}

void DBusTestEnvironment::SetUp() {
    // ✅ 먼저 버스 타입을 설정
    DBusName::getInstance().setBusType(G_BUS_TYPE_SYSTEM);
    
    if (!DBusName::getInstance().initialize()) {
        FAIL() << "Failed to initialize D-Bus for testing";
    }
    
    application = std::make_unique<GattApplication>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/test/gatt")
    );
    
    Logger::info("Global test environment set up successfully");
}

void DBusTestEnvironment::TearDown() {
    // 공유 리소스 정리
    if (application && application->isRegistered()) {
        application->unregisterFromBlueZ();
    }
    application.reset();
    
    DBusName::getInstance().shutdown();
    Logger::info("Global test environment torn down");
}

} // namespace ggk