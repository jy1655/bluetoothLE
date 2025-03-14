#include <gtest/gtest.h>
#include "DBusObject.h"
#include "Logger.h"

using namespace ggk;

// 테스트 전용 DBusObject 하위 클래스
class TestableDBusObject : public DBusObject {
public:
    using DBusObject::DBusObject;  // 생성자 상속
    
    // protected 메서드 노출
    std::string getXml() const {
        return generateIntrospectionXml();
    }
};

class DBusObjectTest : public ::testing::Test {
protected:
    // 기본 생성자에서 세션 버스로 초기화
    DBusConnection connection{G_BUS_TYPE_SESSION};
    TestableDBusObject* dbusObject;

    void SetUp() override {
        ggk::Logger::info("Setting up DBusObject test environment.");
        // 연결 설정
        ASSERT_TRUE(connection.connect());

        // 테스트 가능한 객체 생성
        dbusObject = new TestableDBusObject(connection, DBusObjectPath("/org/example/TestObject"));

        static GVariant* testPropertyValue = nullptr;
        std::vector<DBusProperty> properties = {
            {"TestProperty", "s", true, true, false,
                []() -> GVariant* { 
                    // 저장된 값이 있으면 사용, 없으면 기본값
                    if (testPropertyValue) {
                        return g_variant_ref(testPropertyValue);
                    }
                    return g_variant_new_string("TestValue"); 
                },
                [](GVariant* v) { 
                    // 이전 값 해제
                    if (testPropertyValue) {
                        g_variant_unref(testPropertyValue);
                    }
                    // 새 값 저장
                    testPropertyValue = g_variant_ref(v);
                    return true; 
                }
            }
        };

        ASSERT_TRUE(dbusObject->addInterface("org.example.TestInterface", properties));
        ASSERT_TRUE(dbusObject->addMethod("org.example.TestInterface", "TestMethod", 
            []([[maybe_unused]] const DBusMethodCall& call) {
                ggk::Logger::info("TestMethod invoked.");
            }
        ));

        ASSERT_TRUE(dbusObject->registerObject());
    }

    void TearDown() override {
        ggk::Logger::info("Tearing down DBusObject test environment.");
        dbusObject->unregisterObject();
        delete dbusObject;
        connection.disconnect();
    }
};

TEST_F(DBusObjectTest, RegisterAndUnregister) {
    EXPECT_TRUE(dbusObject->unregisterObject());  // 먼저 등록 해제
    EXPECT_TRUE(dbusObject->registerObject());    // 다시 등록
    EXPECT_TRUE(dbusObject->unregisterObject());  // 다시 등록 해제
}

TEST_F(DBusObjectTest, EmitSignalTest) {
    bool signalResult = dbusObject->emitSignal("org.example.TestInterface", "TestSignal");
    EXPECT_TRUE(signalResult);
}

TEST_F(DBusObjectTest, SetAndGetProperty) {
    GVariantPtr value(g_variant_new_string("NewTestValue"), &g_variant_unref);

    EXPECT_TRUE(dbusObject->setProperty("org.example.TestInterface", "TestProperty", std::move(value)));

    GVariantPtr retrieved = dbusObject->getProperty("org.example.TestInterface", "TestProperty");
    ASSERT_NE(retrieved.get(), nullptr);
    EXPECT_STREQ(g_variant_get_string(retrieved.get(), nullptr), "NewTestValue");
}

TEST_F(DBusObjectTest, EmitSignal) {
    GVariantPtr params(g_variant_new("(s)", "SignalData"), &g_variant_unref);

    EXPECT_TRUE(dbusObject->emitSignal("org.example.TestInterface", "TestSignal", std::move(params)));
}

TEST_F(DBusObjectTest, IntrospectionXmlIndirectTest) {
    // 튜플 타입 "(s)"로 수정 (단일 문자열 "s"가 아님)
    GVariantPtr introspectResult = connection.callMethod(
        "org.freedesktop.DBus",
        dbusObject->getPath(),
        "org.freedesktop.DBus.Introspectable",
        "Introspect",
        makeNullGVariantPtr(),
        "(s)"  // "s"에서 "(s)"로 변경
    );

    ASSERT_NE(introspectResult.get(), nullptr);

    // 튜플에서 문자열 추출
    const gchar* xml = nullptr;
    g_variant_get(introspectResult.get(), "(&s)", &xml);
    
    ASSERT_NE(xml, nullptr);

    std::string introspectionXml(xml);
    EXPECT_NE(introspectionXml.find("<interface name='org.example.TestInterface'>"), std::string::npos);
    EXPECT_NE(introspectionXml.find("<method name='TestMethod'>"), std::string::npos);
}
