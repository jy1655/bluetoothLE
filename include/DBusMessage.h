// DBusMessage.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include "DBusTypes.h"
#include "DBusError.h"

namespace ggk {

class DBusMessage {
public:
    // 메시지 생성 메서드들
    static DBusMessage createMethodCall(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& method
    );

    static DBusMessage createMethodReturn(GDBusMethodInvocation* invocation);
    
    static DBusMessage createSignal(
        const std::string& path,
        const std::string& interface,
        const std::string& name
    );

    static DBusMessage createError(
        GDBusMethodInvocation* invocation,
        const DBusError& error
    );

    // 파라미터 관리
    void addArgument(GVariant* variant);
    void addArguments(GVariant* variant, ...);
    GVariant* getBody() const;

    // 메시지 속성 접근자
    DBusMessageType getType() const;
    std::string getInterface() const;
    std::string getPath() const;
    std::string getMember() const;
    std::string getDestination() const;
    std::string getSender() const;
    std::string getSignature() const;

    // GDBusMessage 직접 접근이 필요한 경우를 위한 getter
    GDBusMessage* getMessage() const { return message.get(); }

private:
    // RAII를 위한 unique_ptr wrapper
    using GDBusMessagePtr = std::unique_ptr<GDBusMessage, decltype(&g_object_unref)>;
    GDBusMessagePtr message;

    // private 생성자 - 팩토리 메서드를 통해서만 생성 가능
    explicit DBusMessage(GDBusMessage* message);
    
    // 복사 금지
    DBusMessage(const DBusMessage&) = delete;
    DBusMessage& operator=(const DBusMessage&) = delete;
};

} // namespace ggk