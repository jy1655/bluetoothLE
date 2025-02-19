#pragma once

#include <gio/gio.h>
#include <memory>
#include <list>
#include <string>
#include "DBusObject.h"
#include "GattApplication.h"
#include "Logger.h"

namespace ggk {

// 데이터 콜백 타입 정의
using GGKServerDataGetter = std::function<std::vector<uint8_t>()>;
using GGKServerDataSetter = std::function<void(const std::vector<uint8_t>&)>;

class Server {
public:
    // 서버가 관리하는 D-Bus 객체들의 컬렉션
    using Objects = std::list<DBusObject>;

    // 생성자
    Server(const std::string& serviceName,
          const std::string& advertisingName,
          const std::string& advertisingShortName,
          GGKServerDataGetter getter = nullptr,
          GGKServerDataSetter setter = nullptr);
    
    ~Server();

    // 서버 초기화 및 시작/중지
    bool initialize();
    void start();
    void stop();

    // GATT 서비스 관리
    bool registerGattApplication();
    void unregisterGattApplication();
    GattApplication* getGattApplication() { return gattApp.get(); }
    const GattApplication* getGattApplication() const { return gattApp.get(); }

    // Accessors
    const Objects& getObjects() const { return objects; }
    bool getEnableBREDR() const { return enableBREDR; }
    bool getEnableSecureConnection() const { return enableSecureConnection; }
    bool getEnableConnectable() const { return enableConnectable; }
    bool getEnableDiscoverable() const { return enableDiscoverable; }
    bool getEnableAdvertising() const { return enableAdvertising; }
    bool getEnableBondable() const { return enableBondable; }
    GGKServerDataGetter getDataGetter() const { return dataGetter; }
    GGKServerDataSetter getDataSetter() const { return dataSetter; }
    const std::string& getAdvertisingName() const { return advertisingName; }
    const std::string& getAdvertisingShortName() const { return advertisingShortName; }
    const std::string& getServiceName() const { return serviceName; }
    
    // D-Bus 관련
    std::string getOwnedName() const { return "com." + serviceName; }
    
    // D-Bus 객체 및 인터페이스 찾기
    std::shared_ptr<const DBusInterface> findInterface(
        const DBusObjectPath& objectPath,
        const std::string& interfaceName) const;

    bool callMethod(
        const DBusObjectPath& objectPath,
        const std::string& interfaceName,
        const std::string& methodName,
        GDBusConnection* pConnection,
        GVariant* pParameters,
        GDBusMethodInvocation* pInvocation,
        gpointer pUserData) const;

    const GattProperty* findProperty(
        const DBusObjectPath& objectPath,
        const std::string& interfaceName,
        const std::string& propertyName) const;

private:
    // D-Bus 객체 관리
    Objects objects;

    // GATT 애플리케이션
    std::unique_ptr<GattApplication> gattApp;

    // D-Bus 연결
    GDBusConnection* dbusConnection;
    guint ownedNameId;
    guint registeredObjectId;

    // 블루투스 상태 플래그
    bool enableBREDR;
    bool enableSecureConnection;
    bool enableConnectable;
    bool enableDiscoverable;
    bool enableAdvertising;
    bool enableBondable;

    // 데이터 콜백
    GGKServerDataGetter dataGetter;
    GGKServerDataSetter dataSetter;

    // 서버 식별자
    std::string advertisingName;
    std::string advertisingShortName;
    std::string serviceName;

    // 내부 헬퍼 메서드
    bool setupDBus();
    void cleanupDBus();
    static void onBusAcquired(GDBusConnection* connection,
                             const gchar* name,
                             gpointer userData);
    static void onNameAcquired(GDBusConnection* connection,
                              const gchar* name,
                              gpointer userData);
    static void onNameLost(GDBusConnection* connection,
                          const gchar* name,
                          gpointer userData);

    // BlueZ 어댑터 관리를 위한 추가
    std::string adapterPath{"/org/bluez/hci0"};  // 기본 어댑터 경로
    bool setAdapterProperty(const char* property, GVariant* value);
    bool getAdapterProperty(const char* property, GVariant** value);
};

// 전역 서버 인스턴스
extern std::shared_ptr<Server> TheServer;

} // namespace ggk