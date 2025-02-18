#pragma once

#include <gio/gio.h>
#include <map>
#include <memory>
#include <string>
#include "DBusObject.h"
#include "DBusInterface.h"

namespace ggk {

class GattService;

class GattApplication {
public:
    static const char* BLUEZ_GATT_MANAGER_IFACE;    // "org.bluez.GattManager1"
    static const char* BLUEZ_SERVICE_NAME;          // "org.bluez"
    static const char* BLUEZ_OBJECT_PATH;           // "/org/bluez/hci0"

    GattApplication();
    virtual ~GattApplication();

    // GATT 서비스 관리
    bool addService(std::shared_ptr<GattService> service);
    std::shared_ptr<GattService> getService(const std::string& uuid);
    void removeService(const std::string& uuid);

    // GattManager1에 등록
    bool registerApplication(GDBusConnection* connection, const char* path);
    void unregisterApplication();

    // D-Bus 객체 접근
    DBusObject& getRootObject() { return rootObject; }
    const DBusObject& getRootObject() const { return rootObject; }

protected:
    // D-Bus 객체 트리 생성
    virtual GVariant* getObjectsManaged();
    
    // 이벤트 핸들러
    virtual void onApplicationRegistered();
    virtual void onApplicationUnregistered();
    virtual void onServiceAdded(const std::shared_ptr<GattService>& service);
    virtual void onServiceRemoved(const std::shared_ptr<GattService>& service);

private:
    // D-Bus 연결 관리
    GDBusConnection* dbusConnection;
    std::string applicationPath;
    
    // 객체 트리 관리
    DBusObject rootObject;
    std::map<std::string, std::shared_ptr<GattService>> services;

    // GattManager1 인터페이스와의 통신
    static void onRegisterApplicationReply(GObject* source,
                                         GAsyncResult* res,
                                         gpointer user_data);
                                         
    static void onUnregisterApplicationReply(GObject* source,
                                           GAsyncResult* res,
                                           gpointer user_data);

    // 내부 유틸리티 메서드
    bool isRegistered() const;
    void cleanup();
};

} // namespace ggk