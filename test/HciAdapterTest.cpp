#include <gtest/gtest.h>
#include "HciAdapter.h"
#include "DBusConnection.h"

using namespace ggk;

class HciAdapterTest : public ::testing::Test {
protected:
    DBusConnection connection;
    HciAdapter adapter;

    void SetUp() override {
        ASSERT_TRUE(connection.connectSystemBus()) << "D-Bus system bus connection failed";
        ASSERT_TRUE(adapter.initialize(connection, "JetsonBLE")) << "Failed to initialize HciAdapter";
    }

    void TearDown() override {
        adapter.stop();
    }
};

// ✅ Adapter 초기화 테스트
TEST_F(HciAdapterTest, InitializeTest) {
    EXPECT_TRUE(adapter.isInitialized()) << "Adapter should be initialized";
    EXPECT_EQ(adapter.getAdapterPath(), "/org/bluez/hci0");
}

// ✅ 전원 설정 테스트
TEST_F(HciAdapterTest, SetPoweredTest) {
    EXPECT_TRUE(adapter.setPowered(true)) << "Failed to power on adapter";
    sleep(1);
    EXPECT_TRUE(adapter.setPowered(false)) << "Failed to power off adapter";
}

// ✅ Discoverable 설정 테스트
TEST_F(HciAdapterTest, SetDiscoverableTest) {
    EXPECT_TRUE(adapter.setDiscoverable(true, 5)) << "Failed to set discoverable ON";
    sleep(1);
    EXPECT_TRUE(adapter.setDiscoverable(false)) << "Failed to set discoverable OFF";
}

// ✅ Advertising on/off 테스트
TEST_F(HciAdapterTest, AdvertisingTest) {
    if (!adapter.isAdvertisingSupported()) {
        GTEST_SKIP() << "Advertising not supported on this adapter.";
    }

    EXPECT_TRUE(adapter.enableAdvertising()) << "Failed to enable advertising";
    sleep(1);
    EXPECT_TRUE(adapter.disableAdvertising()) << "Failed to disable advertising";
}

// ✅ 장치 이름 설정 테스트
TEST_F(HciAdapterTest, SetAdapterNameTest) {
    std::string newName = "JetsonBLE-Test";
    EXPECT_TRUE(adapter.setName(newName)) << "Failed to set adapter name";
}

// ✅ Reset 테스트
TEST_F(HciAdapterTest, ResetTest) {
    EXPECT_TRUE(adapter.reset()) << "Adapter reset failed";
}

// ✅ Advertising 지원 여부 확인
TEST_F(HciAdapterTest, CheckAdvertisingSupport) {
    bool supported = adapter.isAdvertisingSupported();
    Logger::info(std::string("Advertising supported: ") + (supported ? "yes" : "no"));
    SUCCEED();  // 단순 확인 목적
}
