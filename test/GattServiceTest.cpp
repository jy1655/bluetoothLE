#include <gtest/gtest.h>
#include <glib.h>
#include "Utils.h"
#include "GattService.h"
#include "GattTypes.h"
#include "Logger.h"
#include "DBusTestEnvironment.h"  // Added this include

using namespace ggk;

// GattService의 GVariant 생성 메서드를 테스트하는 테스트 클래스
class GattServiceGvariantTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 공유 D-Bus 연결 사용
        DBusConnection& connection = DBusTestEnvironment::getConnection();
        
        // 테스트용 서비스 생성
        service = std::make_unique<GattService>(
            connection,
            DBusObjectPath("/test/service"),
            GattUuid::fromShortUuid(0x180F), // Battery Service UUID
            true
        );
    }
    
    void TearDown() override {
        service.reset();
        // 공유 연결은 해제하지 않음 (DBusTestEnvironment가 관리)
    }
    
    std::unique_ptr<GattService> service;
};

// getUuidProperty 테스트
TEST_F(GattServiceGvariantTest, GetUuidProperty) {
    GVariant* result = service->getUuidProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "s");
    
    // UUID 값 검증
    const gchar* uuidValue = g_variant_get_string(result, nullptr);
    EXPECT_STREQ(uuidValue, "0000180f00001000800000805f9b34fb");
    
    g_variant_unref(result);
}

// getPrimaryProperty 테스트
TEST_F(GattServiceGvariantTest, GetPrimaryProperty) {
    GVariant* result = service->getPrimaryProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "b");
    
    // Primary 값 검증 (true로 설정함)
    gboolean isPrimary = g_variant_get_boolean(result);
    EXPECT_TRUE(isPrimary);
    
    g_variant_unref(result);
}

// getCharacteristicsProperty 테스트 (빈 특성 목록)
TEST_F(GattServiceGvariantTest, GetEmptyCharacteristicsProperty) {
    // 특성이 추가되기 전 (빈 배열 상태)
    GVariant* result = service->getCharacteristicsProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 빈 배열 검증
    EXPECT_EQ(g_variant_n_children(result), 0);
    
    g_variant_unref(result);
}

// 특성을 추가한 후 getCharacteristicsProperty 테스트
TEST_F(GattServiceGvariantTest, GetCharacteristicsPropertyWithItems) {
    // 먼저 특성을 추가
    GattCharacteristicPtr characteristic = service->createCharacteristic(
        GattUuid::fromShortUuid(0x2A19), // Battery Level
        GattProperty::PROP_READ,
        GattPermission::PERM_READ
    );
    
    // Null이 아닌지 확인
    ASSERT_NE(characteristic, nullptr);
    
    // getCharacteristicsProperty 테스트
    GVariant* result = service->getCharacteristicsProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 이제 배열에 하나의 항목이 있어야 함
    EXPECT_EQ(g_variant_n_children(result), 1);
    
    g_variant_unref(result);
}

// setupDBusInterfaces 테스트
TEST_F(GattServiceGvariantTest, SetupDBusInterfaces) {
    // 인터페이스 설정 및 등록 테스트
    bool result = service->setupDBusInterfaces();
    EXPECT_TRUE(result);
    EXPECT_TRUE(service->isRegistered());
}