#include <gtest/gtest.h>
#include "GattAdvertisement.h"
#include "DBusTestEnvironment.h" 
#include "DBusObjectPath.h"
#include "GattTypes.h"

using namespace ggk;

class GattAdvertisementTest : public ::testing::Test {
protected:
    std::unique_ptr<GattAdvertisement> advertisement;

    void SetUp() override {
        // 공유 D-Bus 연결 사용
        DBusConnection& connection = DBusTestEnvironment::getConnection();

        advertisement = std::make_unique<GattAdvertisement>(
            connection,
            DBusObjectPath("/com/example/advertisement_test")
        );
    }

    void TearDown() override {
        if (advertisement && advertisement->isRegistered()) {
            advertisement->unregisterFromBlueZ();
        }
        advertisement.reset();
        // 공유 연결은 해제하지 않음 (DBusTestEnvironment가 관리)
    }
};

TEST_F(GattAdvertisementTest, RegisterWithBlueZ) {
    bool result = advertisement->registerWithBlueZ();
    EXPECT_TRUE(result);

    // BLE 광고가 실제로 활성화되었는지 확인
    system("btmgmt info");
}

TEST_F(GattAdvertisementTest, UnregisterFromBlueZ) {
    advertisement->registerWithBlueZ();
    bool result = advertisement->unregisterFromBlueZ();
    EXPECT_TRUE(result);
}