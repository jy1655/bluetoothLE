#pragma once

#include <gio/gio.h>
#include <map>
#include <memory>
#include <string>
#include "DBusObject.h"
#include "DBusInterface.h"
#include "GattUuid.h"

namespace ggk {

class GattService;

class GattApplication : public DBusInterface {
    public:
        static const char* INTERFACE_NAME;
        static const char* BLUEZ_GATT_MANAGER_IFACE;
        static const char* BLUEZ_SERVICE_NAME;
        static const char* BLUEZ_OBJECT_PATH;
    
        GattApplication();
        virtual ~GattApplication();
    
        // DBusInterface 구현
        virtual const std::string& getName() const override { return name; }
        virtual DBusObjectPath getPath() const override { return objectPath; }
        virtual GVariant* getObjectsManaged() override;
    
        // GATT 서비스 관리
        bool addService(std::shared_ptr<GattService> service);
        std::shared_ptr<GattService> getService(const GattUuid& uuid);  // string -> GattUuid
        void removeService(const GattUuid& uuid);  // string -> GattUuid
    
        // GattManager1에 등록
        bool registerApplication(GDBusConnection* connection);  // path 파라미터 제거
    
        void unregisterApplication();

        // 상태 확인
        bool isRegistered() const { return dbusConnection != nullptr; }
        GVariant* getManagedObjects();
    
    protected:
        virtual void onApplicationRegistered();
        virtual void onApplicationUnregistered();
        virtual void onServiceAdded(const std::shared_ptr<GattService>& service);
        virtual void onServiceRemoved(const std::shared_ptr<GattService>& service);
    
    private:
        DBusObjectPath objectPath;  // 추가
        GDBusConnection* dbusConnection;
        std::map<std::string, std::shared_ptr<GattService>> services;
    
        static void onRegisterApplicationReply(GObject* source,
                                             GAsyncResult* res,
                                             gpointer user_data);
        
        static void onUnregisterApplicationReply(GObject* source,
                                               GAsyncResult* res,
                                               gpointer user_data);
    
        void buildManagedObjects(GVariantBuilder* builder);
    };
}