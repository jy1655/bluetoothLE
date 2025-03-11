#include <gtest/gtest.h>
#include <glib.h>
#include "Utils.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattTypes.h"
#include "Logger.h"
#include "DBusConnection.h"

using namespace ggk;

// GattCharacteristic의 GVariant 생성 메서드를 테스트하는 테스트 클래스
class GattCharacteristicGvariantTest : public ::testing::Test {
protected:
    void SetUp() override {
        // D-Bus 연결
        ASSERT_TRUE(connection.connect());
        
        // 테스트용 서비스 생성
        service = std::make_unique<GattService>(
            connection,
            DBusObjectPath("/test/service"),
            GattUuid::fromShortUuid(0x180F), // Battery Service UUID
            true
        );
        
        // 테스트용 특성 생성
        characteristic = std::make_unique<GattCharacteristic>(
            connection,
            DBusObjectPath("/test/service/char1"),
            GattUuid::fromShortUuid(0x2A19), // Battery Level
            *service,
            static_cast<uint8_t>(GattProperty::READ) | static_cast<uint8_t>(GattProperty::NOTIFY),
            static_cast<uint8_t>(GattPermission::READ)
        );
        
        // 초기값 설정
        std::vector<uint8_t> initialValue = {0x64}; // 100%
        characteristic->setValue(initialValue);
    }
    
    void TearDown() override {
        characteristic.reset();
        service.reset();
        connection.disconnect();
    }
    
    DBusConnection connection;
    std::unique_ptr<GattService> service;
    std::unique_ptr<GattCharacteristic> characteristic;
};

// getUuidProperty 테스트
TEST_F(GattCharacteristicGvariantTest, GetUuidProperty) {
    GVariant* result = characteristic->getUuidProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "s");
    
    // UUID 값 검증
    const gchar* uuidValue = g_variant_get_string(result, nullptr);
    EXPECT_STREQ(uuidValue, "00002a1900001000800000805f9b34fb");
    
    g_variant_unref(result);
}

// getServiceProperty 테스트
TEST_F(GattCharacteristicGvariantTest, GetServiceProperty) {
    GVariant* result = characteristic->getServiceProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "o");
    
    // 서비스 경로 검증
    const gchar* path = g_variant_get_string(result, nullptr);
    EXPECT_STREQ(path, "/test/service");
    
    g_variant_unref(result);
}

// getPropertiesProperty 테스트
TEST_F(GattCharacteristicGvariantTest, GetPropertiesProperty) {
    // 특성은 READ와 NOTIFY 속성을 가지고 있음
    GVariant* result = characteristic->getPropertiesProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 속성 배열 검증
    EXPECT_GE(g_variant_n_children(result), 2); // 최소 "read"와 "notify"가 있어야 함
    
    bool hasRead = false;
    bool hasNotify = false;
    
    for (gsize i = 0; i < g_variant_n_children(result); i++) {
        GVariant* child = g_variant_get_child_value(result, i);
        const gchar* flag = g_variant_get_string(child, nullptr);
        
        if (strcmp(flag, "read") == 0) hasRead = true;
        if (strcmp(flag, "notify") == 0) hasNotify = true;
        
        g_variant_unref(child);
    }
    
    EXPECT_TRUE(hasRead);
    EXPECT_TRUE(hasNotify);
    
    g_variant_unref(result);
}

// getDescriptorsProperty 빈 배열 테스트
TEST_F(GattCharacteristicGvariantTest, GetEmptyDescriptorsProperty) {
    // 설명자가 추가되기 전 (빈 배열 상태)
    GVariant* result = characteristic->getDescriptorsProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 빈 배열 검증
    EXPECT_EQ(g_variant_n_children(result), 0);
    
    g_variant_unref(result);
}

// getDescriptorsProperty 설명자 추가 테스트
TEST_F(GattCharacteristicGvariantTest, GetDescriptorsPropertyWithItems) {
    // 설명자 추가
    GattDescriptorPtr descriptor = characteristic->createDescriptor(
        GattUuid::fromShortUuid(0x2902), // CCCD
        static_cast<uint8_t>(GattPermission::READ) | static_cast<uint8_t>(GattPermission::WRITE)
    );
    
    // Null이 아닌지 확인
    ASSERT_NE(descriptor, nullptr);
    
    // getDescriptorsProperty 테스트
    GVariant* result = characteristic->getDescriptorsProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 배열에 하나의 항목이 있어야 함
    EXPECT_EQ(g_variant_n_children(result), 1);
    
    g_variant_unref(result);
}

// getNotifyingProperty 테스트
TEST_F(GattCharacteristicGvariantTest, GetNotifyingProperty) {
    // 초기값은 false
    GVariant* result = characteristic->getNotifyingProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "b");
    EXPECT_FALSE(g_variant_get_boolean(result));
    g_variant_unref(result);
    
    // 알림 활성화 후 테스트
    characteristic->startNotify();
    result = characteristic->getNotifyingProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "b");
    EXPECT_TRUE(g_variant_get_boolean(result));
    g_variant_unref(result);
}

// setupDBusInterfaces 테스트
TEST_F(GattCharacteristicGvariantTest, SetupDBusInterfaces) {
    // 인터페이스 설정 및 등록 테스트
    bool result = characteristic->setupDBusInterfaces();
    EXPECT_TRUE(result);
    EXPECT_TRUE(characteristic->isRegistered());
}
