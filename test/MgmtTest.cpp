#include <gtest/gtest.h>
#include "Mgmt.h"

using namespace ggk;

// ✅ 1. `setName()` 테스트 (BLE 장치 이름 변경)
TEST(MgmtTest, SetNameTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    std::string testName = "JetsonBLE";
    std::string shortName = "JBLE";

    EXPECT_TRUE(mgmt.setName(testName, shortName)) << "BLE 장치 이름 변경 실패!";
}

// ✅ 2. `setDiscoverable()` 테스트 (BLE 검색 가능 상태 변경)
TEST(MgmtTest, SetDiscoverableTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setDiscoverable(1, 60)) << "BLE 검색 가능 상태 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setDiscoverable(0, 0)) << "BLE 검색 가능 상태 비활성화 실패!";
}

// ✅ 3. `setPowered()` 테스트 (BLE 전원 ON/OFF)
TEST(MgmtTest, SetPoweredTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setPowered(true)) << "BLE 전원 켜기 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setPowered(false)) << "BLE 전원 끄기 실패!";
}

// ✅ 4. `setBredr()` 테스트 (BR/EDR 모드 ON/OFF)
TEST(MgmtTest, SetBredrTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setBredr(true)) << "BLE BR/EDR 모드 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setBredr(false)) << "BLE BR/EDR 모드 비활성화 실패!";
}

// ✅ 5. `setSecureConnections()` 테스트 (Secure Connection 모드 ON/OFF)
TEST(MgmtTest, SetSecureConnectionsTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setSecureConnections(1)) << "Secure Connection 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setSecureConnections(0)) << "Secure Connection 비활성화 실패!";
}

// ✅ 6. `setBondable()` 테스트 (BLE 장치가 Bonding을 지원하는지 확인)
TEST(MgmtTest, SetBondableTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setBondable(true)) << "BLE Bonding 모드 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setBondable(false)) << "BLE Bonding 모드 비활성화 실패!";
}

// ✅ 7. `setConnectable()` 테스트 (BLE 장치를 연결 가능 상태로 변경)
TEST(MgmtTest, SetConnectableTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setConnectable(true)) << "BLE Connectable 모드 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setConnectable(false)) << "BLE Connectable 모드 비활성화 실패!";
}

// ✅ 8. `setLE()` 테스트 (BLE Low Energy 모드 ON/OFF)
TEST(MgmtTest, SetLETest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setLE(true)) << "BLE LE 모드 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setLE(false)) << "BLE LE 모드 비활성화 실패!";
}

// ✅ 9. `setAdvertising()` 테스트 (BLE 광고(Advertising) ON/OFF)
TEST(MgmtTest, SetAdvertisingTest) {
    HciAdapter adapter;
    ASSERT_TRUE(adapter.initialize()) << "HciAdapter 초기화 실패!";

    Mgmt mgmt(adapter);

    EXPECT_TRUE(mgmt.setAdvertising(1)) << "BLE Advertising 활성화 실패!";
    sleep(1);
    EXPECT_TRUE(mgmt.setAdvertising(0)) << "BLE Advertising 비활성화 실패!";
}
