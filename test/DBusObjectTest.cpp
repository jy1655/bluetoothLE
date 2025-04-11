#include <gtest/gtest.h>
#include "DBusObject.h"
#include "DBusTestEnvironment.h"
#include "Logger.h"
#include "DBusTypes.h"
#include <thread>
#include <chrono>

using namespace ggk;

class TestableDBusObject : public DBusObject {
public:
    using DBusObject::DBusObject;
    std::string getXml() const { return generateIntrospectionXml(); }
};

class DBusObjectTest : public ::testing::Test {
protected:
    TestableDBusObject* dbusObject = nullptr;
    GVariant* testPropertyValue = nullptr;
    inline static int testCounter = 0;

    void SetUp() override {
        Logger::info("Setting up DBusObject test environment.");
        DBusConnection& conn = DBusTestEnvironment::getConnection();

        std::string objPath = "/org/example/TestObject" + std::to_string(++testCounter);
        dbusObject = new TestableDBusObject(conn, DBusObjectPath(objPath));

        std::vector<DBusProperty> props = {
            {
                "TestProperty", "s", true, true, false,
                [this]() -> GVariant* {
                    return testPropertyValue ? g_variant_ref(testPropertyValue)
                                             : g_variant_new_string("TestValue");
                },
                [this](GVariant* val) {
                    if (testPropertyValue)
                        g_variant_unref(testPropertyValue);
                    testPropertyValue = g_variant_ref(val);
                    return true;
                }
            }
        };

        ASSERT_TRUE(dbusObject->addInterface("org.example.TestInterface", props));
        ASSERT_TRUE(dbusObject->addMethod("org.example.TestInterface", "TestMethod",
            [](const DBusMethodCall&) {
                Logger::info("TestMethod invoked.");
            }
        ));

        ASSERT_TRUE(dbusObject->registerObject());
    }

    void TearDown() override {
        Logger::info("Tearing down DBusObject test environment.");
        if (dbusObject) {
            dbusObject->unregisterObject();
            delete dbusObject;
            dbusObject = nullptr;
        }

        if (testPropertyValue) {
            g_variant_unref(testPropertyValue);
            testPropertyValue = nullptr;
        }
    }

    void waitForDBusSync() const {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

// ───────────────────────────────────────────────
// 테스트 케이스들
// ───────────────────────────────────────────────

TEST_F(DBusObjectTest, RegisterThenUnregister_ObjectIsHandledProperly) {
    EXPECT_TRUE(dbusObject->unregisterObject());
    EXPECT_TRUE(dbusObject->registerObject());
    EXPECT_TRUE(dbusObject->unregisterObject());
}

TEST_F(DBusObjectTest, SetAndGetProperty_WorksAsExpected) {
    GVariant* rawValue = g_variant_new_string("NewTestValue");
    GVariantPtr newValue = makeGVariantPtr(rawValue, true);

    EXPECT_TRUE(dbusObject->setProperty("org.example.TestInterface", "TestProperty", std::move(newValue)));

    GVariantPtr readValue = dbusObject->getProperty("org.example.TestInterface", "TestProperty");
    ASSERT_NE(readValue.get(), nullptr);
    EXPECT_STREQ(g_variant_get_string(readValue.get(), nullptr), "NewTestValue");
}

TEST_F(DBusObjectTest, EmitSignalWithoutArgs_Succeeds) {
    EXPECT_TRUE(dbusObject->emitSignal("org.example.TestInterface", "TestSignal"));
}

TEST_F(DBusObjectTest, EmitSignalWithArgs_Succeeds) {
    GVariant* rawParams = g_variant_new("(s)", "SignalData");
    GVariantPtr params = makeGVariantPtr(rawParams, true);

    EXPECT_TRUE(dbusObject->emitSignal("org.example.TestInterface", "TestSignal", std::move(params)));
}

TEST_F(DBusObjectTest, GeneratedXml_ContainsExpectedElements) {
    const std::string xml = dbusObject->getXml();
    EXPECT_NE(xml.find("org.example.TestInterface"), std::string::npos);
    EXPECT_NE(xml.find("TestMethod"), std::string::npos);
    EXPECT_NE(xml.find("TestProperty"), std::string::npos);
    Logger::debug("Generated XML:\n" + xml);
}

TEST_F(DBusObjectTest, DbusIntrospection_ReturnsExpectedXmlFromBus) {
    ASSERT_TRUE(dbusObject->isRegistered());
    waitForDBusSync();

    DBusConnection& conn = DBusTestEnvironment::getConnection();

    GVariantPtr result = conn.callMethod(
        DBusName::getInstance().getBusName(),
        dbusObject->getPath(),
        DBusInterface::INTROSPECTABLE,
        "Introspect",
        makeNullGVariantPtr(),
        "(s)"
    );

    ASSERT_NE(result.get(), nullptr);

    const gchar* xml = nullptr;
    g_variant_get(result.get(), "(&s)", &xml);
    ASSERT_NE(xml, nullptr);

    std::string xmlStr(xml);
    Logger::debug("Returned Introspection XML:\n" + xmlStr);

    EXPECT_NE(xmlStr.find("org.example.TestInterface"), std::string::npos);
    EXPECT_NE(xmlStr.find("TestMethod"), std::string::npos);
}
