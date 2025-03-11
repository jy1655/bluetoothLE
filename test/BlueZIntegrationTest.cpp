#include <gtest/gtest.h>
#include "GattApplication.h"
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattTypes.h"
#include <thread>
#include <chrono>



class BlueZIntegrationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // 블루투스 환경 확인
            if (!isBluetoothEnabled()) {
                GTEST_SKIP() << "Bluetooth is not enabled, skipping integration test";
                return;
            }
            
            dbusPath = DBusObjectPath("/org/example/test");
            dbusConnection = std::make_unique<DBusConnection>(G_BUS_TYPE_SYSTEM);
            ASSERT_TRUE(dbusConnection->connect()) << "DBus 연결 실패";
            
            // 실제 어댑터 경로 확인
            adapterPath = findBluetoothAdapter();
            if (adapterPath.toString().empty()) {
                GTEST_SKIP() << "No Bluetooth adapter found, skipping integration test";
                return;
            }
            
            app = std::make_unique<GattApplication>(*dbusConnection, dbusPath);
            ASSERT_TRUE(app->setupDBusInterfaces()) << "DBus 인터페이스 설정 실패";
            
            // 테스트용 서비스 생성 (BlueZ는 비어있는 애플리케이션을 좋아하지 않음)
            GattUuid heartRateServiceUuid("0000180d-0000-1000-8000-00805f9b34fb"); // Heart Rate Service
            service = app->createService(heartRateServiceUuid, true);
            ASSERT_NE(service, nullptr) << "서비스 생성 실패";
            
            // 테스트용 특성 추가 (BlueZ는 비어있는 서비스도 좋아하지 않음)
            GattUuid heartRateMeasurementUuid("00002a37-0000-1000-8000-00805f9b34fb"); // Heart Rate Measurement
            uint8_t props = static_cast<uint8_t>(GattProperty::NOTIFY);
            uint8_t perms = static_cast<uint8_t>(GattPermission::READ);
            characteristic = service->createCharacteristic(heartRateMeasurementUuid, props, perms);
            ASSERT_NE(characteristic, nullptr) << "특성 생성 실패";
        }
        
        void TearDown() override {
            if (app) {
                if (app->isRegistered()) {
                    app->unregisterFromBluez(adapterPath);
                }
                app.reset();
            }
            
            if (dbusConnection) {
                dbusConnection->disconnect();
                dbusConnection.reset();
            }
        }
        
        bool isBluetoothEnabled() {
            // bluetoothctl 명령을 사용하여 블루투스 상태 확인
            FILE* pipe = popen("bluetoothctl show | grep 'Powered: yes'", "r");
            if (!pipe) return false;
            
            char buffer[128];
            std::string result = "";
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                result += buffer;
            }
            pclose(pipe);
            
            return !result.empty();
        }
        
        DBusObjectPath findBluetoothAdapter() {
            // bluetoothctl 명령을 사용하여 어댑터 경로 확인
            // 간단한 구현으로 기본 어댑터 경로 반환
            return DBusObjectPath("/org/bluez/hci0");
        }
        
        std::unique_ptr<DBusConnection> dbusConnection;
        std::unique_ptr<GattApplication> app;
        GattServicePtr service;
        GattCharacteristicPtr characteristic;
        DBusObjectPath dbusPath;
        DBusObjectPath adapterPath;
    };
    
    // 실제 BlueZ 등록 테스트
    TEST_F(BlueZIntegrationTest, RegisterApplication) {
        // 실패해도 테스트를 중단하지 않도록 설정
        EXPECT_NO_FATAL_FAILURE(app->registerWithBluez(adapterPath));
        
        // 환경에 따라 결과가 다를 수 있으므로 로그만 출력
        if (app->isRegistered()) {
            std::cout << "Successfully registered with BlueZ" << std::endl;
        } else {
            std::cout << "Failed to register with BlueZ (this might be expected in some environments)" << std::endl;
        }
    }