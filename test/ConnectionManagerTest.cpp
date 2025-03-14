// test/ConnectionManagerTest.cpp
#include <gtest/gtest.h>
#include "ConnectionManager.h"
#include "DBusTestEnvironment.h"
#include <chrono>
#include <thread>

using namespace ggk;

class ConnectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 콜백 호출 여부를 추적하기 위한 변수 초기화
        connectionCallbackCalled = false;
        disconnectionCallbackCalled = false;
        propertyChangedCallbackCalled = false;
        
        // 테스트용 디바이스 정보
        testDeviceAddress = "AA:BB:CC:DD:EE:FF";
        testDevicePath = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";
    }

    void TearDown() override {
        // ConnectionManager 종료
        ConnectionManager::getInstance().shutdown();
    }

    // 모의 D-Bus 시그널 생성 (Interfaces Added)
    void sendMockInterfacesAddedSignal(bool connected) {
        // a{sa{sv}} 타입의 인터페이스 맵 생성
        GVariantBuilder ifacesBuilder;
        g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
        
        // Device 인터페이스 속성 맵 생성
        GVariantBuilder propsBuilder;
        g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
        
        // Connected 속성 추가
        g_variant_builder_add(&propsBuilder, "{sv}", "Connected", 
                             g_variant_new_boolean(connected));
        
        // Address 속성 추가
        g_variant_builder_add(&propsBuilder, "{sv}", "Address", 
                             g_variant_new_string(testDeviceAddress.c_str()));
        
        // Device 인터페이스 추가
        g_variant_builder_add(&ifacesBuilder, "{sa{sv}}", 
                             BlueZConstants::DEVICE_INTERFACE.c_str(),
                             &propsBuilder);
        
        // InterfacesAdded 신호 매개변수 생성
        GVariant* params = g_variant_new("(oa{sa{sv}})", 
                                       testDevicePath.c_str(),
                                       &ifacesBuilder);
        
        GVariantPtr paramsPtr(g_variant_ref_sink(params), &g_variant_unref);
        
        // 신호 전송
        DBusConnection& conn = DBusTestEnvironment::getConnection();
        conn.emitSignal(
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            "InterfacesAdded",
            std::move(paramsPtr)
        );
    }
    
    // 모의 D-Bus 시그널 생성 (Interfaces Removed)
    void sendMockInterfacesRemovedSignal() {
        // as 타입의 인터페이스 배열 생성
        GVariantBuilder ifacesBuilder;
        g_variant_builder_init(&ifacesBuilder, G_VARIANT_TYPE("as"));
        
        // Device 인터페이스 추가
        g_variant_builder_add(&ifacesBuilder, "s", BlueZConstants::DEVICE_INTERFACE.c_str());
        
        // InterfacesRemoved 신호 매개변수 생성
        GVariant* params = g_variant_new("(oas)", 
                                       testDevicePath.c_str(),
                                       &ifacesBuilder);
        
        GVariantPtr paramsPtr(g_variant_ref_sink(params), &g_variant_unref);
        
        // 신호 전송
        DBusConnection& conn = DBusTestEnvironment::getConnection();
        conn.emitSignal(
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            "InterfacesRemoved",
            std::move(paramsPtr)
        );
    }
    
    // 모의 D-Bus 시그널 생성 (Properties Changed)
    void sendMockPropertiesChangedSignal() {
        // a{sv} 타입의 속성 맵 생성
        GVariantBuilder propsBuilder;
        g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
        
        // RSSI 속성 추가
        g_variant_builder_add(&propsBuilder, "{sv}", "RSSI", 
                             g_variant_new_int16(-65));
        
        // as 타입의 무효화된 속성 배열 생성
        GVariantBuilder invalidatedBuilder;
        g_variant_builder_init(&invalidatedBuilder, G_VARIANT_TYPE("as"));
        
        // PropertiesChanged 신호 매개변수 생성
        GVariant* params = g_variant_new("(sa{sv}as)", 
                                       BlueZConstants::DEVICE_INTERFACE.c_str(),
                                       &propsBuilder,
                                       &invalidatedBuilder);
        
        GVariantPtr paramsPtr(g_variant_ref_sink(params), &g_variant_unref);
        
        // 신호 전송
        DBusConnection& conn = DBusTestEnvironment::getConnection();
        conn.emitSignal(
            DBusObjectPath(testDevicePath),
            BlueZConstants::PROPERTIES_INTERFACE,
            "PropertiesChanged",
            std::move(paramsPtr)
        );
    }

    // 테스트 상태 변수
    bool connectionCallbackCalled;
    bool disconnectionCallbackCalled;
    bool propertyChangedCallbackCalled;
    std::string testDeviceAddress;
    std::string testDevicePath;
};

// 초기화 테스트
TEST_F(ConnectionManagerTest, Initialize) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    bool result = connMgr.initialize(connection);
    EXPECT_TRUE(result);
    EXPECT_TRUE(connMgr.getConnectedDevices().empty());
}

// 연결 이벤트 테스트
TEST_F(ConnectionManagerTest, DeviceConnectedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    // 초기화
    ASSERT_TRUE(connMgr.initialize(connection));
    
    // 연결 콜백 설정
    connMgr.setOnConnectionCallback([this](const std::string& deviceAddress) {
        EXPECT_EQ(deviceAddress, testDeviceAddress);
        connectionCallbackCalled = true;
    });
    
    // 연결 상태가 true인 모의 InterfacesAdded 시그널 전송
    sendMockInterfacesAddedSignal(true);
    
    // 콜백 호출을 위한 잠시 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 콜백이 호출되었는지 확인
    EXPECT_TRUE(connectionCallbackCalled);
    
    // 디바이스가 연결된 목록에 추가되었는지 확인
    auto devices = connMgr.getConnectedDevices();
    EXPECT_EQ(devices.size(), 1);
    if (!devices.empty()) {
        EXPECT_EQ(devices[0], testDeviceAddress);
    }
    
    EXPECT_TRUE(connMgr.isDeviceConnected(testDeviceAddress));
}

// 연결 해제 이벤트 테스트
TEST_F(ConnectionManagerTest, DeviceDisconnectedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    // 초기화
    ASSERT_TRUE(connMgr.initialize(connection));
    
    // 먼저 연결 상태로 설정
    connMgr.setOnConnectionCallback([this](const std::string& deviceAddress) {
        connectionCallbackCalled = true;
    });
    
    sendMockInterfacesAddedSignal(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectionCallbackCalled);
    ASSERT_TRUE(connMgr.isDeviceConnected(testDeviceAddress));
    
    // 연결 해제 콜백 설정
    connMgr.setOnDisconnectionCallback([this](const std::string& deviceAddress) {
        EXPECT_EQ(deviceAddress, testDeviceAddress);
        disconnectionCallbackCalled = true;
    });
    
    // 모의 InterfacesRemoved 시그널 전송
    sendMockInterfacesRemovedSignal();
    
    // 콜백 호출을 위한 잠시 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 콜백이 호출되었는지 확인
    EXPECT_TRUE(disconnectionCallbackCalled);
    
    // 디바이스가 연결된 목록에서 제거되었는지 확인
    EXPECT_TRUE(connMgr.getConnectedDevices().empty());
    EXPECT_FALSE(connMgr.isDeviceConnected(testDeviceAddress));
}

// 속성 변경 이벤트 테스트
TEST_F(ConnectionManagerTest, PropertyChangedEvent) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    // 초기화
    ASSERT_TRUE(connMgr.initialize(connection));
    
    // 먼저 연결 상태로 설정
    connMgr.setOnConnectionCallback([this](const std::string& deviceAddress) {
        connectionCallbackCalled = true;
    });
    
    sendMockInterfacesAddedSignal(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectionCallbackCalled);
    
    // 속성 변경 콜백 설정
    connMgr.setOnPropertyChangedCallback([this](const std::string& interface, 
                                               const std::string& property, 
                                               GVariantPtr value) {
        EXPECT_EQ(interface, BlueZConstants::DEVICE_INTERFACE);
        EXPECT_EQ(property, "RSSI");
        EXPECT_NE(value.get(), nullptr);
        
        // RSSI 값 확인
        if (property == "RSSI" && value.get()) {
            int16_t rssi = g_variant_get_int16(value.get());
            EXPECT_EQ(rssi, -65);
        }
        
        propertyChangedCallbackCalled = true;
    });
    
    // 모의 PropertiesChanged 시그널 전송
    sendMockPropertiesChangedSignal();
    
    // 콜백 호출을 위한 잠시 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 콜백이 호출되었는지 확인
    EXPECT_TRUE(propertyChangedCallbackCalled);
}

// 연결 및 연결 해제 통합 테스트
TEST_F(ConnectionManagerTest, DeviceConnectionLifecycle) {
    ConnectionManager& connMgr = ConnectionManager::getInstance();
    DBusConnection& connection = DBusTestEnvironment::getConnection();
    
    // 초기화
    ASSERT_TRUE(connMgr.initialize(connection));
    
    // 연결 콜백 설정
    connMgr.setOnConnectionCallback([this](const std::string& deviceAddress) {
        connectionCallbackCalled = true;
    });
    
    // 연결 해제 콜백 설정
    connMgr.setOnDisconnectionCallback([this](const std::string& deviceAddress) {
        disconnectionCallbackCalled = true;
    });
    
    // 1. 연결 이벤트 테스트
    sendMockInterfacesAddedSignal(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(connectionCallbackCalled);
    EXPECT_TRUE(connMgr.isDeviceConnected(testDeviceAddress));
    
    // 2. 속성 변경 이벤트 테스트
    connMgr.setOnPropertyChangedCallback([this](const std::string& interface, 
                                               const std::string& property, 
                                               GVariantPtr value) {
        propertyChangedCallbackCalled = true;
    });
    
    sendMockPropertiesChangedSignal();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(propertyChangedCallbackCalled);
    
    // 3. 연결 해제 이벤트 테스트
    sendMockInterfacesRemovedSignal();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(disconnectionCallbackCalled);
    EXPECT_FALSE(connMgr.isDeviceConnected(testDeviceAddress));
}