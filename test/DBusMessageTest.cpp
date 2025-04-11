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

// 안전한 기본 동작 테스트 - 생성자 없이 nullptr message 처리
TEST_F(DBusMessageTest, NullMessageReturnsSafeDefaults) {
    // 임시 메시지 생성
    auto validMsg = DBusMessage::createSignal(
        "/org/example/Object",
        "org.example.Interface",
        "TestSignal"
    );

    // move 이후 원래 객체는 "null" 상태가 되어야 함
    DBusMessage moved = std::move(validMsg);

    // validMsg는 이제 null 상태일 것
    EXPECT_EQ(validMsg.getPath(), "");
    EXPECT_EQ(validMsg.getInterface(), "");
    EXPECT_EQ(validMsg.getMember(), "");
    EXPECT_EQ(validMsg.getDestination(), "");
    EXPECT_EQ(validMsg.getSender(), "");
    EXPECT_EQ(validMsg.getSignature(), "");
    EXPECT_EQ(validMsg.getType(), DBusMessageType::ERROR); // default fallback
}
