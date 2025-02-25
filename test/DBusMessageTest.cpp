#include <gtest/gtest.h>
#include <gio/gio.h>
#include "../include/DBusXml.h"
#include "../include/DBusMessage.h"
#include "../include/DBusError.h"
#include "../include/Logger.h"

using namespace ggk;

class DBusMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        GError* error = nullptr;
        
        // DBus 연결 설정
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!connection) {
            Logger::error(SSTR << "Failed to get session bus connection: " 
                << (error ? error->message : "Unknown error"));
            if (error) g_error_free(error);
            FAIL() << "Failed to get session bus connection";
            return;
        }
        Logger::info(SSTR << "DBus session connection established with unique name: " 
            << g_dbus_connection_get_unique_name(connection));

        // 권한 체크
        gchar* owner = nullptr;
        error = nullptr;
        owner = g_dbus_connection_get_unique_name(connection);
        if (!owner) {
            Logger::error("Failed to get connection owner name");
            FAIL() << "Failed to get connection owner name";
            return;
        }
        Logger::info(SSTR << "Connection owner: " << owner);
        g_free(owner);

        registerDBusService();
    }

    void TearDown() override {
        if (connection) {
            if (object_id > 0) {
                g_dbus_connection_unregister_object(connection, object_id);
                Logger::debug("Unregistered DBus object");
            }
            
            if (service_id > 0) {
                g_bus_unown_name(service_id);
                Logger::debug("Released DBus service name");
            }

            g_object_unref(connection);
            Logger::debug("Released DBus connection");
        }

        if (introspection_data) {
            g_dbus_node_info_unref(introspection_data);
            Logger::debug("Released introspection data");
        }
    }

    static void handle_method_call(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* method_name, 
        GVariant* parameters,
        GDBusMethodInvocation* invocation,
        gpointer user_data)
    {
        Logger::debug(SSTR << "Received method call: " << method_name << " from " << sender);
        current_invocation = invocation;
        current_parameters = parameters;
    }

    static GVariant* get_property(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* property_name,
        GError** error,
        gpointer user_data)
    {
        Logger::debug(SSTR << "Get property request: " << property_name);
        
        if (g_strcmp0(property_name, "TestProperty") == 0) {
            return g_variant_new_string("DefaultValue");
        }
    
        Logger::warn(SSTR << "Property not found: " << property_name);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Property not found");
        return nullptr;
    }
    
    static gboolean set_property(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* object_path,
        const gchar* interface_name,
        const gchar* property_name,
        GVariant* value,
        GError** error,
        gpointer user_data)
    {
        Logger::debug(SSTR << "Set property request: " << property_name);
        
        if (g_strcmp0(property_name, "TestProperty") == 0) {
            return TRUE;
        }
    
        Logger::warn(SSTR << "Property not found: " << property_name);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Property not found");
        return FALSE;
    }

    void registerDBusService() {
        Logger::info("Registering DBus service...");

        // DBus 서비스 소유 설정
        service_id = g_bus_own_name(
            G_BUS_TYPE_SESSION,
            "org.test.Service",
            G_BUS_NAME_OWNER_FLAGS_NONE,
            nullptr, nullptr, nullptr, nullptr, nullptr
        );

        if (service_id == 0) {
            Logger::error("Failed to own DBus service name");
            FAIL() << "Failed to own DBus service name";
            return;
        }

        // DBusXml을 사용하여 인터페이스 XML 생성
        DBusProperty prop = {"TestProperty", "s", true, true, true, nullptr, nullptr};
        DBusMethodCall method = {"", "org.test.Interface", "TestMethod", nullptr, nullptr};
        DBusSignal signal = {"TestSignal", {{"s", "message", ""}}};

        std::string interfaceXml = DBusXml::createInterface(
            "org.test.Interface",
            {prop}, {method}, {signal}
        );

        Logger::debug(SSTR << "Generated interface XML:\n" << interfaceXml);

        GError* error = nullptr;
        introspection_data = g_dbus_node_info_new_for_xml(interfaceXml.c_str(), &error);
        if (!introspection_data) {
            Logger::error(SSTR << "Failed to parse introspection XML: " 
                << (error ? error->message : "Unknown error"));
            if (error) g_error_free(error);
            FAIL() << "Failed to parse introspection XML";
            return;
        }

        GDBusInterfaceVTable interface_vtable = {
            handle_method_call,
            get_property, 
            set_property,
            {0}
        };

        object_id = g_dbus_connection_register_object(
            connection,
            "/org/test/Object",
            introspection_data->interfaces[0],
            &interface_vtable,
            nullptr,
            nullptr,
            &error
        );

        if (object_id == 0) {
            Logger::error(SSTR << "Failed to register DBus object: " 
                << (error ? error->message : "Unknown error"));
            if (error) g_error_free(error);
            FAIL() << "Failed to register DBus object";
            return;
        }

        Logger::info("DBus service successfully registered");
    }

    GDBusConnection* connection = nullptr;
    guint object_id = 0;
    guint service_id = 0;
    GDBusNodeInfo* introspection_data = nullptr;
    static inline GDBusMethodInvocation* current_invocation = nullptr;
    static inline GVariant* current_parameters = nullptr;
};

TEST_F(DBusMessageTest, CallMethodAndVerifyResponse) {
    Logger::info("Testing method call and response...");

    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        "org.test.Service",
        "/org/test/Object",
        "org.test.Interface",
        "TestMethod",
        g_variant_new("(s)", "TestParam"),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        Logger::error(SSTR << "Method call failed: " << error->message);
        g_error_free(error);
        FAIL() << "Method call failed";
        return;
    }

    ASSERT_NE(current_invocation, nullptr) << "Failed to receive method invocation";
    ASSERT_NE(current_parameters, nullptr) << "Failed to receive parameters";

    const gchar* received_value = nullptr;
    g_variant_get(current_parameters, "(s)", &received_value);
    EXPECT_STREQ(received_value, "TestParam");

    DBusMessage response = DBusMessage::createMethodReturn(current_invocation);
    response.addArgument(g_variant_new("(s)", "ResponseMessage"));
    Logger::info("Method call test completed successfully");
}

TEST_F(DBusMessageTest, SendAndReceiveSignal) {
    Logger::info("Testing signal send and receive...");
    
    bool signal_received = false;
    
    guint subscription_id = g_dbus_connection_signal_subscribe(
        connection,
        "org.test.Service",
        "org.test.Interface",
        "TestSignal",
        "/org/test/Object",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection*, const gchar*, const gchar*,
           const gchar*, const gchar*, GVariant* parameters,
           gpointer user_data) {
            Logger::debug("Signal received");
            bool* received = static_cast<bool*>(user_data);
            *received = true;
            
            const gchar* signal_data = nullptr;
            g_variant_get(parameters, "(s)", &signal_data);
            EXPECT_STREQ(signal_data, "TestSignalData");
        },
        &signal_received,
        nullptr
    );

    GError* error = nullptr;
    g_dbus_connection_emit_signal(
        connection,
        nullptr,
        "/org/test/Object",
        "org.test.Interface",
        "TestSignal",
        g_variant_new("(s)", "TestSignalData"),
        &error
    );

    if (error) {
        Logger::error(SSTR << "Failed to emit signal: " << error->message);
        g_error_free(error);
        FAIL() << "Failed to emit signal";
        return;
    }

    for (int i = 0; i < 100 && !signal_received; ++i) {
        g_main_context_iteration(nullptr, FALSE);
    }
    
    EXPECT_TRUE(signal_received);
    g_dbus_connection_signal_unsubscribe(connection, subscription_id);
    Logger::info("Signal test completed successfully");
}