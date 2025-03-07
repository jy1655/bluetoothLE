#include <gtest/gtest.h>
#include "DBusMessage.h"
#include "Logger.h"

using namespace ggk;

class DBusMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::info("Setting up DBusMessage test environment.");
    }

    void TearDown() override {
        Logger::info("Tearing down DBusMessage test environment.");
    }
};

TEST_F(DBusMessageTest, CreateMethodCallMessage) {
    auto message = DBusMessage::createMethodCall(
        "org.example.Destination",
        "/org/example/Object",
        "org.example.Interface",
        "TestMethod"
    );

    EXPECT_EQ(message.getDestination(), "org.example.Destination");
    EXPECT_EQ(message.getPath(), "/org/example/Object");
    EXPECT_EQ(message.getInterface(), "org.example.Interface");
    EXPECT_EQ(message.getMember(), "TestMethod");
}

TEST_F(DBusMessageTest, CreateSignalMessage) {
    auto message = DBusMessage::createSignal(
        "/org/example/Object",
        "org.example.Interface",
        "TestSignal"
    );

    EXPECT_EQ(message.getPath(), "/org/example/Object");
    EXPECT_EQ(message.getInterface(), "org.example.Interface");
    EXPECT_EQ(message.getMember(), "TestSignal");
}

// 이 부분은 존재하지 않는 API로 인해 직접적인 생성 불가
/*
TEST_F(DBusMessageTest, CreateErrorMessage) {
    GDBusMessage* dummyMsg = g_dbus_message_new();
    GDBusMethodInvocationPtr invocation(
        g_dbus_method_invocation_new(dummyMsg),
        &g_object_unref
    );

    DBusError error(DBusError::ERROR_FAILED, "Test error");
    auto message = DBusMessage::createError(invocation, error);

    EXPECT_EQ(message.getType(), DBusMessageType::ERROR);
    EXPECT_EQ(message.getInterface(), ""); // 에러 메시지는 인터페이스 없음
    EXPECT_EQ(message.getMember(), ""); // 에러 메시지는 멤버 없음

    g_object_unref(dummyMsg);
}
*/