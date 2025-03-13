#include <gtest/gtest.h>
#include "../include/DBusObjectPath.h"

using namespace ggk;

// ✅ 기본 생성자 테스트
TEST(DBusObjectPathTest, DefaultConstructor) {
    DBusObjectPath path;
    EXPECT_EQ(path.toString(), "/");
}

// ✅ 문자열을 이용한 생성자 테스트
TEST(DBusObjectPathTest, StringConstructor) {
    DBusObjectPath path("/org/example/path");
    EXPECT_EQ(path.toString(), "/org/example/path");
}

// ✅ append() 테스트
TEST(DBusObjectPathTest, AppendPath) {
    DBusObjectPath path("/org/example");
    path.append("service");
    EXPECT_EQ(path.toString(), "/org/example/service");
}

// ✅ 연산자 `+=` 테스트
TEST(DBusObjectPathTest, AppendOperator) {
    DBusObjectPath path("/org/example");
    path += "device";
    EXPECT_EQ(path.toString(), "/org/example/device");
}

// ✅ `==` 연산자 테스트
TEST(DBusObjectPathTest, EqualityOperator) {
    DBusObjectPath path1("/org/example");
    DBusObjectPath path2("/org/example");
    EXPECT_TRUE(path1 == path2);
}

