#include "Server.h"
#include "DBusObject.h"
#include "GattApplication.h"
#include "Logger.h"

namespace ggk {

std::shared_ptr<Server> TheServer;

Server::Server(const std::string& serviceName,
              const std::string& advertisingName,
              const std::string& advertisingShortName,
              GGKServerDataGetter getter,
              GGKServerDataSetter setter)
    : serviceName(serviceName)
    , advertisingName(advertisingName)
    , advertisingShortName(advertisingShortName)
    , dataGetter(getter)
    , dataSetter(setter)
    , dbusConnection(nullptr)
    , ownedNameId(0)
    , registeredObjectId(0)
    , enableBREDR(false)      // BLE only
    , enableSecureConnection(true)
    , enableConnectable(true)
    , enableDiscoverable(true)
    , enableAdvertising(true)
    , enableBondable(true)
    , gattApp(std::make_unique<GattApplication>()) {

    // Root 객체 생성
    objects.emplace_back(DBusObjectPath("/"));
    Logger::debug("Server created with name: " + serviceName);
}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    if (!setupDBus()) {
        Logger::error("Failed to setup D-Bus");
        return false;
    }

    if (!registerGattApplication()) {
        Logger::error("Failed to register GATT application");
        return false;
    }

    Logger::info("Server initialized successfully");
    return true;
}

void Server::start() {
    if (!dbusConnection) {
        Logger::error("Cannot start server: D-Bus not initialized");
        return;
    }

    // 소유한 이름 요청
    ownedNameId = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                                getOwnedName().c_str(),
                                G_BUS_NAME_OWNER_FLAGS_NONE,
                                onBusAcquired,
                                onNameAcquired,
                                onNameLost,
                                this,
                                nullptr);

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

    return gattApp->registerWithManager(dbusConnection,
                                      (getServiceName() + "/gatt").c_str());
}

void Server::unregisterGattApplication() {
    if (gattApp && dbusConnection) {
        gattApp->unregisterFromManager(dbusConnection);
    }
}

std::shared_ptr<const DBusInterface> Server::findInterface(
    const DBusObjectPath& objectPath,
    const std::string& interfaceName) const {
    
    // GATT 애플리케이션 인터페이스 확인
    if (gattApp) {
        auto interface = gattApp->findInterface(objectPath, interfaceName);
        if (interface) {
            return interface;
        }
    }

    // 일반 객체 인터페이스 확인
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

    // GATT 애플리케이션 메서드 확인
    if (gattApp && gattApp->handleMethod(objectPath, interfaceName,
                                        methodName, pConnection,
                                        pParameters, pInvocation,
                                        pUserData)) {
        return true;
    }

    // 일반 객체 메서드 확인
    for (const auto& object : objects) {
        if (object.callMethod(objectPath, interfaceName,
                            methodName, pConnection,
                            pParameters, pInvocation,
                            pUserData)) {
            return true;
        }
    }

    return false;
}

const GattProperty* Server::findProperty(
    const DBusObjectPath& objectPath,
    const std::string& interfaceName,
    const std::string& propertyName) const {

    // GATT 애플리케이션 프로퍼티 확인
    if (gattApp) {
        if (auto property = gattApp->findProperty(objectPath,
                                                interfaceName,
                                                propertyName)) {
            return property;
        }
    }

    // TODO: 일반 객체 프로퍼티 확인 구현
    return nullptr;
}

bool Server::setupDBus() {
    GError* error = nullptr;
    dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);

    if (error) {
        Logger::error("Failed to connect to D-Bus: " + std::string(error->message));
        g_error_free(error);
        return false;
    }

    return true;
}

void Server::cleanupDBus() {
    if (ownedNameId > 0) {
        g_bus_unown_name(ownedNameId);
        ownedNameId = 0;
    }

    if (registeredObjectId > 0) {
        g_dbus_connection_unregister_object(dbusConnection, registeredObjectId);
        registeredObjectId = 0;
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
    Logger::debug("D-Bus acquired: " + std::string(name));
}

void Server::onNameAcquired(GDBusConnection* connection,
                           const gchar* name,
                           gpointer userData) {
    auto* server = static_cast<Server*>(userData);
    Logger::info("D-Bus name acquired: " + std::string(name));
}

void Server::onNameLost(GDBusConnection* connection,
                       const gchar* name,
                       gpointer userData) {
    auto* server = static_cast<Server*>(userData);
    Logger::warn("D-Bus name lost: " + std::string(name));
}

} // namespace ggk