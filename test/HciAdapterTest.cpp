#include <gtest/gtest.h>
#include "../include/HciAdapter.h"

using namespace ggk;

class HciAdapterTest : public ::testing::Test {
protected:
    HciAdapter adapter;

    // BLE 초기 상태 저장
    bool initialPowered;
    bool initialLEEnabled;
    bool initialAdvertisingEnabled;
    std::string initialAdapterName;
    
    void SetUp() override {
        ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";
    
        // ✅ BLE 현재 상태 저장
        initialPowered = adapter.setPowered(true);  // 전원 상태 저장
        initialLEEnabled = adapter.setLEEnabled(true);  // LE 모드 상태 저장
        initialAdvertisingEnabled = adapter.setAdvertisingEnabled(false);  // 광고 상태 저장
        initialAdapterName = "JetsonBLE";  // 원래 이름 (필요시 hciconfig로 읽을 수도 있음)
    }
    
    void TearDown() override {
        // ✅ 테스트 후 BLE 상태 원래대로 복구
        adapter.setPowered(initialPowered);
        adapter.setLEEnabled(initialLEEnabled);
        adapter.setAdvertisingEnabled(initialAdvertisingEnabled);
        adapter.setAdapterName(initialAdapterName);
    
        sleep(1);  // 변경 사항이 반영될 시간을 확보
    }
};

// ✅ 1. `initialize()` 테스트 (BLE 인터페이스 초기화)
TEST(HciAdapterTest, InitializeTest) {
    HciAdapter adapter;

    EXPECT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패! BLE 인터페이스(hci0)가 활성화되어 있는지 확인하세요.";
}

// ✅ 2. `setPowered()` 테스트 (BLE 전원 ON/OFF)
TEST(HciAdapterTest, SetPoweredTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    EXPECT_TRUE(adapter.setPowered(true)) << "BLE 어댑터 전원 켜기 실패!";
    sleep(1); // 변경 반영 대기
    EXPECT_TRUE(adapter.setPowered(false)) << "BLE 어댑터 전원 끄기 실패!";
}

// ✅ 3. `setLEEnabled()` 테스트 (BLE LE 모드 ON/OFF)
TEST(HciAdapterTest, SetLEEnabledTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    EXPECT_TRUE(adapter.setLEEnabled(true)) << "BLE LE 모드 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(adapter.setLEEnabled(false)) << "BLE LE 모드 비활성화 실패!";
}

// ✅ 4. `setAdvertisingEnabled()` 테스트 (BLE Advertising ON/OFF)
TEST(HciAdapterTest, SetAdvertisingTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    EXPECT_TRUE(adapter.setAdvertisingEnabled(true)) << "BLE Advertising 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(adapter.setAdvertisingEnabled(false)) << "BLE Advertising 비활성화 실패!";
}

// ✅ 5. `setAdapterName()` 테스트 (BLE 장치 이름 변경)
TEST(HciAdapterTest, SetAdapterNameTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    std::string testName = "JetsonBLE";
    EXPECT_TRUE(adapter.setAdapterName(testName)) << "BLE 장치 이름 변경 실패!";
}
