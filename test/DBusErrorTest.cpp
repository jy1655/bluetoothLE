#include <gtest/gtest.h>
#include "../include/DBusError.h"
#include "../include/DBusTypes.h"

using namespace ggk;

TEST(DBusErrorTest, ConstructorWithNameAndMessage) {
    DBusError error(DBusError::ERROR_FAILED, "Failure occurred");

    EXPECT_EQ(error.getName(), DBusError::ERROR_FAILED);
    EXPECT_EQ(error.getMessage(), "Failure occurred");
}

TEST(DBusErrorTest, ConstructFromValidGError) {
    // GError 객체 생성 (직접)
    GError* rawError = g_error_new_literal(
        g_quark_from_string(DBusError::ERROR_NO_REPLY),
        0,
        "No response from server"
    );
    GErrorPtr gerror = makeGErrorPtr(rawError);

    DBusError error(gerror.get());

    EXPECT_EQ(error.getName(), DBusError::ERROR_NO_REPLY);
    EXPECT_EQ(error.getMessage(), "No response from server");
}

TEST(DBusErrorTest, ConstructFromNullGError) {
    DBusError error(nullptr);

    EXPECT_EQ(error.getName(), DBusError::ERROR_FAILED);  // 기본 오류 코드
    EXPECT_EQ(error.getMessage(), "Null error pointer");
}

TEST(DBusErrorTest, ToGErrorConversion) {
    DBusError error(DBusError::ERROR_NOT_SUPPORTED, "This feature is not supported");
    GErrorPtr gerror = error.toGError();

    ASSERT_NE(gerror, nullptr);
    EXPECT_STREQ(gerror->message, "This feature is not supported");
    EXPECT_EQ(std::string(g_quark_to_string(gerror->domain)), DBusError::ERROR_NOT_SUPPORTED);
}

TEST(DBusErrorTest, ToStringFormat) {
    DBusError error(DBusError::ERROR_INVALID_ARGS, "Missing required arguments");
    std::string expected = std::string(DBusError::ERROR_INVALID_ARGS) + ": Missing required arguments";

    EXPECT_EQ(error.toString(), expected);
}

TEST(DBusErrorTest, IsErrorTypePositiveAndNegative) {
    DBusError error(DBusError::ERROR_UNKNOWN_OBJECT, "No such object");

    EXPECT_TRUE(error.isErrorType(DBusError::ERROR_UNKNOWN_OBJECT));
    EXPECT_FALSE(error.isErrorType(DBusError::ERROR_FAILED));
}
