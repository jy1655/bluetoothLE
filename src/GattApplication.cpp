#include "GattApplication.h"
#include "GattService.h"
#include "Logger.h"

namespace ggk {

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

    const std::string& uuid = service->getUUID();
    if (services.find(uuid) != services.end()) {
        Logger::error("Service with UUID " + uuid + " already exists");
        return false;
    }

    services[uuid] = service;
    
    // 서비스를 D-Bus 객체 트리에 추가
    std::string path = "/service" + std::to_string(services.size());
    rootObject.addChild(DBusObjectPath(path));

    onServiceAdded(service);
    Logger::info("Added GATT service: " + uuid);
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

    // 각 서비스에 대한 객체 경로와 인터페이스 정보 추가
    for (const auto& pair : services) {
        const auto& service = pair.second;
        std::string path = applicationPath + "/service" + pair.first;

        GVariantBuilder interfaceBuilder;
        g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

        // 서비스 프로퍼티 추가
        service->addManagedObjectProperties(&interfaceBuilder);

        g_variant_builder_add(&builder, "{oa{sa{sv}}}", 
                            path.c_str(),
                            &interfaceBuilder);
    }

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

// 보호된 가상 메서드의 기본 구현
void GattApplication::onApplicationRegistered() {}
void GattApplication::onApplicationUnregistered() {}
void GattApplication::onServiceAdded(const std::shared_ptr<GattService>&) {}
void GattApplication::onServiceRemoved(const std::shared_ptr<GattService>&) {}

} // namespace ggk