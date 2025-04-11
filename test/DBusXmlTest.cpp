#include <gtest/gtest.h>
#include "DBusXml.h"
#include "DBusTypes.h"
#include <gio/gio.h>

using namespace ggk;

class DBusXmlTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 필요한 초기화가 있다면 여기에
    }

    void TearDown() override {
        // 정리 로직
    }
};

TEST_F(DBusXmlTest, CreateProperty_ReadOnlyWithSignal) {
    DBusProperty prop{
        "TestProperty",
        "s",
        true,  // readable
        false, // writable
        true,  // emitsChangedSignal
        nullptr,
        nullptr
    };

    const std::string expected =
        "  <property name='TestProperty' type='s' access='read'>\n"
        "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>\n"
        "  </property>\n";

    std::string result = DBusXml::createProperty(prop, 1);
    EXPECT_EQ(result, expected);
}

TEST_F(DBusXmlTest, CreateMethod_WithInOutArgs) {
    std::vector<DBusArgument> inArgs = {
        {"s", "inputArg", "in"}
    };
    std::vector<DBusArgument> outArgs = {
        {"i", "outputArg", "out"}
    };

    const std::string expected =
        "  <method name='TestMethod'>\n"
        "    <arg name='inputArg' type='s' direction='in'/>\n"
        "    <arg name='outputArg' type='i' direction='out'/>\n"
        "  </method>\n";

    std::string result = DBusXml::createMethod("TestMethod", inArgs, outArgs, 1);
    EXPECT_EQ(result, expected);
}

TEST_F(DBusXmlTest, CreateSignal_WithSingleArg) {
    DBusSignal signal{
        "TestSignal",
        {
            {"s", "signalArg"}
        }
    };

    const std::string expected =
        "  <signal name='TestSignal'>\n"
        "    <arg name='signalArg' type='s'/>\n"
        "  </signal>\n";

    std::string result = DBusXml::createSignal(signal, 1);
    EXPECT_EQ(result, expected);
}

TEST_F(DBusXmlTest, CreateInterface_WithAllElements) {
    DBusProperty prop{
        "InterfaceProperty",
        "i",
        true,
        true,
        false,
        nullptr,
        nullptr
    };

    std::vector<DBusMethodCall> methods;
    methods.emplace_back(
        "", "", "InterfaceMethod",
        makeNullGVariantPtr(),
        makeNullGDBusMethodInvocationPtr()
    );

    DBusSignal signal{
        "InterfaceSignal",
        {{"s", "interfaceArg"}}
    };

    std::string xml = DBusXml::createInterface(
        "org.example.Interface",
        {prop},
        methods, // ✅ 복사 없음
        {signal},
        0
    );

    EXPECT_NE(xml.find("<interface name='org.example.Interface'>"), std::string::npos);
    EXPECT_NE(xml.find("<property name='InterfaceProperty' type='i' access='readwrite'/>"), std::string::npos);
    EXPECT_NE(xml.find("<method name='InterfaceMethod'>"), std::string::npos);
    EXPECT_NE(xml.find("<signal name='InterfaceSignal'>"), std::string::npos);
}
