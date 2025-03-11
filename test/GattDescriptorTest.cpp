#include <gtest/gtest.h>
#include <glib.h>
#include "Utils.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "GattTypes.h"
#include "Logger.h"
#include "DBusConnection.h"

using namespace ggk;

// GattDescriptor의 GVariant 생성 메서드를 테스트하는 테스트 클래스
class GattDescriptorGvariantTest : public ::testing::Test {
protected:
    void SetUp() override {
        // D-Bus 연결
        ASSERT_TRUE(connection.connect());
        
        // 테스트용 서비스 생성
        service = std::make_unique<GattService>(
            connection,
            DBusObjectPath("/test/service"),
            GattUuid::fromShortUuid(0x180F), // Battery Service
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
        
        // 테스트용 설명자 생성
        descriptor = std::make_unique<GattDescriptor>(
            connection,
            DBusObjectPath("/test/service/char1/desc1"),
            GattUuid::fromShortUuid(0x2902), // CCCD
            *characteristic,
            static_cast<uint8_t>(GattPermission::READ) | static_cast<uint8_t>(GattPermission::WRITE)
        );
        
        // 초기값 설정
        std::vector<uint8_t> initialValue = {0x00, 0x00}; // 알림 비활성화
        descriptor->setValue(initialValue);
    }
    
    void TearDown() override {
        descriptor.reset();
        characteristic.reset();
        service.reset();
        connection.disconnect();
    }
    
    DBusConnection connection;
    std::unique_ptr<GattService> service;
    std::unique_ptr<GattCharacteristic> characteristic;
    std::unique_ptr<GattDescriptor> descriptor;
};

// getUuidProperty 테스트
TEST_F(GattDescriptorGvariantTest, GetUuidProperty) {
    GVariant* result = descriptor->getUuidProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "s");
    
    // UUID 값 검증
    const gchar* uuidValue = g_variant_get_string(result, nullptr);
    EXPECT_STREQ(uuidValue, "0000290200001000800000805f9b34fb");
    
    g_variant_unref(result);
}

// getCharacteristicProperty 테스트
TEST_F(GattDescriptorGvariantTest, GetCharacteristicProperty) {
    GVariant* result = descriptor->getCharacteristicProperty();
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "o");
    
    // 특성 경로 검증
    const gchar* path = g_variant_get_string(result, nullptr);
    EXPECT_STREQ(path, "/test/service/char1");
    
    g_variant_unref(result);
}

// getPermissionsProperty 테스트
TEST_F(GattDescriptorGvariantTest, GetPermissionsProperty) {
    // 여기서 허가 플래그 생성 확인 (READ, WRITE)
    GVariant* result = descriptor->getPermissionsProperty();
    
    // NULL 확인 - 가장 중요한 확인
    ASSERT_NE(result, nullptr) << "getPermissionsProperty returned nullptr";
    
    // 배열 타입 확인
    EXPECT_STREQ(g_variant_get_type_string(result), "as") 
        << "Expected type 'as', got: " << g_variant_get_type_string(result);
    
    // 적어도 "read"와 "write" 플래그가 있어야 함
    EXPECT_GE(g_variant_n_children(result), 2);
    
    bool hasRead = false;
    bool hasWrite = false;
    
    for (gsize i = 0; i < g_variant_n_children(result); i++) {
        GVariant* child = g_variant_get_child_value(result, i);
        const gchar* flag = g_variant_get_string(child, nullptr);
        
        if (strcmp(flag, "read") == 0) hasRead = true;
        if (strcmp(flag, "write") == 0) hasWrite = true;
        
        g_variant_unref(child);
    }
    
    EXPECT_TRUE(hasRead) << "Missing 'read' permission flag";
    EXPECT_TRUE(hasWrite) << "Missing 'write' permission flag";
    
    g_variant_unref(result);
}

// 빈 권한으로 테스트 (빈 배열 테스트)
TEST_F(GattDescriptorGvariantTest, GetPermissionsPropertyWithNoFlags) {
    // 0 권한으로 새 설명자 생성 (빈 권한)
    auto emptyPermDescriptor = std::make_unique<GattDescriptor>(
        connection,
        DBusObjectPath("/test/service/char1/desc2"),
        GattUuid::fromShortUuid(0x2901), // User Description
        *characteristic,
        0 // 권한 없음
    );
    
    // 이 함수가 NULL을 반환하지 않아야 함
    GVariant* result = emptyPermDescriptor->getPermissionsProperty();
    ASSERT_NE(result, nullptr) << "getPermissionsProperty with no permissions returned nullptr";
    
    // 배열 타입 확인
    EXPECT_STREQ(g_variant_get_type_string(result), "as") 
        << "Expected type 'as', got: " << g_variant_get_type_string(result);
    
    // 구현에 따라 비어있을 수도 있고, 기본 권한이 설정될 수도 있음
    // 두 경우 모두 배열이 유효해야 함
    EXPECT_GE(g_variant_n_children(result), 0);
    
    g_variant_unref(result);
}

// setupDBusInterfaces 테스트
TEST_F(GattDescriptorGvariantTest, SetupDBusInterfaces) {
    // 설명자의 인터페이스 설정 및 등록 테스트
    bool result = descriptor->setupDBusInterfaces();
    EXPECT_TRUE(result);
    EXPECT_TRUE(descriptor->isRegistered());
}

// CCCD 값 설정 및 알림 상태 변경 테스트
TEST_F(GattDescriptorGvariantTest, CccdValueSettingAndNotification) {
    // 특성 알림 초기 상태 확인
    EXPECT_FALSE(characteristic->isNotifying());
    
    // CCCD 값을 설정하여 알림 활성화
    std::vector<uint8_t> notifyEnableValue = {0x01, 0x00};
    descriptor->setValue(notifyEnableValue);
    
    // 특성의 알림 상태가 변경되었는지 확인
    EXPECT_TRUE(characteristic->isNotifying());
    
    // 알림 비활성화
    std::vector<uint8_t> notifyDisableValue = {0x00, 0x00};
    descriptor->setValue(notifyDisableValue);
    
    // 특성의 알림 상태가 다시 비활성화 되었는지 확인
    EXPECT_FALSE(characteristic->isNotifying());
}
