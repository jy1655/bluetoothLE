#include <gtest/gtest.h>
#include "DBusConnection.h"
#include "Logger.h"

using namespace ggk;

class DBusEnvironment : public ::testing::Environment {
public:
    DBusConnection connection;

    void SetUp() override {
        Logger::info("Setting up DBus test environment.");
        ASSERT_TRUE(connection.connect());
    }

    void TearDown() override {
        Logger::info("Tearing down DBus test environment.");
        connection.disconnect();
    }
};

TEST(DBusConnectionTest, ConnectAndDisconnectTest) {
    DBusConnection conn;
    EXPECT_FALSE(conn.isConnected());

    ASSERT_TRUE(conn.connect());
    EXPECT_TRUE(conn.isConnected());

    EXPECT_TRUE(conn.disconnect());
    EXPECT_FALSE(conn.isConnected());
}

TEST(DBusConnectionTest, CallMethod_InvalidDestination) {
    DBusConnection connection;
    ASSERT_TRUE(connection.connect());

    auto result = connection.callMethod(
        "invalid.destination",
        DBusObjectPath("/org/invalid/object"),
        "org.freedesktop.DBus.Introspectable",
        "Introspect"
    );

    EXPECT_EQ(result, nullptr);
}

TEST(DBusConnectionTest, EmitSignal_WithoutRegistration) {
    DBusConnection connection;
    ASSERT_TRUE(connection.connect());

    bool result = connection.emitSignal(
        DBusObjectPath("/org/example/Test"),
        "org.example.Interface",
        "TestSignal"
    );

    EXPECT_TRUE(result);
}

TEST(DBusConnectionTest, RegisterAndUnregisterObject) {
    DBusConnection connection;
    ASSERT_TRUE(connection.connect());

    std::string validXml = R"xml(
        <node>
          <interface name='org.example.TestInterface'>
            <method name='TestMethod'>
            </method>
          </interface>
        </node>
    )xml";

    DBusObjectPath testPath("/org/example/Test");

    ASSERT_TRUE(connection.registerObject(
        testPath,
        validXml,
        {},
        {}
    ));

    EXPECT_FALSE(connection.registerObject(
        testPath,
        validXml,
        {},
        {}
    ));

    ASSERT_TRUE(connection.unregisterObject(testPath));
    EXPECT_FALSE(connection.unregisterObject(testPath));
}
