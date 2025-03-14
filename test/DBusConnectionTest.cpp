#include <gtest/gtest.h>
#include "DBusConnection.h"
#include "DBusName.h"
#include "DBusTestEnvironment.h"
#include "Logger.h"

using namespace ggk;

class DBusConnectionTest : public ::testing::Test {
protected:
    // 테스트 설정 및 정리 로직은 필요하지 않음 - DBusTestEnvironment가 처리
};

// 기존 IsConnectedInitially 테스트를 DBusName을 사용하도록 수정
TEST_F(DBusConnectionTest, IsConnectedInitially) {
    // 직접 DBusConnection 생성하지 않고 DBusName에서 관리하는 연결 사용
    DBusConnection& conn = DBusName::getInstance().getConnection();
    EXPECT_TRUE(conn.isConnected());
}

/**

// DisconnectAndReconnect 테스트
TEST_F(DBusConnectionTest, DisconnectAndReconnect) {
    // 주의: 이 테스트는 전역 연결에 영향을 줄 수 있음
    // 테스트 후 재연결이 필요할 수 있음
    DBusConnection& conn = DBusName::getInstance().getConnection();
    EXPECT_TRUE(conn.isConnected());
    
    // 연결 해제는 테스트하지 않음 (전역 연결에 영향을 줌)
    // 대신 연결 상태만 확인
    EXPECT_TRUE(conn.isConnected());
}

*/

// CallMethod_InvalidDestination 테스트 수정
TEST_F(DBusConnectionTest, CallMethod_InvalidDestination) {
    DBusConnection& connection = DBusName::getInstance().getConnection();
    
    auto result = connection.callMethod(
        "invalid.destination",
        DBusObjectPath("/org/invalid/object"),
        "org.freedesktop.DBus.Introspectable",
        "Introspect"
    );

    EXPECT_EQ(result, nullptr);
}

// RegisterDuplicateObject 테스트 수정
TEST_F(DBusConnectionTest, RegisterDuplicateObject) {
    DBusConnection& connection = DBusName::getInstance().getConnection();

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
}

// DBusName 패턴에서는 복수의 인스턴스가 적절하지 않음, 대체 검증: 동일한 인스턴스 참조 확인
TEST_F(DBusConnectionTest, MultipleInstances) {
    // 이 테스트는 싱글톤 패턴에 맞지 않아 제거하거나 다른 방식으로 검증해야 함
    DBusConnection& conn = DBusName::getInstance().getConnection();
    EXPECT_TRUE(conn.isConnected());
    
    // 대체 검증: 동일한 인스턴스 참조 확인
    DBusConnection& conn2 = DBusName::getInstance().getConnection();
    EXPECT_TRUE(conn2.isConnected());
    
    // 두 참조가 동일한 객체를 가리키는지 확인
    EXPECT_EQ(&conn, &conn2);
}