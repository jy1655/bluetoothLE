#include <gtest/gtest.h>
#include "../include/DBusXml.h"
#include "../include/DBusTypes.h"
#include <gio/gio.h>

using namespace ggk;

using namespace ggk;

class GLibEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // GLib/GIO 초기화 (필요 시)
    }
};

TEST(DBusXmlTest, CreateProperty) {
    DBusProperty prop{"TestProperty", "s", true, false, true, nullptr, nullptr};
    std::string expected =
        "  <property name='TestProperty' type='s' access='read'>\n"
        "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>\n"
        "  </property>\n";

    auto result = DBusXml::createProperty(prop, 1);
    EXPECT_EQ(result, expected);
}

TEST(DBusXmlTest, CreateMethod) {
    std::vector<DBusArgument> inArgs = { {"s", "inputArg", "in"} };
    std::vector<DBusArgument> outArgs = { {"i", "outputArg", "out"} };

    std::string expected =
        "  <method name='TestMethod'>\n"
        "    <arg name='inputArg' type='s' direction='in'/>\n"
        "    <arg name='outputArg' type='i' direction='out'/>\n"
        "  </method>\n";

    auto result = DBusXml::createMethod("TestMethod", inArgs, outArgs, 1);
    EXPECT_EQ(result, expected);
}

TEST(DBusXmlTest, CreateSignal) {
    DBusSignal signal{"TestSignal", { {"s", "signalArg", ""} }};

    std::string expected =
        "  <signal name='TestSignal'>\n"
        "    <arg name='signalArg' type='s'/>\n"
        "  </signal>\n";

    auto result = DBusXml::createSignal(signal, 1);
    EXPECT_EQ(result, expected);
}

TEST(DBusXmlTest, CreateInterface) {
    DBusProperty prop{"InterfaceProperty", "i", true, true, false, nullptr, nullptr};

    std::vector<DBusMethodCall> methods;
    methods.push_back(DBusMethodCall{
        "", "", "InterfaceMethod",
        GVariantPtr(nullptr, g_variant_unref),
        GDBusMethodInvocationPtr(nullptr, g_object_unref)
    });

    DBusSignal signal{"InterfaceSignal", {{"s", "interfaceArg", ""}}};

    std::string result = DBusXml::createInterface(
        "org.example.Interface",
        {prop},
        std::move(methods),
        {signal},
        0
    );

    EXPECT_NE(result.find("<interface name='org.example.Interface'>"), std::string::npos);
    EXPECT_NE(result.find("<property name='InterfaceProperty' type='i' access='readwrite'/>"), std::string::npos);
    EXPECT_NE(result.find("<method name='InterfaceMethod'>"), std::string::npos);
    EXPECT_NE(result.find("<signal name='InterfaceSignal'>"), std::string::npos);
}



