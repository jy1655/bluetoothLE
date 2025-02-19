#pragma once

#include <gio/gio.h>
#include <map>
#include <memory>
#include "DBusInterface.h"
#include "GattService.h"

namespace ggk {

class GattApplication : public DBusInterface {
public:
    static const char* INTERFACE_NAME;          // "org.freedesktop.DBus.ObjectManager"
    static const char* BLUEZ_GATT_MANAGER_IFACE;// "org.bluez.GattManager1"
    static const char* BLUEZ_SERVICE_NAME;      // "org.bluez"
    static const char* BLUEZ_OBJECT_PATH;       // "/org/bluez/hci0"

    GattApplication();
    virtual ~GattApplication();

    // DBusInterface 구현
    virtual DBusObjectPath getPath() const override { return objectPath; }

    // GATT 서비스 관리
    bool addService(std::shared_ptr<GattService> service);
    std::shared_ptr<GattService> getService(const GattUuid& uuid);
    void removeService(const GattUuid& uuid);

    // GattManager1에 애플리케이션 등록
    bool registerApplication(GDBusConnection* connection);
    void unregisterApplication();

    // ObjectManager 인터페이스 메서드
    GVariant* getManagedObjects();

protected:
    virtual void onApplicationRegistered();
    virtual void onApplicationUnregistered();
    virtual void onServiceAdded(const std::shared_ptr<GattService>& service);
    virtual void onServiceRemoved(const std::shared_ptr<GattService>& service);

private:
    DBusObjectPath objectPath;
    GDBusConnection* dbusConnection;
    std::map<std::string, std::shared_ptr<GattService>> services;

    // DBus 메서드 핸들러
    static void onRegisterApplicationReply(GObject* source,
                                         GAsyncResult* res,
                                         gpointer user_data);
    
    static void onUnregisterApplicationReply(GObject* source,
                                           GAsyncResult* res,
                                           gpointer user_data);

    // 내부 유틸리티
    void buildManagedObjects(GVariantBuilder* builder);
    bool isRegistered() const { return dbusConnection != nullptr; }
};

} // namespace ggk