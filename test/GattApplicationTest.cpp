#include <gtest/gtest.h>
#include "GattApplication.h"
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattTypes.h"
#include <thread>
#include <chrono>

using namespace ggk;

class GattApplicationRealTest : public ::testing::Test {
protected:
    void SetUp() override {
        static int testCounter = 0;
        dbusPath = DBusObjectPath("/org/example/test" + std::to_string(testCounter++));
        dbusConnection = std::make_unique<DBusConnection>(G_BUS_TYPE_SYSTEM);

        ASSERT_TRUE(dbusConnection->connect()) << "DBus 연결 실패";

        app = std::make_unique<GattApplication>(*dbusConnection, dbusPath);
        adapterPath = DBusObjectPath("/org/bluez/hci0"); // 실제 블루투스 어댑터 경로
    }

    void TearDown() override {
        // isRegistered() 메서드 사용
        if (app->isRegistered) {  // 게터 메서드 사용
            app->unregisterFromBluez(adapterPath);
        }
        
        app.reset();
        dbusConnection.reset();
    }

    std::unique_ptr<DBusConnection> dbusConnection;
    std::unique_ptr<GattApplication> app;
    DBusObjectPath dbusPath;
    DBusObjectPath adapterPath;
};

// **GattApplication 객체 생성 테스트**
TEST_F(GattApplicationRealTest, Constructor_InitializesProperly) {
    EXPECT_EQ(app->getPath().toString(), "/org/example/test0");
    // 초기 상태에서는 등록되지 않아야 함
    EXPECT_FALSE(app->isRegistered);
}

// **GattApplication의 D-Bus 인터페이스 설정 테스트**
TEST_F(GattApplicationRealTest, SetupDBusInterfaces_Success) {
    EXPECT_TRUE(app->setupDBusInterfaces());

    // D-Bus 객체 등록 확인 (DBusObject::isRegistered() 메서드가 있다면)
    EXPECT_TRUE(app->DBusObject::isRegistered());
    
    // 그러나 BlueZ 등록은 아직 안 됨
    EXPECT_FALSE(app->isRegistered);
}

// 에러 케이스 테스트 추가
TEST_F(GattApplicationRealTest, RegisterWithInvalidPath_Fails) {
    ASSERT_TRUE(app->setupDBusInterfaces()) << "DBus 인터페이스 설정 실패";
    
    // 잘못된 어댑터 경로로 등록 시도
    DBusObjectPath invalidPath("/org/bluez/invalid1");
    EXPECT_FALSE(app->registerWithBluez(invalidPath));
}

// **GattApplication의 BlueZ 등록 테스트**
TEST_F(GattApplicationRealTest, RegisterWithBluez_Success) {
    ASSERT_TRUE(app->setupDBusInterfaces()) << "DBus 인터페이스 설정 실패";
    
    EXPECT_TRUE(app->registerWithBluez(adapterPath));

    // 2초 대기 후 블루투스 서비스 확인
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 등록 후 상태 확인
    EXPECT_TRUE(app->isRegistered);

    // 테스트 완료 후 원상복구
    EXPECT_TRUE(app->unregisterFromBluez(adapterPath));
}

// **GattApplication의 BlueZ 등록 해제 테스트**
TEST_F(GattApplicationRealTest, UnregisterFromBluez_Success) {
    ASSERT_TRUE(app->setupDBusInterfaces()) << "DBus 인터페이스 설정 실패";
    ASSERT_TRUE(app->registerWithBluez(adapterPath)) << "BlueZ 등록 실패";

    EXPECT_TRUE(app->unregisterFromBluez(adapterPath));

    // 2초 대기 후 확인
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 등록 해제 후 상태 확인
    EXPECT_FALSE(app->isRegistered);
}

// **GattApplication에서 GattService 생성 및 D-Bus 반영 확인 테스트**
TEST_F(GattApplicationRealTest, CreateAndRetrieveService) {
    GattUuid serviceUuid("12345678-1234-5678-1234-56789abcdef0");
    
    auto service = app->createService(serviceUuid, true);
    ASSERT_NE(service, nullptr) << "GattService 생성 실패";

    auto retrievedService = app->getService(serviceUuid);
    ASSERT_NE(retrievedService, nullptr) << "GattService 조회 실패";
    EXPECT_EQ(service, retrievedService);

    // D-Bus에 등록되었는지 확인
    auto servicesMap = app->getServices();
    EXPECT_EQ(servicesMap.size(), 1);
}

// **GattApplication에서 존재하지 않는 GattService 조회 테스트**
TEST_F(GattApplicationRealTest, GetService_NotFound) {
    GattUuid invalidUuid("00000000-0000-0000-0000-000000000000");

    auto retrievedService = app->getService(invalidUuid);
    EXPECT_EQ(retrievedService, nullptr);
}
