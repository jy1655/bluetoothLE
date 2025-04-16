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
#include "DBusInterface.h"

namespace ggk {

/**
 * @brief D-Bus 연결을 관리하는 클래스 (GLib/GIO 구현)
 * 
 * 이 클래스는 D-Bus 연결을 생성하고 관리하며, 메서드 호출, 시그널 발생,
 * 객체 등록 등의 D-Bus 통신 기능을 제공합니다.
 */
class DBusConnection : public IDBusConnection {
public:
    /**
     * @brief 생성자
     * 
     * @param busType 연결할 D-Bus 유형 (시스템 또는 세션)
     */
    DBusConnection(GBusType busType = G_BUS_TYPE_SYSTEM);
    
    /**
     * @brief 소멸자
     * 
     * 모든 등록된 객체를 해제하고 연결을 종료합니다.
     */
    ~DBusConnection() override;
    
    // 복사 방지
    DBusConnection(const DBusConnection&) = delete;
    DBusConnection& operator=(const DBusConnection&) = delete;
    
    // IDBusConnection 인터페이스 구현
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    
    GVariantPtr callMethod(
        const std::string& destination,
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& method,
        GVariantPtr parameters = makeNullGVariantPtr(),
        const std::string& replySignature = "",
        int timeoutMs = -1
    ) override;
    
    bool emitSignal(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& signalName,
        GVariantPtr parameters = makeNullGVariantPtr()
    ) override;
    
    bool registerObject(
        const DBusObjectPath& path,
        const std::string& introspectionXml,
        const std::map<std::string, std::map<std::string, MethodHandler>>& methodHandlers,
        const std::map<std::string, std::vector<DBusProperty>>& properties
    ) override;
    
    bool unregisterObject(const DBusObjectPath& path) override;
    
    bool emitPropertyChanged(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& propertyName,
        GVariantPtr value
    ) override;
    
    guint addSignalWatch(
        const std::string& sender,
        const std::string& interface,
        const std::string& signalName,
        const DBusObjectPath& path,
        SignalHandler handler
    ) override;
    
    bool removeSignalWatch(guint watchId) override;
    
    /**
     * @brief 원시 GDBusConnection 획득
     * 
     * @return 원시 GDBusConnection 포인터
     */
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

// 팩토리 클래스 메서드 구현
inline std::shared_ptr<IDBusConnection> DBusConnectionFactory::createConnection(GBusType busType) {
    return std::make_shared<DBusConnection>(busType);
}

} // namespace ggk