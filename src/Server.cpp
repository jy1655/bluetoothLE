#include "Server.h"

namespace ggk {

// 전역 서버 인스턴스 초기화
std::shared_ptr<Server> TheServer;

// BlueZ D-Bus 인터페이스 상수
static const char* BLUEZ_SERVICE = "org.bluez";
static const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
static const char* LE_ADVERTISING_MANAGER_IFACE = "org.bluez.LEAdvertisingManager1";
static const char* GATT_MANAGER_IFACE = "org.bluez.GattManager1";

Server::Server(const std::string& serviceName,
              const std::string& advertisingName,
              const std::string& advertisingShortName,
              GGKServerDataGetter getter,
              GGKServerDataSetter setter)
    : dbusConnection(nullptr)
    , ownedNameId(0)
    , registeredObjectId(0)
    , enableBREDR(false)
    , enableSecureConnection(true)
    , enableConnectable(true)
    , enableDiscoverable(true)
    , enableAdvertising(true)
    , enableBondable(true)
    , dataGetter(getter)
    , dataSetter(setter)
    , advertisingName(advertisingName)
    , advertisingShortName(advertisingShortName)
    , serviceName(serviceName) {
    
    gattApp = std::make_unique<GattApplication>();
}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    if (!setupDBus()) {
        Logger::error("Failed to setup D-Bus");
        return false;
    }

    Logger::info("Server initialized successfully");
    return true;
}

void Server::start() {
    if (!dbusConnection) {
        Logger::error("No D-Bus connection available");
        return;
    }

    // 어댑터 설정
    GVariant* value = g_variant_new_boolean(enableBREDR);
    setAdapterProperty("Powered", value);
    setAdapterProperty("Discoverable", g_variant_new_boolean(enableDiscoverable));
    setAdapterProperty("Pairable", g_variant_new_boolean(enableBondable));

    // GATT 애플리케이션 등록
    if (enableAdvertising) {
        registerGattApplication();
    }

    Logger::info("Server started");
}

void Server::stop() {
    unregisterGattApplication();
    cleanupDBus();
    Logger::info("Server stopped");
}

bool Server::registerGattApplication() {
    if (!gattApp || !dbusConnection) {
        return false;
    }
    return gattApp->registerApplication(dbusConnection);
}

void Server::unregisterGattApplication() {
    if (gattApp) {
        gattApp->unregisterApplication();
    }
}

bool Server::setupDBus() {
    GError* error = nullptr;
    
    // D-Bus 연결 설정
    ownedNameId = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                                getOwnedName().c_str(),
                                G_BUS_NAME_OWNER_FLAGS_NONE,
                                onBusAcquired,
                                onNameAcquired,
                                onNameLost,
                                this,
                                nullptr);

    if (ownedNameId == 0) {
        Logger::error("Failed to own D-Bus name");
        return false;
    }

    return true;
}

void Server::cleanupDBus() {
    if (registeredObjectId > 0) {
        g_dbus_connection_unregister_object(dbusConnection, registeredObjectId);
        registeredObjectId = 0;
    }

    if (ownedNameId > 0) {
        g_bus_unown_name(ownedNameId);
        ownedNameId = 0;
    }

    if (dbusConnection) {
        g_object_unref(dbusConnection);
        dbusConnection = nullptr;
    }
}

void Server::onBusAcquired(GDBusConnection* connection,
                          const gchar* name,
                          gpointer userData) {
    auto* server = static_cast<Server*>(userData);
    if (!server) return;

    server->dbusConnection = static_cast<GDBusConnection*>(g_object_ref(connection));
    Logger::info("D-Bus connection acquired: " + std::string(name));
}

void Server::onNameAcquired(GDBusConnection* connection,
                           const gchar* name,
                           gpointer userData) {
    Logger::info("D-Bus name acquired: " + std::string(name));
}

void Server::onNameLost(GDBusConnection* connection,
                       const gchar* name,
                       gpointer userData) {
    Logger::warn("D-Bus name lost: " + std::string(name));
}

std::shared_ptr<const DBusInterface> Server::findInterface(
    const DBusObjectPath& objectPath,
    const std::string& interfaceName) const {
    for (const auto& object : objects) {
        auto interface = object.findInterface(objectPath, interfaceName);
        if (interface) {
            return interface;
        }
    }
    return nullptr;
}

bool Server::callMethod(
    const DBusObjectPath& objectPath,
    const std::string& interfaceName,
    const std::string& methodName,
    GDBusConnection* pConnection,
    GVariant* pParameters,
    GDBusMethodInvocation* pInvocation,
    gpointer pUserData) const {
    
    for (const auto& object : objects) {
        if (object.callMethod(objectPath, interfaceName, methodName,
                            pConnection, pParameters, pInvocation, pUserData)) {
            return true;
        }
    }
    return false;
}

const GattProperty* Server::findProperty(
    const DBusObjectPath& objectPath,
    const std::string& interfaceName,
    const std::string& propertyName) const {
    
    auto interface = findInterface(objectPath, interfaceName);
    if (!interface) {
        return nullptr;
    }

    // TODO: Implement property lookup in DBusInterface
    return nullptr;
}

bool Server::setAdapterProperty(const char* property, GVariant* value) {
    if (!dbusConnection) return false;

    GError* error = nullptr;
    g_dbus_connection_call_sync(
        dbusConnection,
        BLUEZ_SERVICE,
        adapterPath.c_str(),
        "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", ADAPTER_INTERFACE, property, value),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        Logger::error("Failed to set adapter property: " + std::string(error->message));
        g_error_free(error);
        return false;
    }

    return true;
}

bool Server::getAdapterProperty(const char* property, GVariant** value) {
    if (!dbusConnection || !value) return false;

    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        dbusConnection,
        BLUEZ_SERVICE,
        adapterPath.c_str(),
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", ADAPTER_INTERFACE, property),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        Logger::error("Failed to get adapter property: " + std::string(error->message));
        g_error_free(error);
        return false;
    }

    GVariant* variant = nullptr;
    g_variant_get(result, "(v)", &variant);
    *value = variant;
    g_variant_unref(result);

    return true;
}

} // namespace ggk