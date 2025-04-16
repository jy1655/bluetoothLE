#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include "DBusTypes.h"
#include "DBusObjectPath.h"

namespace ggk {

/**
 * @brief D-Bus 추상 인터페이스
 *
 * 이 인터페이스는 D-Bus 구현체를 추상화하여 미래에 다른 D-Bus 라이브러리로
 * 전환할 수 있도록 합니다 (예: libdbus, sdbus-c++ 등).
 */
class IDBusConnection {
public:
    // 콜백 타입 정의
    using MethodHandler = std::function<void(const DBusMethodCall&)>;
    using SignalHandler = std::function<void(const std::string&, GVariantPtr)>;
    
    virtual ~IDBusConnection() = default;
    
    // 연결 관리
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // 메시지 전송
    virtual GVariantPtr callMethod(
        const std::string& destination,
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& method,
        GVariantPtr parameters = makeNullGVariantPtr(),
        const std::string& replySignature = "",
        int timeoutMs = -1
    ) = 0;
    
    // 시그널 발생
    virtual bool emitSignal(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& signalName,
        GVariantPtr parameters = makeNullGVariantPtr()
    ) = 0;
    
    // 객체 등록
    virtual bool registerObject(
        const DBusObjectPath& path,
        const std::string& introspectionXml,
        const std::map<std::string, std::map<std::string, MethodHandler>>& methodHandlers,
        const std::map<std::string, std::vector<DBusProperty>>& properties
    ) = 0;
    
    // 객체 등록 해제
    virtual bool unregisterObject(const DBusObjectPath& path) = 0;
    
    // 속성 변경 알림
    virtual bool emitPropertyChanged(
        const DBusObjectPath& path,
        const std::string& interface,
        const std::string& propertyName,
        GVariantPtr value
    ) = 0;
    
    // 시그널 감시
    virtual guint addSignalWatch(
        const std::string& sender,
        const std::string& interface,
        const std::string& signalName,
        const DBusObjectPath& path,
        SignalHandler handler
    ) = 0;
    
    // 시그널 감시 제거
    virtual bool removeSignalWatch(guint watchId) = 0;
};

/**
 * @brief D-Bus 연결 인터페이스 팩토리
 * 
 * 이 클래스는 D-Bus 연결 구현체를 생성하는 팩토리입니다.
 * 미래에 구현체를 전환할 때 이 팩토리만 수정하면 됩니다.
 */
class DBusConnectionFactory {
public:
    /**
     * @brief D-Bus 연결 구현체 생성
     * 
     * @param busType 버스 타입 (시스템 또는 세션)
     * @return 구현체 인스턴스
     */
    static std::shared_ptr<IDBusConnection> createConnection(GBusType busType = G_BUS_TYPE_SYSTEM);
};

} // namespace ggk