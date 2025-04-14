#include "DBusConnection.h"
#include <stdexcept>
#include "Utils.h"

namespace ggk {

// 핸들러 데이터 구조체
struct HandlerData {
    DBusConnection* connection;
    std::map<std::string, std::map<std::string, DBusConnection::MethodHandler>> methodHandlers;
    std::map<std::string, std::vector<DBusProperty>> properties;
};

DBusConnection::DBusConnection(GBusType busType)
    : busType(busType) {
}

DBusConnection::~DBusConnection() {
    if (connection) {
        disconnect();  // 연결 해제
    }
}

bool DBusConnection::connect() {
    if (isConnected()) {
        return true;
    }
    
    GError* error = nullptr;
    GDBusConnection* rawConnection = g_bus_get_sync(busType, nullptr, &error);
    
    if (!rawConnection) {
        if (error) {
            Logger::error("Failed to connect to D-Bus: " + std::string(error->message));
            g_error_free(error);
            error = nullptr;
        } else {
            Logger::error("Failed to connect to D-Bus: Unknown error");
        }
        return false;
    }
    
    // 중요: 참조 카운트를 명시적으로 증가시킨 후 스마트 포인터에 저장
    connection = makeGDBusConnectionPtr(g_object_ref_sink(rawConnection));
    
    // exit-on-close 설정 해제
    g_dbus_connection_set_exit_on_close(connection.get(), FALSE);
    
    Logger::info("Connected to D-Bus");
    return true;
}

bool DBusConnection::disconnect() {
    // 등록된 모든 객체 해제
    for (const auto& obj : registeredObjects) {
        g_dbus_connection_unregister_object(connection.get(), obj.second);
    }
    registeredObjects.clear();
    
    // 시그널 핸들러 해제
    for (const auto& handler : signalHandlers) {
        g_dbus_connection_signal_unsubscribe(connection.get(), handler.first);
    }
    signalHandlers.clear();
    
    // 연결 해제
    if (connection) {
        connection.reset();
        Logger::info("Disconnected from D-Bus");
    }
    
    return true;
}

bool DBusConnection::isConnected() const {
    return connection && !g_dbus_connection_is_closed(connection.get());
}


GVariantPtr DBusConnection::callMethod(
    const std::string& destination,
    const DBusObjectPath& path,
    const std::string& interface,
    const std::string& method,
    GVariantPtr parameters,
    const std::string& replySignature,
    int timeoutMs)
{
    if (!isConnected()) {
        Logger::error("Cannot call method: not connected to D-Bus");
        return makeNullGVariantPtr();
    }
    
    // Retry configuration
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 500;  // 0.5 second retry interval
    GError* error = nullptr;
    GVariantType* replyType = nullptr;
    
    // Use RAII for cleaning up GVariantType if needed
    std::unique_ptr<GVariantType, void(*)(GVariantType*)> replyTypeGuard(nullptr, 
        [](GVariantType* p) { if (p) g_variant_type_free(p); });
    
    try {
        // Validate reply signature if provided
        if (!replySignature.empty()) {
            if (!g_variant_type_string_is_valid(replySignature.c_str())) {
                Logger::error("Invalid reply signature: " + replySignature);
                return makeNullGVariantPtr();
            }
            replyType = g_variant_type_new(replySignature.c_str());
            replyTypeGuard.reset(replyType);  // Ensure cleanup
        }
        
        // Log the method call
        Logger::debug("Calling D-Bus method: " + destination + 
            " " + path.toString() + " " + 
            interface + "." + method);
        
        // Implement retry loop
        GVariant* result = nullptr;
        bool success = false;
        std::string lastErrorMessage;
        
        for (int attempt = 0; attempt < MAX_RETRIES && !success; attempt++) {
            if (attempt > 0) {
                Logger::info("Retrying D-Bus call (attempt " + std::to_string(attempt+1) + 
                           " of " + std::to_string(MAX_RETRIES) + "): " + interface + "." + method);
                // Reset error
                if (error) {
                    g_error_free(error);
                    error = nullptr;
                }
                // Delay before retry
                usleep(RETRY_DELAY_MS * 1000);
            }
            
            // Call the method
            result = g_dbus_connection_call_sync(
                connection.get(),
                destination.c_str(),
                path.c_str(),
                interface.c_str(),
                method.c_str(),
                parameters ? parameters.get() : nullptr,
                replyType,
                G_DBUS_CALL_FLAGS_NONE,
                timeoutMs > 0 ? timeoutMs : 30000,  // Default 30 second timeout
                nullptr,
                &error
            );
            
            // End loop if successful
            if (result) {
                success = true;
                break;
            }
            
            // Analyze error (don't retry certain errors)
            if (error) {
                lastErrorMessage = error->message ? error->message : "Unknown error";
                
                // Don't retry specific errors (e.g., non-existent service/method)
                if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
                    g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) ||
                    g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT) ||
                    g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE)) {
                    
                    Logger::error("D-Bus method call failed with non-retriable error: " + lastErrorMessage);
                    break;  // Stop retrying
                }
                
                Logger::warn("D-Bus method call failed (will retry): " + lastErrorMessage);
            }
        }
        
        // Check for final error
        if (!success && error) {
            std::string errorMessage = error->message ? error->message : "Unknown error";
            Logger::error("D-Bus method call failed: " + errorMessage);
            
            // Categorize common errors for better handling
            if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)) {
                Logger::error("D-Bus service not available: " + destination);
            } else if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                Logger::error("Unknown method: " + method + " on interface " + interface);
            } else if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT)) {
                Logger::error("Unknown object: " + path.toString());
            } else if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_TIMEOUT)) {
                Logger::error("D-Bus call timed out after " + std::to_string(timeoutMs) + "ms");
            }
            
            g_error_free(error);
            return makeNullGVariantPtr();
        }
        
        // Return the result (if any)
        if (result) {
            // Case 1: 이미 알고 있는 타입이 딕셔너리인데 튜플을 받은 경우
            if (replyType && 
                g_variant_type_equal(replyType, G_VARIANT_TYPE("a{oa{sa{sv}}}")) && 
                g_variant_is_of_type(result, G_VARIANT_TYPE("(a{oa{sa{sv}}})"))) {
                    
                Logger::debug("Detected tuple-wrapped dictionary from BlueZ 5.82+ (with type)");
                // Extract the dictionary from the tuple
                GVariant* extracted = g_variant_get_child_value(result, 0);
                g_variant_unref(result);
                
                if (extracted) {
                    return makeGVariantPtr(extracted, true);
                }
            }
            // Case 2: replyType이 없지만 GetManagedObjects 결과 포맷인 경우
            else if (!replyType && 
                    g_variant_is_of_type(result, G_VARIANT_TYPE("(a{oa{sa{sv}}})"))) {
                
                Logger::debug("Detected tuple-wrapped dictionary from BlueZ 5.82+ (no expected type)");
                // Extract the dictionary from the tuple
                GVariant* extracted = g_variant_get_child_value(result, 0);
                g_variant_unref(result);
                
                if (extracted) {
                    return makeGVariantPtr(extracted, true);
                }
            }
            
            // Normal case - return the result as-is
            return makeGVariantPtr(result, true);
        }
        
        return makeNullGVariantPtr();
    } catch (const std::exception& e) {
        Logger::error("Exception in D-Bus method call: " + std::string(e.what()));
        if (error) g_error_free(error);
        return makeNullGVariantPtr();
    }
}

bool DBusConnection::emitSignal(
    const DBusObjectPath& path,
    const std::string& interface,
    const std::string& signalName,
    GVariantPtr parameters)
{
    if (!isConnected()) {
        Logger::error("Cannot emit signal: not connected to D-Bus");
        return false;
    }
    
    GError* error = nullptr;
    gboolean success = g_dbus_connection_emit_signal(
        connection.get(),
        nullptr,  // 대상이 없음 (브로드캐스트)
        path.c_str(),
        interface.c_str(),
        signalName.c_str(),
        parameters ? parameters.get() : nullptr,
        &error
    );
    
    if (!success) {
        if (error) {
            Logger::error("Failed to emit D-Bus signal: " + std::string(error->message));
            g_error_free(error);
            error = nullptr; 
        } else {
            Logger::error("Failed to emit D-Bus signal: Unknown error");
        }
        return false;
    }
    
    return true;
}

bool DBusConnection::registerObject(
    const DBusObjectPath& path,
    const std::string& introspectionXml,
    const std::map<std::string, std::map<std::string, MethodHandler>>& methodHandlers,
    const std::map<std::string, std::vector<DBusProperty>>& properties)
{
    if (!isConnected()) {
        Logger::error("Cannot register object: not connected to D-Bus");
        return false;
    }
    
    // 이미 등록된 객체 확인
    std::lock_guard<std::mutex> lock(mutex);
    if (registeredObjects.find(path.toString()) != registeredObjects.end()) {
        Logger::warn("Object already registered at path: " + path.toString());
        return false;
    }
    
    // XML 파싱
    GError* error = nullptr;
    GDBusNodeInfo* nodeInfo = g_dbus_node_info_new_for_xml(introspectionXml.c_str(), &error);
    
    if (!nodeInfo) {
        if (error) {
            Logger::error("Failed to parse introspection XML: " + std::string(error->message));
            g_error_free(error);
            error = nullptr;
        } else {
            Logger::error("Failed to parse introspection XML: Unknown error");
        }
        return false;
    }
    
    // 핸들러 데이터 설정 - 소유권을 전달할 데이터 복사 생성
    HandlerData* data = new HandlerData();
    data->connection = this;
    data->methodHandlers = methodHandlers;
    data->properties = properties;
    
    // 인터페이스별 처리
    GDBusInterfaceVTable vtable = {
        handleMethodCall,
        handleGetProperty,
        handleSetProperty,
        { nullptr }  // 기타 필드 초기화
    };
    
    guint registrationId = 0;
    
    // 모든 인터페이스 등록
    for (GDBusInterfaceInfo** interfaces = nodeInfo->interfaces; interfaces && *interfaces; interfaces++) {
        registrationId = g_dbus_connection_register_object(
            connection.get(),
            path.c_str(),
            *interfaces,
            &vtable,
            data,
            [](gpointer data) { delete static_cast<HandlerData*>(data); },
            &error
        );
        
        if (registrationId == 0) {
            if (error) {
                Logger::error("Failed to register interface " + std::string((*interfaces)->name) + 
                             ": " + std::string(error->message));
                g_error_free(error);
                error = nullptr;
            } else {
                Logger::error("Failed to register interface " + std::string((*interfaces)->name));
            }
        }
    }
    
    g_dbus_node_info_unref(nodeInfo);
    
    if (registrationId == 0) {
        return false;
    }
    
    registeredObjects[path.toString()] = registrationId;
    Logger::info("Registered D-Bus object at path: " + path.toString());
    
    return true;
}

bool DBusConnection::unregisterObject(const DBusObjectPath& path) {
    if (!isConnected()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    auto it = registeredObjects.find(path.toString());
    if (it == registeredObjects.end()) {
        Logger::warn("No registered object at path: " + path.toString());
        return false;
    }
    
    if (g_dbus_connection_unregister_object(connection.get(), it->second)) {
        registeredObjects.erase(it);
        Logger::info("Unregistered D-Bus object at path: " + path.toString());
        return true;
    }
    
    Logger::error("Failed to unregister D-Bus object at path: " + path.toString());
    return false;
}

bool DBusConnection::emitPropertyChanged(
    const DBusObjectPath& path,
    const std::string& interface,
    const std::string& propertyName,
    GVariantPtr value)
{
    if (!isConnected()) {
        return false;
    }
    
    // 변경된 속성 사전 생성
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));  // 구체적인 타입 사용
    g_variant_builder_add(&builder, "{sv}", propertyName.c_str(), value.get());
    
    // 무효화된 속성 빈 배열
    GVariantBuilder invalidatedBuilder;
    g_variant_builder_init(&invalidatedBuilder, G_VARIANT_TYPE("as"));  // 구체적인 타입 사용
    
    // 시그널 매개변수 생성
    GVariant* params = g_variant_new("(sa{sv}as)",
        interface.c_str(),
        &builder,
        &invalidatedBuilder);
    
    // floating reference를 싱크하여 참조 카운트 관리
    GVariant* ref_sinked_params = g_variant_ref_sink(params);
    
    // cleanup: 빌더 리소스 해제
    g_variant_builder_clear(&builder);
    g_variant_builder_clear(&invalidatedBuilder);
    
    // 스마트 포인터로 래핑
    GVariantPtr paramsPtr = makeGVariantPtr(params, true); 
    
    // PropertiesChanged 시그널 발생
    return emitSignal(
        path,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        std::move(paramsPtr)
    );
}

guint DBusConnection::addSignalWatch(
    const std::string& sender,
    const std::string& interface,
    const std::string& signalName,
    const DBusObjectPath& path,
    SignalHandler handler)
{
    if (!isConnected()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    
    guint subscriptionId = g_dbus_connection_signal_subscribe(
        connection.get(),
        sender.empty() ? nullptr : sender.c_str(),
        interface.empty() ? nullptr : interface.c_str(),
        signalName.empty() ? nullptr : signalName.c_str(),
        path.toString().empty() ? nullptr : path.c_str(),
        nullptr,  // arg0
        G_DBUS_SIGNAL_FLAGS_NONE,
        handleSignal,
        this,
        nullptr  // user_data_free_func
    );
    
    if (subscriptionId > 0) {
        signalHandlers[subscriptionId] = handler;
        Logger::debug("Added signal watch: " + interface + "." + signalName);
    } else {
        Logger::error("Failed to add signal watch: " + interface + "." + signalName);
    }
    
    return subscriptionId;
}

bool DBusConnection::removeSignalWatch(guint watchId) {
    if (!isConnected() || watchId == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = signalHandlers.find(watchId);
    if (it == signalHandlers.end()) {
        return false;
    }
    
    g_dbus_connection_signal_unsubscribe(connection.get(), watchId);
    signalHandlers.erase(it);
    
    return true;
}

// 정적 핸들러 구현
void DBusConnection::handleMethodCall(
    GDBusConnection* connection,
    const gchar* sender,
    const gchar* objectPath,
    const gchar* interfaceName,
    const gchar* methodName,
    GVariant* parameters,
    GDBusMethodInvocation* invocation,
    gpointer userData)
{
    HandlerData* data = static_cast<HandlerData*>(userData);
    
    // 인터페이스 및 메서드 핸들러 찾기
    auto ifaceIt = data->methodHandlers.find(interfaceName);
    if (ifaceIt == data->methodHandlers.end()) {
        g_dbus_method_invocation_return_error(invocation, 
            g_quark_from_static_string(DBusError::ERROR_UNKNOWN_INTERFACE),
            0, "Unknown interface: %s", interfaceName);
        return;
    }
    
    auto methodIt = ifaceIt->second.find(methodName);
    if (methodIt == ifaceIt->second.end()) {
        g_dbus_method_invocation_return_error(invocation, 
            g_quark_from_static_string(DBusError::ERROR_UNKNOWN_METHOD),
            0, "Unknown method: %s.%s", interfaceName, methodName);
        return;
    }
    
    // 메서드 호출 처리
    DBusMethodCall call(
        sender ? sender : "",
        interfaceName ? interfaceName : "",
        methodName ? methodName : "",
        parameters ? makeGVariantPtr(parameters, false) : makeNullGVariantPtr(),
        invocation ? makeGDBusMethodInvocationPtr(g_object_ref(invocation)) : makeNullGDBusMethodInvocationPtr()
    );

    try {
        methodIt->second(call);
    } catch (const std::exception& e) {
        Logger::error("Exception in D-Bus method handler: " + std::string(e.what()));
        g_dbus_method_invocation_return_error(invocation, 
            g_quark_from_static_string(DBusError::ERROR_FAILED),
            0, "Internal error: %s", e.what());
    } catch (...) {
        Logger::error("Unknown exception in D-Bus method handler");
        g_dbus_method_invocation_return_error(invocation, 
            g_quark_from_static_string(DBusError::ERROR_FAILED),
            0, "Unknown internal error");
    }
}

// handleGetProperty 함수 수정
GVariant* DBusConnection::handleGetProperty(
    [[maybe_unused]] GDBusConnection* connection,
    [[maybe_unused]] const gchar* sender,
    [[maybe_unused]] const gchar* objectPath,
    const gchar* interfaceName,
    const gchar* propertyName,
    GError** error,
    gpointer userData)
{
    HandlerData* data = static_cast<HandlerData*>(userData);
    
    // 인터페이스 속성 찾기
    auto interfaceIt = data->properties.find(interfaceName);
    if (interfaceIt == data->properties.end()) {
        g_set_error(error, g_quark_from_static_string(DBusError::ERROR_UNKNOWN_INTERFACE),
                   0, "Unknown interface: %s", interfaceName);
        return nullptr;  // GVariantPtr 대신 nullptr 반환
    }
    
    // 속성 찾기
    for (const auto& prop : interfaceIt->second) {
        if (prop.name == propertyName) {
            if (!prop.readable) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_PROPERTY_READ_ONLY),
                           0, "Property is not readable: %s.%s", interfaceName, propertyName);
                return nullptr;  // GVariantPtr 대신 nullptr 반환
            }
            
            if (!prop.getter) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "No getter for property: %s.%s", interfaceName, propertyName);
                return nullptr;  // GVariantPtr 대신 nullptr 반환
            }
            
            try {
                return prop.getter();  
            } catch (const std::exception& e) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "Error getting property: %s", e.what());
                return nullptr;  // GVariantPtr 대신 nullptr 반환
            } catch (...) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "Unknown error getting property");
                return nullptr;  
            }
        }
    }
    
    g_set_error(error, g_quark_from_static_string(DBusError::ERROR_UNKNOWN_PROPERTY),
               0, "Unknown property: %s.%s", interfaceName, propertyName);
    return nullptr; 
}

gboolean DBusConnection::handleSetProperty(
    GDBusConnection* connection,
    const gchar* sender,
    const gchar* objectPath,
    const gchar* interfaceName,
    const gchar* propertyName,
    GVariant* value,
    GError** error,
    gpointer userData)
{
    HandlerData* data = static_cast<HandlerData*>(userData);
    
    // 인터페이스 속성 찾기
    auto interfaceIt = data->properties.find(interfaceName);
    if (interfaceIt == data->properties.end()) {
        g_set_error(error, g_quark_from_static_string(DBusError::ERROR_UNKNOWN_INTERFACE),
                   0, "Unknown interface: %s", interfaceName);
        return FALSE;
    }
    
    // 속성 찾기
    for (const auto& prop : interfaceIt->second) {
        if (prop.name == propertyName) {
            if (!prop.writable) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_PROPERTY_READ_ONLY),
                           0, "Property is not writable: %s.%s", interfaceName, propertyName);
                return FALSE;
            }
            
            if (!prop.setter) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "No setter for property: %s.%s", interfaceName, propertyName);
                return FALSE;
            }
            
            try {
                return prop.setter(value) ? TRUE : FALSE;
            } catch (const std::exception& e) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "Error setting property: %s", e.what());
                return FALSE;
            } catch (...) {
                g_set_error(error, g_quark_from_static_string(DBusError::ERROR_FAILED),
                           0, "Unknown error setting property");
                return FALSE;
            }
        }
    }
    
    g_set_error(error, g_quark_from_static_string(DBusError::ERROR_UNKNOWN_PROPERTY),
               0, "Unknown property: %s.%s", interfaceName, propertyName);
    return FALSE;
}

void DBusConnection::handleSignal(
    GDBusConnection* connection,
    const gchar* sender,
    const gchar* objectPath,
    const gchar* interfaceName,
    const gchar* signalName,
    GVariant* parameters,
    gpointer userData)
{
    DBusConnection* self = static_cast<DBusConnection*>(userData);
    
    std::lock_guard<std::mutex> lock(self->mutex);
    
    for (const auto& handler : self->signalHandlers) {
        try {
            handler.second(
                std::string(signalName ? signalName : ""),
                parameters ? makeGVariantPtr(parameters, false) : makeNullGVariantPtr() // false = 소유권 이전 안 함
            );
        } catch (const std::exception& e) {
            Logger::error("Exception in D-Bus signal handler: " + std::string(e.what()));
        } catch (...) {
            Logger::error("Unknown exception in D-Bus signal handler");
        }
    }
}

} // namespace ggk