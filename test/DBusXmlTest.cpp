#include <gtest/gtest.h>
#include "../include/DBusXml.h"

using namespace ggk;

class DBusXmlTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 초기화 코드 (필요할 경우)
    }
    
    void TearDown() override {
        // 정리 코드 (필요할 경우)
    }
};

// ✅ 1. `createInterface()` 테스트
TEST(DBusXmlTest, CreateInterfaceTest) {
    std::string name = "org.example.Test";
    std::string content = "  <method name=\"TestMethod\"/>\n";
    std::string expected =
        "<interface name=\"org.example.Test\">\n"
        "  <method name=\"TestMethod\"/>\n"
        "</interface>\n";

    EXPECT_EQ(DBusXml::createInterface(name, content, 0), expected);
}

// ✅ 2. `createMethod()` 테스트
TEST(DBusXmlTest, CreateMethodTest) {
    std::map<std::string, std::string> args = {{"arg1", "s"}, {"arg2", "i"}};
    std::string expected =
        "  <method name=\"TestMethod\">\n"
        "    <arg type=\"s\" name=\"arg1\" direction=\"in\"/>\n"
        "    <arg type=\"i\" name=\"arg2\" direction=\"in\"/>\n"
        "  </method>\n";

    EXPECT_EQ(DBusXml::createMethod("TestMethod", args, "", 1), expected);
}

// ✅ 3. `createSignal()` 테스트
TEST(DBusXmlTest, CreateSignalTest) {
    std::map<std::string, std::string> args = {{"arg1", "s"}};
    std::string expected =
        "  <signal name=\"TestSignal\">\n"
        "    <arg type=\"s\" name=\"arg1\"/>\n"
        "  </signal>\n";

    EXPECT_EQ(DBusXml::createSignal("TestSignal", args, "", 1), expected);
}

// ✅ 4. `createProperty()` 테스트
TEST(DBusXmlTest, CreatePropertyTest) {
    std::string expected =
        "  <property name=\"TestProperty\" type=\"s\" access=\"readwrite\">\n"
        "    <annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>\n"
        "  </property>\n";

    EXPECT_EQ(DBusXml::createProperty("TestProperty", "s", "readwrite", true, "", 1), expected);
}

// ✅ 5. `createArgument()` 테스트
TEST(DBusXmlTest, CreateArgumentTest) {
    std::string expected =
        "    <arg type=\"s\" name=\"arg1\" direction=\"in\"/>\n";

    EXPECT_EQ(DBusXml::createArgument("s", "in", "arg1", "", 2), expected);
}

// ✅ 6. `createAnnotation()` 테스트
TEST(DBusXmlTest, CreateAnnotationTest) {
    std::string expected =
        "    <annotation name=\"org.freedesktop.DBus.Description\" value=\"Test annotation\"/>\n";

    EXPECT_EQ(DBusXml::createAnnotation("org.freedesktop.DBus.Description", "Test annotation", 2), expected);
}

// ✅ 7. `escape()` 테스트 (XML 특수 문자 변환 확인)
TEST(DBusXmlTest, EscapeTest) {
    EXPECT_EQ(DBusXml::escape("&"), "&amp;");
    EXPECT_EQ(DBusXml::escape("<"), "&lt;");
    EXPECT_EQ(DBusXml::escape(">"), "&gt;");
    EXPECT_EQ(DBusXml::escape("\""), "&quot;");
    EXPECT_EQ(DBusXml::escape("'"), "&apos;");
}
