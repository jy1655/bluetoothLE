#include <gtest/gtest.h>
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattTypes.h"
#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "BlueZConstants.h"

using namespace ggk;

class GattTest : public ::testing::Test {
protected:
    std::unique_ptr<DBusConnection> connection;  // 🔹 추가
    std::unique_ptr<GattApplication> app;

    void SetUp() override {
        connection = std::make_unique<DBusConnection>();
        ASSERT_TRUE(connection->connect());  // 🔹 D-Bus 연결 확인
        app = std::make_unique<GattApplication>(*connection);
    }

    void TearDown() override {
        // 먼저 BlueZ에서 등록 해제
        if (app && app->isRegistered()) {
            app->unregisterFromBlueZ();
        }
        
        // 명시적으로 모든 서비스 제거
        if (app) {
            auto services = app->getServices();
            for (auto& service : services) {
                app->removeService(service->getUuid());
            }
        }
        
        // 연결 해제
        app.reset();
        connection->disconnect();
        connection.reset();
        
        // 잠시 대기
        usleep(50000);  // 50ms
    }
};

TEST_F(GattTest, GattService_Creation) {
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    EXPECT_EQ(service->getUuid().toString(), "12345678-1234-5678-1234-56789abcdef0");
    EXPECT_TRUE(service->isPrimary());
}

TEST_F(GattTest, AddGattCharacteristic) {
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    auto characteristic = service->createCharacteristic(
        GattUuid("87654321-4321-6789-4321-56789abcdef0"),
        GattProperty::PROP_READ | GattProperty::PROP_WRITE,
        GattPermission::PERM_READ_ENCRYPTED | GattPermission::PERM_WRITE_ENCRYPTED  // ✅ 수정
    );

    ASSERT_NE(characteristic, nullptr);
    EXPECT_EQ(characteristic->getUuid().toString(), "87654321-4321-6789-4321-56789abcdef0");
    EXPECT_EQ(characteristic->getProperties(), (GattProperty::PROP_READ | GattProperty::PROP_WRITE));
    EXPECT_EQ(characteristic->getPermissions(), (GattPermission::PERM_READ_ENCRYPTED | GattPermission::PERM_WRITE_ENCRYPTED));
}

TEST_F(GattTest, RegisterWithBlueZ) {

    app.reset(new GattApplication(*connection, DBusObjectPath("/com/example/gatt")));

    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    // 서비스에 특성 추가
    auto characteristic = service->createCharacteristic(
        GattUuid("87654321-4321-6789-4321-56789abcdef0"),
        GattProperty::PROP_READ | GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ
    );

    // 서비스를 애플리케이션에 추가
    app->addService(service);

    // 먼저 서비스 인터페이스 설정
    EXPECT_TRUE(service->setupDBusInterfaces());
    
    // 애플리케이션 인터페이스 설정
    EXPECT_TRUE(app->setupDBusInterfaces());
    
    // 객체가 등록되었는지 확인
    EXPECT_TRUE(app->isRegistered());

    // BlueZ에 등록 시도 - 타임아웃은 예상된 문제로 표시하되, 다른 문제는 실패로 처리
    try {
        // 타임아웃을 5초로 줄여서 테스트 속도 개선 (원래 25초 대신)
        bool result = app->registerWithBlueZ();
        
        // 정상 등록 성공 시 확인
        EXPECT_TRUE(result);
        std::cout << "BlueZ registration successful" << std::endl;
    }
    catch (const std::exception& e) {
        std::string error = e.what();
        
        // 타임아웃 확인
        if (error.find("Timeout") != std::string::npos) {
            std::cout << "Expected timeout occurred with BlueZ registration" << std::endl;
            // 타임아웃은 예상된 결과이므로 테스트는 실패로 표시하되, 개발자가 인지할 수 있도록 함
            FAIL() << "BlueZ registration timed out after 5 seconds. This is an expected issue with the current BlueZ implementation.";
        }
        else {
            // 타임아웃 이외의 예외는 실제 문제이므로 실패로 처리
            FAIL() << "Unexpected error during BlueZ registration: " << error;
        }
    }

    // unregisterFromBlueZ 테스트
    try {
        bool unregResult = app->unregisterFromBlueZ();
        EXPECT_TRUE(unregResult);
    }
    catch (const std::exception& e) {
        // 등록 해제 실패는 중요한 문제이므로 실패로 처리
        FAIL() << "Failed to unregister from BlueZ: " << e.what();
    }
}

TEST_F(GattTest, GattCharacteristic_ReadWrite) {
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    auto characteristic = service->createCharacteristic(
        GattUuid("87654321-4321-6789-4321-56789abcdef0"),
        GattProperty::PROP_READ | GattProperty::PROP_WRITE,
        GattPermission::PERM_READ_ENCRYPTED | GattPermission::PERM_WRITE_ENCRYPTED  // ✅ 수정
    );

    app->addService(service);

    GattData testData = {0x12, 0x34, 0x56};
    characteristic->setValue(testData);
    EXPECT_EQ(characteristic->getValue(), testData);
}

TEST_F(GattTest, GattCharacteristic_Notify) {
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    auto characteristic = service->createCharacteristic(
        GattUuid("87654321-4321-6789-4321-56789abcdef0"),
        GattProperty::PROP_NOTIFY,
        GattPermission::PERM_READ_ENCRYPTED  // ✅ 수정
    );

    app->addService(service);

    EXPECT_FALSE(characteristic->isNotifying());
    EXPECT_TRUE(characteristic->startNotify());
    EXPECT_TRUE(characteristic->isNotifying());
    EXPECT_TRUE(characteristic->stopNotify());
    EXPECT_FALSE(characteristic->isNotifying());
}

TEST_F(GattTest, GetManagedObjects) {
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    app->addService(service);

    DBusMethodCall dummyCall;
    app->handleGetManagedObjects(dummyCall);

    auto response = app->createManagedObjectsDict();
    EXPECT_NE(response, nullptr);
}
