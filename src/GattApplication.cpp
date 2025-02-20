#include "GattApplication.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"
#include "Logger.h"

namespace ggk {

const char* GattApplication::INTERFACE_NAME = "org.freedesktop.DBus.ObjectManager";
const char* GattApplication::BLUEZ_GATT_MANAGER_IFACE = "org.bluez.GattManager1";
const char* GattApplication::BLUEZ_SERVICE_NAME = "org.bluez";
const char* GattApplication::BLUEZ_OBJECT_PATH = "/org/bluez/hci0";

GattApplication::GattApplication()
   : DBusInterface(INTERFACE_NAME)
   , objectPath("/")
   , dbusConnection(nullptr) {
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
   onServiceAdded(service);
   Logger::debug("Added GATT service: " + uuid);
   return true;
}

std::shared_ptr<GattService> GattApplication::getService(const GattUuid& uuid) {
   auto it = services.find(uuid.toString());
   return (it != services.end()) ? it->second : nullptr;
}

void GattApplication::removeService(const GattUuid& uuid) {
   auto it = services.find(uuid.toString());
   if (it != services.end()) {
       auto service = it->second;
       services.erase(it);
       onServiceRemoved(service);
       Logger::debug("Removed GATT service: " + uuid.toString());
   }
}

bool GattApplication::registerApplication(GDBusConnection* connection) {
   if (!connection) {
       Logger::error("No D-Bus connection provided");
       return false;
   }

   if (isRegistered()) {
       Logger::warn("GATT application already registered");
       return true;
   }

   dbusConnection = connection;

   // GetManagedObjects 메서드 등록
   const char* xmlInterface = R"XML(
       <node>
           <interface name="org.freedesktop.DBus.ObjectManager">
               <method name="GetManagedObjects">
                   <arg name="objects" type="a{oa{sa{sv}}}" direction="out"/>
               </method>
           </interface>
       </node>
   )XML";

   // 메서드 핸들러 설정
   const GDBusInterfaceVTable interfaceVTable = {
       nullptr,  // method_call
       nullptr,  // get_property
       nullptr,  // set_property
       { nullptr }  // padding
   };

   GError* error = nullptr;
   GDBusNodeInfo* introspection = g_dbus_node_info_new_for_xml(xmlInterface, &error);
   if (error) {
       Logger::error("Failed to parse interface XML: " + std::string(error->message));
       g_error_free(error);
       return false;
   }

   g_dbus_connection_register_object(
       connection,
       objectPath.c_str(),
       introspection->interfaces[0],
       &interfaceVTable,
       this,
       nullptr,
       &error
   );

   if (error) {
       Logger::error("Failed to register object: " + std::string(error->message));
       g_error_free(error);
       g_dbus_node_info_unref(introspection);
       return false;
   }

   // GattManager1에 애플리케이션 등록
   GVariant* params = g_variant_new("(oa{sv})", 
                                  objectPath.c_str(),
                                  nullptr);  // 옵션 없음

   g_dbus_connection_call(
       connection,
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
       this
   );

   g_dbus_node_info_unref(introspection);
   Logger::info("Registering GATT application");
   return true;
}

void GattApplication::unregisterApplication() {
   if (!isRegistered()) {
       return;
   }

   g_dbus_connection_call(
       dbusConnection,
       BLUEZ_SERVICE_NAME,
       BLUEZ_OBJECT_PATH,
       BLUEZ_GATT_MANAGER_IFACE,
       "UnregisterApplication",
       g_variant_new("(o)", objectPath.c_str()),
       nullptr,
       G_DBUS_CALL_FLAGS_NONE,
       -1,
       nullptr,
       (GAsyncReadyCallback)onUnregisterApplicationReply,
       this
   );

   Logger::info("Unregistering GATT application");
}

GVariant* GattApplication::getManagedObjects() {
   GVariantBuilder builder;
   g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
   
   buildManagedObjects(&builder);
   
   return g_variant_builder_end(&builder);
}

void GattApplication::buildManagedObjects(GVariantBuilder* builder) {
   // 서비스 객체 추가
   for (const auto& [uuid, service] : services) {
       GVariantBuilder interfaceBuilder;
       g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

       // 서비스 프로퍼티 추가
       GVariantBuilder propsBuilder;
       g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));
       
       // UUID
       g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                           g_variant_new_string(service->getUUID().toString128().c_str()));
       
       // Primary
       g_variant_builder_add(&propsBuilder, "{sv}", "Primary",
                           g_variant_new_boolean(service->isPrimary()));

       g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                           GattService::INTERFACE_NAME,
                           &propsBuilder);

       g_variant_builder_add(builder, "{oa{sa{sv}}}",
                           service->getPath().c_str(),
                           &interfaceBuilder);

       // Characteristic 객체 추가
       for (const auto& characteristic : service->getCharacteristics()) {
           g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
           g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));

           // Characteristic 프로퍼티 추가
           g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                               g_variant_new_string(characteristic->getUUID().toString128().c_str()));
           g_variant_builder_add(&propsBuilder, "{sv}", "Service",
                               g_variant_new_object_path(service->getPath().c_str()));
           
           g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                               GattCharacteristic::INTERFACE_NAME,
                               &propsBuilder);

           g_variant_builder_add(builder, "{oa{sa{sv}}}",
                               characteristic->getPath().c_str(),
                               &interfaceBuilder);

           // Descriptor 객체 추가
           for (const auto& descriptor : characteristic->getDescriptors()) {
               g_variant_builder_init(&interfaceBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
               g_variant_builder_init(&propsBuilder, G_VARIANT_TYPE("a{sv}"));

               // Descriptor 프로퍼티 추가
               g_variant_builder_add(&propsBuilder, "{sv}", "UUID",
                                   g_variant_new_string(descriptor->getUUID().toString128().c_str()));
               g_variant_builder_add(&propsBuilder, "{sv}", "Characteristic",
                                   g_variant_new_object_path(characteristic->getPath().c_str()));

               g_variant_builder_add(&interfaceBuilder, "{sa{sv}}",
                                   GattDescriptor::INTERFACE_NAME,
                                   &propsBuilder);

               g_variant_builder_add(builder, "{oa{sa{sv}}}",
                                   descriptor->getPath().c_str(),
                                   &interfaceBuilder);
           }
       }
   }
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

   self->dbusConnection = nullptr;
   self->onApplicationUnregistered();
   Logger::info("GATT application unregistered successfully");
}

// Protected virtual methods의 기본 구현
void GattApplication::onApplicationRegistered() {}
void GattApplication::onApplicationUnregistered() {}
void GattApplication::onServiceAdded(const std::shared_ptr<GattService>&) {}
void GattApplication::onServiceRemoved(const std::shared_ptr<GattService>&) {}

GVariant* GattApplication::getObjectsManaged() {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    
    buildManagedObjects(&builder);
    
    return g_variant_builder_end(&builder);
}

} // namespace ggk