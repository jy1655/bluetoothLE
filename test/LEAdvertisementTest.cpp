#include <gtest/gtest.h>
#include "LEAdvertisement.h"
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattTypes.h"

using namespace ggk;

class LEAdvertisementIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<DBusConnection> connection;
    std::unique_ptr<LEAdvertisement> advertisement;

    void SetUp() override {
        connection = std::make_unique<DBusConnection>();
        ASSERT_TRUE(connection->connect());  // D-Bus 연결 확인

        advertisement = std::make_unique<LEAdvertisement>(
            *connection,
            DBusObjectPath("/com/example/advertisement")
        );
    }

    void TearDown() override {
        if (advertisement->isRegistered()) {
            advertisement->unregisterFromBlueZ();
        }
        connection->disconnect();
    }
};

TEST_F(LEAdvertisementIntegrationTest, RegisterWithBlueZ) {
    bool result = advertisement->registerWithBlueZ();
    EXPECT_TRUE(result);

    // BLE 광고가 실제로 활성화되었는지 확인
    system("btmgmt info");
}

TEST_F(LEAdvertisementIntegrationTest, UnregisterFromBlueZ) {
    advertisement->registerWithBlueZ();
    bool result = advertisement->unregisterFromBlueZ();
    EXPECT_TRUE(result);
}
