#include <gtest/gtest.h>
#include "../include/DBusError.h"
#include <gio/gio.h>

using namespace ggk;

TEST(DBusErrorTest, ConstructorTest) {
    DBusError error(DBusError::ERROR_FAILED, "Test error message");
    EXPECT_EQ(error.getName(), DBusError::ERROR_FAILED);
    EXPECT_EQ(error.getMessage(), "Test error message");
}

TEST(DBusErrorTest, FromGErrorTest) {
    GErrorPtr gerror(
        g_error_new_literal(
            g_quark_from_string(DBusError::ERROR_FAILED),
            0,
            "Simulated DBus error"
        ), 
        &g_error_free
    );

    DBusError error(gerror.get());

    EXPECT_EQ(error.getName(), DBusError::ERROR_FAILED);
    EXPECT_EQ(error.getMessage(), "Simulated DBus error");
}

TEST(DBusErrorTest, FromNullGErrorTest) {
    DBusError error(nullptr);
    EXPECT_EQ(error.getName(), DBusError::ERROR_FAILED);
    EXPECT_EQ(error.getMessage(), "Unknown error");
}

TEST(DBusErrorTest, ToGErrorTest) {
    DBusError error(DBusError::ERROR_FAILED, "Test GError conversion");
    GErrorPtr gerror = error.toGError();

    ASSERT_NE(gerror, nullptr);
    EXPECT_STREQ(gerror->message, "Test GError conversion");
    EXPECT_EQ(std::string(g_quark_to_string(gerror->domain)), DBusError::ERROR_FAILED);
}

TEST(DBusErrorTest, ToStringTest) {
    DBusError error(DBusError::ERROR_UNKNOWN_METHOD, "Unknown method called");
    std::string expected = std::string(DBusError::ERROR_UNKNOWN_METHOD) + ": Unknown method called";

    EXPECT_EQ(error.toString(), expected);
}