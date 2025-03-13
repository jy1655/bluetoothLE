#pragma once

#include <gio/gio.h>
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include "Logger.h"
#include "DBusTypes.h"
#include "DBusError.h"
#include "DBusObjectPath.h"

namespace ggk {

class DBusConnection {
public:
    // 콜백 타입 정의
    using MethodHandler = std::function<void(const DBusMethodCall&)>;
    using SignalHandler = std::function<void(const std::string&, GVariantPtr)>;
    
    // 생성자 및 소멸자
    DBusConnection(GBusType busType = G_BUS_TYPE_SYSTEM);
    ~DBusConnection();
    
    // 연결 관리
    bool connect();
    bool disconnect();
    bool isConnected() const;
    
    // 메시지 전송
    GVariantPtr callMethod(
        const std::string& destination,
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& method,
        GVariantPtr parameters = makeNullGVariantPtr(),
        const std::string& replySignature = "",
        int timeoutMs = -1
    );
    
    bool emitSignal(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& signalName,
        GVariantPtr parameters = makeNullGVariantPtr()
    );
    
    // 객체 등록
    bool registerObject(
        const DBusObjectPath& path,
        const std::string& introspectionXml,
        const std::map<std::string, std::map<std::string, MethodHandler>>& methodHandlers,
        const std::map<std::string, std::vector<DBusProperty>>& properties
    );
    
    bool unregisterObject(const DBusObjectPath& path);
    
    // 속성 변경 알림
    bool emitPropertyChanged(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& propertyName,
        GVariantPtr value
    );
    
    // 시그널 처리
    guint addSignalWatch(
        const std::string& sender,
        const std::string& interface,
        const std::string& signalName,
        const DBusObjectPath& path,
        SignalHandler handler
    );
    
    bool removeSignalWatch(guint watchId);
    
    // 직접 GDBusConnection 액세스
    GDBusConnection* getRawConnection() const { return connection.get(); }

private:
    // D-Bus 연결
    GBusType busType;
    GDBusConnectionPtr connection;
    
    // 등록된 객체 추적
    std::map<std::string, guint> registeredObjects;
    std::map<guint, SignalHandler> signalHandlers;
    
    // 스레드 안전성
    mutable std::mutex mutex;
    
    // D-Bus 메서드 호출 핸들러
    static void handleMethodCall(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* methodName,
        GVariant* parameters,
        GDBusMethodInvocation* invocation,
        gpointer userData
    );
    
    // 속성 접근자 핸들러
    static GVariant* handleGetProperty(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* propertyName,
        GError** error,
        gpointer userData
    );
    
    static gboolean handleSetProperty(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* propertyName,
        GVariant* value,
        GError** error,
        gpointer userData
    );
    
    // 시그널 핸들러
    static void handleSignal(
        GDBusConnection* connection,
        const gchar* sender,
        const gchar* objectPath,
        const gchar* interfaceName,
        const gchar* signalName,
        GVariant* parameters,
        gpointer userData
    );
};

} // namespace ggk