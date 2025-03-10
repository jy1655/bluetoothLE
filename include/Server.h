// BleServer.h
#pragma once

#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattApplication.h"
#include "HciAdapter.h"
#include "Mgmt.h"
#include <memory>
#include <atomic>
#include <string>
#include <map>

namespace ggk {

/**
 * BleServer - Bluetooth LE 서버 기능을 관리하는 클래스
 * 
 * 이 클래스는 어댑터 설정, 광고, 연결 관리 등 서버 관련 기능을 담당합니다.
 * GattApplication과 함께 작동하여 완전한 BLE 주변 장치를 구현합니다.
 */
class BleServer {
public:
    // 생성자
    explicit BleServer(DBusConnection& connection);
    
    // 소멸자
    ~BleServer();
    
    // 초기화 및 정리
    bool initialize();
    void shutdown();
    
    // 어댑터 관리
    bool setAdapterName(const std::string& name);
    bool setPowered(bool powered);
    
    // GATT 애플리케이션 등록
    bool registerApplication(GattApplication& application);
    bool unregisterApplication(GattApplication& application);
    
    // 광고 제어
    struct AdvertisingData {
        std::string localName;
        std::vector<GattUuid> serviceUuids;
        std::map<uint16_t, std::vector<uint8_t>> serviceData;
        std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
        std::vector<uint8_t> rawData;
    };
    
    bool startAdvertising(const AdvertisingData& advData);
    bool stopAdvertising();
    bool isAdvertising() const { return advertising; }
    
    // 연결 관리 (옵션)
    bool isConnected() const { return connected; }
    bool disconnectClient();
    
    // 어댑터 접근
    HciAdapter* getAdapter() { return hciAdapter.get(); }
    const std::string& getAdapterPath() const { return adapterPath; }
    
private:
    // D-Bus 관련
    DBusConnection& connection;
    std::string adapterPath;
    GDBusProxyPtr adapterProxy;
    GDBusProxyPtr advertisingManagerProxy;
    
    // HCI 관련
    std::unique_ptr<HciAdapter> hciAdapter;
    std::unique_ptr<Mgmt> mgmt;
    
    // 상태 플래그
    std::atomic<bool> advertising;
    std::atomic<bool> connected;
    DBusObjectPath advertisingPath;
    
    // 상수
    static constexpr const char* ADVERTISING_MANAGER_INTERFACE = "org.bluez.LEAdvertisingManager1";
    static constexpr const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
    static constexpr const char* BLUEZ_SERVICE = "org.bluez";
    static constexpr const char* BLUEZ_OBJECT_PATH = "/org/bluez";
    
    // 헬퍼 메서드
    bool setupBluetooth();
    bool registerAdvertisement(const AdvertisingData& advData);
    bool unregisterAdvertisement();
    
    // 시그널 핸들러
    void handleDeviceConnected(const DBusObjectPath& devicePath);
    void handleDeviceDisconnected(const DBusObjectPath& devicePath);
};

} // namespace ggk