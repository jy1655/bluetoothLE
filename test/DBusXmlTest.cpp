#include <gtest/gtest.h>
#include "../include/DBusXml.h"
#include "../include/DBusTypes.h"
#include <gio/gio.h>

// GLib 환경 초기화 코드
class GLibEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // GLib/GIO 초기화 (필요한 경우)
    }
};

// DBusXml::createProperty() 테스트
TEST(DBusXmlTest, CreatePropertyTest) {
    ggk::DBusProperty property = {"TestProperty", "s", true, false, true, nullptr, nullptr};
    std::string expectedXml =
        "  <property name='TestProperty' type='s' access='read'>\n"
        "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>\n"
        "  </property>\n";

    std::string generatedXml = ggk::DBusXml::createProperty(property, 1);
    EXPECT_EQ(generatedXml, expectedXml);
}

// DBusXml::createMethod() 테스트
TEST(DBusXmlTest, CreateMethodTest) {
    std::vector<ggk::DBusArgument> inArgs = {
        {"i", "arg1", "in"},
        {"s", "arg2", "in"}
    };
    std::vector<ggk::DBusArgument> outArgs = {
        {"b", "result", "out"}
    };

    std::string expectedXml =
        "  <method name='TestMethod'>\n"
        "    <arg name='arg1' type='i' direction='in'/>\n"
        "    <arg name='arg2' type='s' direction='in'/>\n"
        "    <arg name='result' type='b' direction='out'/>\n"
        "  </method>\n";

    std::string generatedXml = ggk::DBusXml::createMethod("TestMethod", inArgs, outArgs, 1);
    EXPECT_EQ(generatedXml, expectedXml);
}

// DBusXml::createSignal() 테스트
TEST(DBusXmlTest, CreateSignalTest) {
    ggk::DBusSignal signal = {
        "TestSignal",
        {{"i", "arg1", ""}, {"s", "arg2", ""}} // direction 필드가 필요 없음
    };

    std::string expectedXml =
        "  <signal name='TestSignal'>\n"
        "    <arg name='arg1' type='i'/>\n"
        "    <arg name='arg2' type='s'/>\n"
        "  </signal>\n";

    std::string generatedXml = ggk::DBusXml::createSignal(signal, 1);
    EXPECT_EQ(generatedXml, expectedXml);
}

// DBusXml::createInterface() 테스트
TEST(DBusXmlTest, CreateInterfaceTest) {
    std::vector<ggk::DBusProperty> properties = {
        {"TestProperty", "s", true, false, true, nullptr, nullptr}
    };

    std::vector<ggk::DBusMethodCall> methods = {
        {"TestSender", "org.example.Interface", "TestMethod", nullptr, nullptr}
    };

    std::vector<ggk::DBusSignal> signals = {
        {"TestSignal", {{"i", "arg1", ""}, {"s", "arg2", ""}}}
    };

    std::string expectedXml =
        "<interface name='org.example.TestInterface'>\n"
        "  <property name='TestProperty' type='s' access='read'>\n"
        "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>\n"
        "  </property>\n"
        "  <method name='TestMethod'>\n"
        "  </method>\n"
        "  <signal name='TestSignal'>\n"
        "    <arg name='arg1' type='i'/>\n"
        "    <arg name='arg2' type='s'/>\n"
        "  </signal>\n"
        "</interface>";

    std::string generatedXml = ggk::DBusXml::createInterface(
        "org.example.TestInterface", properties, methods, signals, 0);

    EXPECT_EQ(generatedXml, expectedXml);
}

