#include <gtest/gtest.h>
#include "DBusObject.h"
#include "Logger.h"
#include "DBusTestEnvironment.h"

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
        TestableDBusObject* dbusObject;
        GVariant* testPropertyValue = nullptr;  // static 제거, 각 테스트 인스턴스에 종속
    
    void SetUp() override {
        Logger::info("Setting up DBusObject test environment.");
            
        // DBusTestEnvironment에서 관리하는 연결 사용
        DBusConnection& connection = DBusTestEnvironment::getConnection();
            
        // 테스트 가능한 객체 생성 (매 테스트마다 고유한 경로 사용)
        static int testCounter = 0;
        std::string objPath = "/org/example/TestObject" + std::to_string(++testCounter);
        dbusObject = new TestableDBusObject(connection, DBusObjectPath(objPath));
    
        std::vector<DBusProperty> properties = {
            {"TestProperty", "s", true, true, false,
                [this]() -> GVariant* { 
                    // 저장된 값이 있으면 사용, 없으면 기본값
                    if (this->testPropertyValue) {
                        return g_variant_ref(this->testPropertyValue);
                    }
                    return g_variant_new_string("TestValue"); 
                },
                [this](GVariant* v) { 
                    // 이전 값 해제
                    if (this->testPropertyValue) {
                        g_variant_unref(this->testPropertyValue);
                    }
                    // 새 값 저장
                    this->testPropertyValue = g_variant_ref(v);
                    return true; 
                }
            }
        };
    
        ASSERT_TRUE(dbusObject->addInterface("org.example.TestInterface", properties));
        ASSERT_TRUE(dbusObject->addMethod("org.example.TestInterface", "TestMethod", 
            []([[maybe_unused]] const DBusMethodCall& call) {
                Logger::info("TestMethod invoked.");
            }
        ));
    
        ASSERT_TRUE(dbusObject->registerObject());
    }
    
    void TearDown() override {
        Logger::info("Tearing down DBusObject test environment.");
            
        // 객체 정리
        if (dbusObject) {
            dbusObject->unregisterObject();
            delete dbusObject;
            dbusObject = nullptr;
        }
            
        // 속성 값 정리
        if (testPropertyValue) {
            g_variant_unref(testPropertyValue);
            testPropertyValue = nullptr;
        }
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
    
// 직접 메서드 테스트 (단위 테스트)
TEST_F(DBusObjectTest, GenerateIntrospectionXmlDirectTest) {
    // TestableDBusObject는 이미 getXml() 메서드를 통해 접근을 제공
    std::string xml = dbusObject->getXml();
    
    // XML 내용 검증
    EXPECT_NE(xml.find("<interface name='org.example.TestInterface'>"), std::string::npos);
    EXPECT_NE(xml.find("<method name='TestMethod'>"), std::string::npos);
    EXPECT_NE(xml.find("<property name='TestProperty' type='s'"), std::string::npos);
    
    // 자세한 로깅으로 디버깅 지원
    Logger::debug("Direct XML generation result:\n" + xml);
}

// 통합 테스트 (D-Bus 시스템과의 통합)
TEST_F(DBusObjectTest, IntrospectionXmlIndirectTest) {
    // 1. 먼저 객체가 등록되었는지 확인
    ASSERT_TRUE(dbusObject->isRegistered()) << "Object is not registered";
    
    // 2. 지연 추가 (D-Bus 시스템에 변경사항이 반영될 시간)
    usleep(100000);  // 100ms
    
    // 3. 명시적으로 연결 객체 가져오기
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    // 4. Introspection 쿼리 실행
    GVariantPtr introspectResult = connection.callMethod(
        DBusName::getInstance().getBusName(),  // 우리 프로그램의 서비스 이름
        dbusObject->getPath(),
        "org.freedesktop.DBus.Introspectable",
        "Introspect",
        makeNullGVariantPtr(),
        "(s)"
    );
    
    // 5. 결과 검증 (null 체크)
    if (!introspectResult) {
        FAIL() << "Introspection call returned null result";
        return;
    }
    
    // 6. XML 내용 추출
    const gchar* xml = nullptr;
    g_variant_get(introspectResult.get(), "(&s)", &xml);
    
    if (!xml) {
        FAIL() << "Introspection XML content is null";
        return;
    }
    
    // 7. 결과 로깅
    std::string introspectionXml(xml);
    Logger::debug("Introspection query result:\n" + introspectionXml);
    
    // 8. 내용 검증
    EXPECT_NE(introspectionXml.find("org.example.TestInterface"), std::string::npos);
    EXPECT_NE(introspectionXml.find("TestMethod"), std::string::npos);
}

