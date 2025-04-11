#include <gtest/gtest.h>
#include "DBusConnection.h"
#include "DBusName.h"
#include "DBusTestEnvironment.h"
#include "Logger.h"

using namespace ggk;

class DBusConnectionTest : public ::testing::Test {
protected:
    DBusConnection& conn = DBusTestEnvironment::getConnection();
};

// ✅ 초기 연결 상태 확인
TEST_F(DBusConnectionTest, IsConnectedInitially) {
    EXPECT_TRUE(conn.isConnected());
}

// ✅ 잘못된 목적지로 메서드 호출 시 nullptr 반환
TEST_F(DBusConnectionTest, CallMethod_InvalidDestination) {
    auto result = conn.callMethod(
        "invalid.destination",
        DBusObjectPath("/org/invalid/object"),
        "org.freedesktop.DBus.Introspectable",
        "Introspect"
    );

    EXPECT_EQ(result, nullptr);
}

// ✅ 동일한 객체를 중복 등록할 경우 실패해야 함
TEST_F(DBusConnectionTest, RegisterDuplicateObject) {
    std::string validXml = R"xml(
        <node>
          <interface name='org.example.TestInterface'>
            <method name='TestMethod'>
            </method>
          </interface>
        </node>
    )xml";

    DBusObjectPath testPath("/org/example/Test");

    // 첫 등록은 성공
    ASSERT_TRUE(conn.registerObject(
        testPath,
        validXml,
        {},
        {}
    ));

    // 동일한 경로로 다시 등록하면 실패
    EXPECT_FALSE(conn.registerObject(
        testPath,
        validXml,
        {},
        {}
    ));

    // 정리
    ASSERT_TRUE(conn.unregisterObject(testPath));
}

// ✅ 싱글톤 연결 객체가 동일한 인스턴스를 참조하는지 확인
TEST_F(DBusConnectionTest, SingletonInstanceIdentity) {
    DBusConnection& conn2 = DBusTestEnvironment::getConnection();
    
    EXPECT_TRUE(conn2.isConnected());
    EXPECT_EQ(&conn, &conn2);
}

TEST_F(DBusConnectionTest, EmitSignal_BroadcastsSuccessfully) {
    DBusConnection& connection = DBusName::getInstance().getConnection();

    DBusObjectPath path("/org/example/TestSignal");

    // 시그널 전송 테스트
    GVariant* rawParam = g_variant_new("(s)", "test");
    GVariantPtr param = makeGVariantPtr(g_variant_ref_sink(rawParam), true);

    bool success = connection.emitSignal(
        path,
        "org.example.TestInterface",
        "TestSignal",
        std::move(param)
    );

    EXPECT_TRUE(success);  // 전송 성공 여부만 확인
}



