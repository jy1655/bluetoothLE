#include "DBusTestEnvironment.h"
#include "Logger.h"

namespace ggk {

// 기본값 설정
std::string DBusTestEnvironment::testBusName = "com.aidall.oculo";

DBusConnection& DBusTestEnvironment::getConnection() {
    return DBusName::getInstance().getConnection();
}

void DBusTestEnvironment::setTestBusName(const std::string& name) {
    testBusName = name;
}

void DBusTestEnvironment::SetUp() {
    Logger::info("Setting up DBusTestEnvironment");

    DBusName::getInstance().setBusType(G_BUS_TYPE_SYSTEM);

    if (!DBusName::getInstance().initialize(testBusName)) {
        FAIL() << "Failed to initialize D-Bus with name: " << testBusName;
    }

    Logger::info("DBusTestEnvironment initialized with bus name: " + testBusName);
}

void DBusTestEnvironment::TearDown() {
    DBusName::getInstance().shutdown();
    Logger::info("DBusTestEnvironment torn down");
}

} // namespace ggk
