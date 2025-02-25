// DBusTypes.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ggk {

// D-Bus 인자 정의 - 다른 구조체들이 참조하므로 먼저 정의
struct DBusArgument {
    std::string signature;   // D-Bus 타입 시그니처
    std::string name;        // 인자 이름
    std::string direction;   // "in" 또는 "out"
};

// D-Bus 메시지 타입 정의
enum class DBusMessageType {
    METHOD_CALL,
    METHOD_RETURN,
    ERROR,
    SIGNAL
};

// D-Bus 기본 인터페이스 상수
struct DBusInterface {
    static constexpr const char* PROPERTIES = "org.freedesktop.DBus.Properties";
    static constexpr const char* INTROSPECTABLE = "org.freedesktop.DBus.Introspectable";
    static constexpr const char* OBJECTMANAGER = "org.freedesktop.DBus.ObjectManager";
};

struct DBusProperty {
    std::string name;
    std::string signature;
    bool readable;
    bool writable;
    bool emitsChangedSignal;
    std::function<GVariant*()> getter;
    std::function<bool(GVariant*)> setter;
};

struct DBusMethodCall {
    std::string sender;
    std::string interface;
    std::string method;
    GVariant* parameters;
    GDBusMethodInvocation* invocation;
};

struct DBusSignal {
    std::string name;
    std::vector<DBusArgument> arguments;
};

// 메모리 관리를 위한 RAII 래퍼
using GVariantPtr = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
using GDBusConnectionPtr = std::unique_ptr<GDBusConnection, decltype(&g_object_unref)>;

} // namespace ggk