#include "GattApplication.h"
#include "GattService.h"
#include "Logger.h"

namespace ggk {

const char* GattApplication::INTERFACE_NAME = "org.freedesktop.DBus.ObjectManager";
const char* GattApplication::BLUEZ_GATT_MANAGER_IFACE = "org.bluez.GattManager1";
const char* GattApplication::BLUEZ_SERVICE_NAME = "org.bluez";
const char* GattApplication::BLUEZ_OBJECT_PATH = "/org/bluez/hci0";

GattApplication::GattApplication()
    : dbusConnection(nullptr)
    , rootObject(DBusObjectPath("/")) {
    Logger::debug("Creating GATT Application");
}

GattApplication::~GattApplication() {
    unregisterApplication();
}

bool GattApplication::addService(std::shared_ptr<GattService> service) {
    if (!service) {
        Logger::error("Attempted to add null GATT service");
        return false;
    }

    const std::string uuid = service->getUUID().toString();
    if (services.find(uuid) != services.end()) {
        Logger::error("Service with UUID " + uuid + " already exists");
        return false;
    }

    services[uuid] = service;
    
    // 서비스를 D-Bus 객체 트리에 추가
    std::string path = "/service" + std::to_string(services.size());
    auto& serviceObject = rootObject.addChild(DBusObjectPath(path));
    serviceObject.addInterface(service);

    // 서비스의 특성들도 DBusObject 트리에 추가
    for (size_t i = 0; i < service->getCharacteristics().size(); i++) {
        auto& characteristic = service->getCharacteristics()[i];
        std::string charPath = path + "/char" + std::to_string(i);
        auto& charObject = serviceObject.addChild(DBusObjectPath(charPath));
        charObject.addInterface(characteristic);
    }

    return true;
}

std::shared_ptr<GattService> GattApplication::getService(const std::string& uuid) {
    auto it = services.find(uuid);
    return (it != services.end()) ? it->second : nullptr;
}

void GattApplication::removeService(const std::string& uuid) {
    auto it = services.find(uuid);
    if (it != services.end()) {
        onServiceRemoved(it->second);
        services.erase(it);
        Logger::info("Removed GATT service: " + uuid);
    }
}

bool GattApplication::registerApplication(GDBusConnection* connection, const char* path) {
    if (!connection) {
        Logger::error("No D-Bus connection provided");
        return false;
    }

    if (isRegistered()) {
        Logger::warn("GATT application already registered");
        return true;
    }

    dbusConnection = connection;
    applicationPath = path;

    GVariant* params = g_variant_new("(oa{sv})", 
                                   path,           // 애플리케이션 경로
                                   nullptr);       // 옵션

    g_dbus_connection_call(dbusConnection,
                          BLUEZ_SERVICE_NAME,
                          BLUEZ_OBJECT_PATH,
                          BLUEZ_GATT_MANAGER_IFACE,
                          "RegisterApplication",
                          params,
                          nullptr,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          nullptr,
                          (GAsyncReadyCallback)onRegisterApplicationReply,
                          this);

    Logger::info("Registering GATT application at path: " + std::string(path));
    return true;
}

void GattApplication::unregisterApplication() {
    if (!isRegistered()) {
        return;
    }

    g_dbus_connection_call(dbusConnection,
                          BLUEZ_SERVICE_NAME,
                          BLUEZ_OBJECT_PATH,
                          BLUEZ_GATT_MANAGER_IFACE,
                          "UnregisterApplication",
                          g_variant_new("(o)", applicationPath.c_str()),
                          nullptr,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          nullptr,
                          (GAsyncReadyCallback)onUnregisterApplicationReply,
                          this);

    Logger::info("Unregistering GATT application");
}

GVariant* GattApplication::getObjectsManaged() {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    
    buildManagedObjects(&builder);
    
    return g_variant_builder_end(&builder);
}

void GattApplication::onRegisterApplicationReply(GObject* source,
                                               GAsyncResult* res,
                                               gpointer user_data) {
    GattApplication* self = static_cast<GattApplication*>(user_data);
    GError* error = nullptr;
    
    GVariant* result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),
                                                    res,
                                                    &error);
    
    if (error) {
        Logger::error("Failed to register application: " + std::string(error->message));
        g_error_free(error);
        return;
    }

    if (result) {
        g_variant_unref(result);
    }

    self->onApplicationRegistered();
    Logger::info("GATT application registered successfully");
}

void GattApplication::onUnregisterApplicationReply(GObject* source,
                                                 GAsyncResult* res,
                                                 gpointer user_data) {
    GattApplication* self = static_cast<GattApplication*>(user_data);
    GError* error = nullptr;
    
    GVariant* result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),
                                                    res,
                                                    &error);
    
    if (error) {
        Logger::error("Failed to unregister application: " + std::string(error->message));
        g_error_free(error);
        return;
    }

    if (result) {
        g_variant_unref(result);
    }

    self->onApplicationUnregistered();
    self->cleanup();
    Logger::info("GATT application unregistered successfully");
}

bool GattApplication::isRegistered() const {
    return dbusConnection != nullptr && !applicationPath.empty();
}

void GattApplication::cleanup() {
    dbusConnection = nullptr;
    applicationPath.clear();
    services.clear();
}

void GattApplication::buildManagedObjects(GVariantBuilder* builder) {
    // Root 객체의 인터페이스 추가
    g_variant_builder_open(builder, G_VARIANT_TYPE("{oa{sa{sv}}}"));
    g_variant_builder_add(builder, "o", getPath().c_str());
    
    // 서비스 객체들의 인터페이스 추가
    for (const auto& [uuid, service] : services) {
        service->addManagedObjectProperties(builder);
    }
    
    g_variant_builder_close(builder);
}

// 보호된 가상 메서드의 기본 구현
void GattApplication::onApplicationRegistered() {}
void GattApplication::onApplicationUnregistered() {}
void GattApplication::onServiceAdded(const std::shared_ptr<GattService>&) {}
void GattApplication::onServiceRemoved(const std::shared_ptr<GattService>&) {}

} // namespace ggk