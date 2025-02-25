#include <gtest/gtest.h>
#include "../include/DBusError.h"
#include <gio/gio.h>

using namespace ggk;

// DBusError 생성자 테스트
TEST(DBusErrorTest, ConstructorTest) {
    DBusError error(DBusErrorCode::FAILED, "Test error message");

    EXPECT_EQ(error.code(), DBusErrorCode::FAILED);
    EXPECT_EQ(error.name(), "org.freedesktop.DBus.Error.Failed");
    EXPECT_EQ(error.message(), "Test error message");
}

// DBusError::fromGError 테스트
TEST(DBusErrorTest, FromGErrorTest) {
    // GError 객체 생성
    GError* gerror = g_error_new_literal(
        g_quark_from_string("org.freedesktop.DBus.Error.Failed"),
        static_cast<gint>(DBusErrorCode::FAILED),
        "Simulated DBus error"
    );

    DBusError error = DBusError::fromGError(gerror);

    EXPECT_EQ(error.code(), DBusErrorCode::FAILED);
    EXPECT_EQ(error.name(), "org.freedesktop.DBus.Error.Failed");
    EXPECT_EQ(error.message(), "Simulated DBus error");

    // GError 객체 해제
    g_error_free(gerror);
}

// DBusError::toGError 테스트
TEST(DBusErrorTest, ToGErrorTest) {
    DBusError error(DBusErrorCode::FAILED, "Test GError conversion");

    GError* gerror = error.toGError();

    EXPECT_STREQ(gerror->message, "Test GError conversion");
    EXPECT_EQ(std::string(g_quark_to_string(gerror->domain)), "org.freedesktop.DBus.Error.Failed");

    // GError 객체 해제
    g_error_free(gerror);
}

// DBusError 에러 코드 ↔ 이름 변환 테스트
TEST(DBusErrorTest, ErrorCodeToNameTest) {
    DBusError error1(DBusErrorCode::FAILED, "Test 1");
    EXPECT_EQ(error1.name(), "org.freedesktop.DBus.Error.Failed");

    DBusError error2(DBusErrorCode::NO_MEMORY, "Test 2");
    EXPECT_EQ(error2.name(), "org.freedesktop.DBus.Error.NoMemory");

    DBusError error3(DBusErrorCode::BLUEZ_FAILED, "Test 3");
    EXPECT_EQ(error3.name(), "org.bluez.Error.Failed");
}

