#include <gtest/gtest.h>
#include "DBusConnection.h"
#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"

using namespace ggk;

class GattServiceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(connection.connect()) << "D-Bus 연결 실패. BlueZ가 실행 중인지 확인하세요.";

        application = std::make_shared<GattApplication>(
            connection,
            DBusObjectPath("/org/example/app0")
        );
        ASSERT_TRUE(application->setupDBusInterfaces());

        // GATT Service 생성 (Heart Rate Service)
        service = application->createService(
            GattUuid::fromShortUuid(0x180D), // Heart Rate
            true
        );
        ASSERT_NE(service, nullptr);

        // GATT Characteristic 생성 (Heart Rate Measurement)
        characteristic = service->createCharacteristic(
            GattUuid::fromShortUuid(0x2A37),
            static_cast<uint8_t>(GattProperty::READ) | static_cast<uint8_t>(GattProperty::NOTIFY),
            static_cast<uint8_t>(GattPermission::READ)
        );
        ASSERT_NE(characteristic, nullptr);

        // 값 설정
        characteristic->setValue({0x60});

        // GATT Descriptor 생성 (CCCD)
        descriptor = characteristic->createDescriptor(
            GattUuid::fromShortUuid(0x2902), // CCCD
            static_cast<uint8_t>(GattPermission::READ) | static_cast<uint8_t>(GattPermission::WRITE)
        );
        ASSERT_NE(descriptor, nullptr);
        
        descriptor->setValue({0x00, 0x00}); // 초기 알림 비활성화

        // BlueZ에 애플리케이션 등록 (선택적)
        // ASSERT_TRUE(application->registerWithBluez(DBusObjectPath("/org/bluez/hci0")));
    }

    void TearDown() override {
        // BlueZ에서 애플리케이션 등록 해제 (선택적)
        // if (application) {
        //     application->unregisterFromBluez(DBusObjectPath("/org/bluez/hci0"));
        // }

        connection.disconnect();

        // 다음 테스트를 위한 짧은 대기
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    DBusConnection connection;
    std::shared_ptr<GattApplication> application;
    GattServicePtr service;
    GattCharacteristicPtr characteristic;
    GattDescriptorPtr descriptor;
};

// 서비스 및 특성 등록 확인
TEST_F(GattServiceIntegrationTest, ServiceAndCharacteristicRegistered) {
    EXPECT_TRUE(service->isRegistered());
    EXPECT_TRUE(characteristic->isRegistered());
    EXPECT_TRUE(descriptor->isRegistered());

    Logger::info("Service Path: " + service->getPath().toString());
    Logger::info("Characteristic Path: " + characteristic->getPath().toString());
    Logger::info("Descriptor Path: " + descriptor->getPath().toString());
}

// Characteristic 값 설정 및 읽기 테스트
TEST_F(GattServiceIntegrationTest, SetAndReadCharacteristicValue) {
    std::vector<uint8_t> testValue = {0x48};  // 예시 값 설정
    characteristic->setValue(testValue);
    
    EXPECT_EQ(characteristic->getValue(), testValue);
    
    // 추가: 특성을 읽을 때 콜백이 동작하는지 확인
    bool callbackCalled = false;
    characteristic->setReadCallback([&callbackCalled, &testValue]() {
        callbackCalled = true;
        return testValue;
    });
    
    // 콜백을 통해 특성 읽기 시뮬레이션
    // 실제로 콜백이 호출되려면 BlueZ를 통한 외부 읽기 요청이 필요
    // 이 부분은 주석 처리하고 별도 스크립트로 읽기 요청 실행 후 로그 확인
    // simulateReadRequest(characteristic->getPath());
    
    // 대안으로 gtest 프레임워크 내에서는 직접 메서드만 확인
    EXPECT_EQ(testValue, characteristic->getValue());
}

// Descriptor 값 설정 (알림 활성화) 및 Notification 상태 확인
TEST_F(GattServiceIntegrationTest, DescriptorNotificationEnableTest) {
    // CCCD 값을 설정하여 알림 활성화 (0x01: 알림 활성화)
    descriptor->setValue({0x01, 0x00});

    EXPECT_TRUE(characteristic->isNotifying());

    // 다시 알림 비활성화
    descriptor->setValue({0x00, 0x00});

    EXPECT_FALSE(characteristic->isNotifying());
}

// 특성 읽기 콜백 설정 및 확인
TEST_F(GattServiceIntegrationTest, CharacteristicReadCallbackTest) {
    characteristic->setReadCallback([]() -> std::vector<uint8_t> {
        return {0x55};  // 콜백에서 반환하는 값
    });

    auto value = characteristic->getValue();
    EXPECT_EQ(value, std::vector<uint8_t>({0x60})); // setValue로 설정된 값 유지

    // 실제 BLE 클라이언트가 읽기 요청 시 콜백 값이 반환되는지 BLE 앱으로 추가 확인
}

// Descriptor 쓰기 콜백 설정 및 호출 확인
TEST_F(GattServiceIntegrationTest, DescriptorWriteCallbackTest) {
    bool writeCallbackCalled = false;

    descriptor->setWriteCallback([&writeCallbackCalled](const std::vector<uint8_t>& newValue) -> bool {
        writeCallbackCalled = true;
        EXPECT_EQ(newValue, std::vector<uint8_t>({0x01, 0x00}));
        return true;
    });

    // BLE 클라이언트로부터 CCCD 값을 변경해 실제로 콜백 호출되는지 BLE 앱에서 테스트
    descriptor->setValue({0x01, 0x00});
    EXPECT_TRUE(writeCallbackCalled);
}

TEST_F(GattServiceIntegrationTest, NotificationTest) {
    // 알림 콜백 설정
    bool notifyCalled = false;
    characteristic->setNotifyCallback([&notifyCalled]() {
        notifyCalled = true;
    });
    
    // 알림 활성화
    EXPECT_TRUE(characteristic->startNotify());
    EXPECT_TRUE(characteristic->isNotifying());
    
    // 값 변경으로 알림 트리거
    characteristic->setValue({0x70});
    
    // 콜백이 호출되었는지 확인
    EXPECT_TRUE(notifyCalled);
    
    // 알림 비활성화
    EXPECT_TRUE(characteristic->stopNotify());
    EXPECT_FALSE(characteristic->isNotifying());
}

TEST_F(GattServiceIntegrationTest, AccessPermissionTest) {
    // 읽기 전용 특성 생성
    auto readOnlyChar = service->createCharacteristic(
        GattUuid::fromShortUuid(0x2A38), // Body Sensor Location
        static_cast<uint8_t>(GattProperty::READ),
        static_cast<uint8_t>(GattPermission::READ)
    );
    
    ASSERT_NE(readOnlyChar, nullptr);
    EXPECT_TRUE(readOnlyChar->isRegistered());
    
    // 쓰기 권한이 없는지 확인 - 단 이 테스트는 API만 확인하므로 실제 권한 제한을 테스트하지는 않음
    EXPECT_FALSE(readOnlyChar->getProperties() & static_cast<uint8_t>(GattProperty::WRITE));
}

TEST_F(GattServiceIntegrationTest, ResourceCleanupTest) {
    // 여러 특성과 설명자를 생성하고 해제
    for (int i = 0; i < 10; i++) {
        auto tempChar = service->createCharacteristic(
            GattUuid::fromShortUuid(0x2A3F + i), // 임의의 UUID
            static_cast<uint8_t>(GattProperty::READ),
            static_cast<uint8_t>(GattPermission::READ)
        );
        
        ASSERT_NE(tempChar, nullptr);
        EXPECT_TRUE(tempChar->isRegistered());
        
        // 명시적으로 등록 해제
        tempChar->unregisterObject();
    }
    
    // 서비스 자체는 여전히 유효해야 함
    EXPECT_TRUE(service->isRegistered());
}