// test/ConnectionManagerTest.cpp
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

// BLE 스택 헤더
#include "DBusName.h"
#include "ConnectionManager.h"
#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattAdvertisement.h"

using namespace ggk;

class ConnectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // DBus 연결 초기화
        ASSERT_TRUE(DBusName::getInstance().initialize());
        dbus_conn = &DBusName::getInstance().getConnection();
        
        // GATT Application 생성하지 않고 바로 광고만 설정
        advertisement = std::make_shared<GattAdvertisement>(
            *dbus_conn,
            DBusObjectPath("/com/example/ble/adv")
        );
        
        advertisement->setLocalName("BLE Test Device");
        ASSERT_TRUE(advertisement->registerWithBlueZ());
        
        // ConnectionManager 초기화
        ASSERT_TRUE(ConnectionManager::getInstance().initialize(*dbus_conn));
    }
    
    void TearDown() override {
        std::cout << "\n====== CLEANING UP BLE PERIPHERAL TEST ======\n" << std::endl;
        
        // 1. 광고 중지
        if (advertisement && advertisement->isRegistered()) {
            std::cout << "Stopping advertisement..." << std::endl;
            advertisement->unregisterFromBlueZ();
        }
        
        // 2. GATT 애플리케이션 등록 해제
        if (application && application->isRegistered()) {
            std::cout << "Unregistering GATT application..." << std::endl;
            application->unregisterFromBlueZ();
        }
        
        // 3. ConnectionManager 종료
        std::cout << "Shutting down ConnectionManager..." << std::endl;
        ConnectionManager::getInstance().shutdown();
        
        // 4. DBusName 종료
        std::cout << "Closing D-Bus connection..." << std::endl;
        DBusName::getInstance().shutdown();
        
        std::cout << "Test cleanup complete." << std::endl;
    }
    
    // Wait for connection with timeout
    bool waitForConnection(int timeoutSeconds) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::seconds(timeoutSeconds), 
                         [this] { return connectionCallbackCalled; });
    }
    
    // Wait for disconnection with timeout
    bool waitForDisconnection(int timeoutSeconds) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::seconds(timeoutSeconds), 
                         [this] { return disconnectionCallbackCalled; });
    }
    
    // Wait for property change with timeout
    bool waitForPropertyChange(int timeoutSeconds) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::seconds(timeoutSeconds), 
                         [this] { return propertyChangedCallbackCalled; });
    }
    
    // Test state variables
    std::mutex mtx;
    std::condition_variable cv;
    bool connectionCallbackCalled;
    bool disconnectionCallbackCalled;
    bool propertyChangedCallbackCalled;
    std::string deviceAddress;
    std::string propertyName;
    
    // BLE stack components
    DBusConnection* dbus_conn;
    std::shared_ptr<GattApplication> application;
    std::shared_ptr<GattAdvertisement> advertisement;
};

// 1. 기본 초기화 테스트
TEST_F(ConnectionManagerTest, Initialize) {
    // 이미 SetUp에서 초기화가 완료됨
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    
    // 연결된 장치 목록 확인
    std::vector<std::string> devices = connMgr.getConnectedDevices();
    std::cout << "Found " << devices.size() << " initially connected devices" << std::endl;
}

// 2. 장치 연결 이벤트 테스트
TEST_F(ConnectionManagerTest, DeviceConnectedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    
    std::cout << "\n===== DEVICE CONNECTION TEST =====\n" << std::endl;
    std::cout << "Waiting for a device to connect..." << std::endl;
    std::cout << "Please connect to 'BLE Test Device' from any BLE Central device (smartphone, etc.)" << std::endl;
    std::cout << "You have 30 seconds to complete the connection" << std::endl;
    
    // 연결 콜백 설정
    connMgr.setOnConnectionCallback([this](const std::string& address) {
        std::cout << "✓ Connection detected from device: " << address << std::endl;
        
        std::lock_guard<std::mutex> lock(mtx);
        connectionCallbackCalled = true;
        deviceAddress = address;
        cv.notify_one();
    });
    
    // 연결 대기
    bool connected = waitForConnection(30);
    if (!connected) {
        std::cout << "No device connected within 30 seconds timeout" << std::endl;
        GTEST_SKIP() << "No device connected during test period";
        return;
    }
    
    // 연결 확인
    EXPECT_TRUE(connectionCallbackCalled);
    EXPECT_FALSE(deviceAddress.empty());
    EXPECT_TRUE(connMgr.isDeviceConnected(deviceAddress));
    
    // 연결된 장치 목록 확인
    auto devices = connMgr.getConnectedDevices();
    EXPECT_FALSE(devices.empty());
    EXPECT_TRUE(std::find(devices.begin(), devices.end(), deviceAddress) != devices.end());
    
    std::cout << "Connection test passed successfully!" << std::endl;
}

// 3. 장치 연결 해제 이벤트 테스트
TEST_F(ConnectionManagerTest, DeviceDisconnectedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    
    std::cout << "\n===== DEVICE DISCONNECTION TEST =====\n" << std::endl;
    
    // 먼저 연결된 장치 확인
    auto devices = connMgr.getConnectedDevices();
    if (devices.empty()) {
        std::cout << "No devices currently connected." << std::endl;
        std::cout << "Please connect to 'BLE Test Device' from a BLE Central device" << std::endl;
        
        // 연결 콜백 설정 및 대기
        connMgr.setOnConnectionCallback([this](const std::string& address) {
            std::cout << "✓ Connection detected from device: " << address << std::endl;
            std::lock_guard<std::mutex> lock(mtx);
            connectionCallbackCalled = true;
            deviceAddress = address;
            cv.notify_one();
        });
        
        bool connected = waitForConnection(30);
        if (!connected) {
            std::cout << "No device connected within timeout" << std::endl;
            GTEST_SKIP() << "No device connected for disconnection test";
            return;
        }
        
        devices = connMgr.getConnectedDevices();
    }
    
    // 이제 연결된 장치가 있어야 함
    ASSERT_FALSE(devices.empty());
    
    std::cout << "Currently connected devices:" << std::endl;
    for (const auto& addr : devices) {
        std::cout << "  - " << addr << std::endl;
    }
    
    std::cout << "Please disconnect the device within 30 seconds" << std::endl;
    std::cout << "(Either turn off Bluetooth on the connected device or use the app to disconnect)" << std::endl;
    
    // 연결 해제 콜백 설정
    connMgr.setOnDisconnectionCallback([this](const std::string& address) {
        std::cout << "✓ Disconnection detected for device: " << address << std::endl;
        std::lock_guard<std::mutex> lock(mtx);
        disconnectionCallbackCalled = true;
        deviceAddress = address;
        cv.notify_one();
    });
    
    // 연결 해제 대기
    bool disconnected = waitForDisconnection(30);
    if (!disconnected) {
        std::cout << "No device disconnected within timeout" << std::endl;
        GTEST_SKIP() << "No device disconnected during test period";
        return;
    }
    
    // 연결 해제 확인
    EXPECT_TRUE(disconnectionCallbackCalled);
    EXPECT_FALSE(deviceAddress.empty());
    EXPECT_FALSE(connMgr.isDeviceConnected(deviceAddress));
    
    std::cout << "Disconnection test passed successfully!" << std::endl;
}

// 4. 속성 변경 테스트
TEST_F(ConnectionManagerTest, PropertyChangedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    
    std::cout << "\n===== PROPERTY CHANGE TEST =====\n" << std::endl;
    
    // 연결된 장치 확인
    auto devices = connMgr.getConnectedDevices();
    if (devices.empty()) {
        std::cout << "No devices currently connected." << std::endl;
        std::cout << "Please connect to 'BLE Test Device' from a BLE Central device" << std::endl;
        
        // 연결 대기
        connMgr.setOnConnectionCallback([this](const std::string& address) {
            deviceAddress = address;
            connectionCallbackCalled = true;
            cv.notify_one();
        });
        
        if (!waitForConnection(30)) {
            GTEST_SKIP() << "No device connected for property change test";
            return;
        }
    }
    
    // 속성 변경 콜백 설정
    connMgr.setOnPropertyChangedCallback([this](const std::string& interface,
                                             const std::string& property,
                                             GVariantPtr value) {
        if (!value) return;
        
        char* str = g_variant_print(value.get(), TRUE);
        std::cout << "Property changed: " << interface << "." << property << " = " << str << std::endl;
        g_free(str);
        
        std::lock_guard<std::mutex> lock(mtx);
        propertyChangedCallbackCalled = true;
        propertyName = property;
        cv.notify_one();
    });
    
    std::cout << "Waiting for property changes..." << std::endl;
    std::cout << "Please interact with the connected device (move it, use the app, etc.)" << std::endl;
    std::cout << "You have 30 seconds to trigger property changes" << std::endl;
    
    // 속성 변경 대기
    bool propertyChanged = waitForPropertyChange(30);
    if (!propertyChanged) {
        std::cout << "No property changes detected within timeout" << std::endl;
        GTEST_SKIP() << "No property changes detected during test period";
        return;
    }
    
    // 속성 변경 확인
    EXPECT_TRUE(propertyChangedCallbackCalled);
    EXPECT_FALSE(propertyName.empty());
    
    std::cout << "Property change test passed successfully!" << std::endl;
}

// 5. 전체 연결 생명주기 테스트
TEST_F(ConnectionManagerTest, DeviceConnectionLifecycle) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    
    std::cout << "\n===== DEVICE LIFECYCLE TEST =====\n" << std::endl;
    std::cout << "This test will verify the complete device connection lifecycle" << std::endl;
    
    // 1. 연결 단계
    std::cout << "\n[PHASE 1] Please connect a device to 'BLE Test Device'" << std::endl;
    
    // 연결 콜백 설정
    std::atomic<bool> connectionDetected(false);
    std::string connectedDevice;
    
    connMgr.setOnConnectionCallback([&](const std::string& address) {
        std::cout << "✓ Connection detected from device: " << address << std::endl;
        connectionDetected = true;
        connectedDevice = address;
        cv.notify_one();
    });
    
    // 연결 대기
    {
        std::unique_lock<std::mutex> lock(mtx);
        auto result = cv.wait_for(lock, std::chrono::seconds(30), 
                                [&]{ return connectionDetected.load(); });
        
        if (!result) {
            std::cout << "No device connected within timeout" << std::endl;
            GTEST_SKIP() << "No device connected during lifecycle test";
            return;
        }
    }
    
    // 연결 확인
    EXPECT_TRUE(connectionDetected);
    EXPECT_FALSE(connectedDevice.empty());
    EXPECT_TRUE(connMgr.isDeviceConnected(connectedDevice));
    
    // 2. 연결 해제 단계
    std::cout << "\n[PHASE 2] Now please disconnect the device: " << connectedDevice << std::endl;
    
    // 연결 해제 콜백 설정
    std::atomic<bool> disconnectionDetected(false);
    
    connMgr.setOnDisconnectionCallback([&](const std::string& address) {
        std::cout << "✓ Disconnection detected for device: " << address << std::endl;
        EXPECT_EQ(address, connectedDevice);
        disconnectionDetected = true;
        cv.notify_one();
    });
    
    // 연결 해제 대기
    {
        std::unique_lock<std::mutex> lock(mtx);
        auto result = cv.wait_for(lock, std::chrono::seconds(30),
                                [&]{ return disconnectionDetected.load(); });
        
        if (!result) {
            std::cout << "Device not disconnected within timeout" << std::endl;
            GTEST_SKIP() << "Device not disconnected during lifecycle test";
            return;
        }
    }
    
    // 연결 해제 확인
    EXPECT_TRUE(disconnectionDetected);
    EXPECT_FALSE(connMgr.isDeviceConnected(connectedDevice));
    
    std::cout << "\nConnection lifecycle test completed successfully!" << std::endl;
}
