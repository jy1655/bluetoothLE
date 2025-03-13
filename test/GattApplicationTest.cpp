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
    void SetUp() override {
        connection = std::make_unique<DBusConnection>();
        ASSERT_TRUE(connection->connect());  // 🔹 D-Bus 연결 확인
        app = std::make_unique<GattApplication>(*connection);
    }

    void TearDown() override {
        app.reset();
        connection->disconnect();
        connection.reset();
    }

    std::unique_ptr<DBusConnection> connection;
    std::unique_ptr<GattApplication> app;
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
    auto service = std::make_shared<GattService>(
        *connection,
        DBusObjectPath("/com/example/gatt/service1"),
        GattUuid("12345678-1234-5678-1234-56789abcdef0"),
        true
    );

    app->addService(service);

    EXPECT_TRUE(app->registerWithBlueZ());
    EXPECT_TRUE(app->isRegistered());

    EXPECT_TRUE(app->unregisterFromBlueZ());
    EXPECT_FALSE(app->isRegistered());
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
