#include "DBusTestEnvironment.cpp"
#include <vector>
#include <string>


// test/DBusObjectTest.cpp
class DBusObjectTest : public ::testing::Test {
    protected:
        TestableDBusObject* dbusObject;
    
        void SetUp() override {
            ggk::Logger::info("Setting up DBusObject test environment.");
            
            // DBusManager가 초기화되었는지 확인
            ASSERT_TRUE(DBusManager::getInstance().isInitialized())
                << "DBusTestEnvironment가 올바르게 설정되지 않았습니다";
    
            // 테스트 객체 경로는 고유해야 함
            static int objectCounter = 0;
            std::string testPath = "/org/example/test/obj" + std::to_string(++objectCounter);
            
            // 중앙화된 DBusManager 사용
            dbusObject = new TestableDBusObject(
                DBusManager::getInstance().getConnection(), 
                DBusObjectPath(testPath)
            );
    
            // 인터페이스 및 메서드 설정
            static GVariant* testPropertyValue = nullptr;
            std::vector<DBusProperty> properties = {
                {"TestProperty", "s", true, true, false,
                    []() -> GVariant* { 
                        if (testPropertyValue) {
                            return g_variant_ref(testPropertyValue);
                        }
                        return g_variant_new_string("TestValue"); 
                    },
                    [](GVariant* v) { 
                        if (testPropertyValue) {
                            g_variant_unref(testPropertyValue);
                        }
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
            // 연결 해제는 하지 않음 - DBusManager가 관리
        }
    };