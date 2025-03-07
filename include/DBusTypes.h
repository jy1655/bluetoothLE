// DBusTypes.h
#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ggk {

// 스마트 포인터 정의
using GVariantPtr = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
using GDBusConnectionPtr = std::unique_ptr<GDBusConnection, decltype(&g_object_unref)>;
using GDBusMethodInvocationPtr = std::unique_ptr<GDBusMethodInvocation, decltype(&g_object_unref)>;
using GErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;
using GDBusNodeInfoPtr = std::unique_ptr<GDBusNodeInfo, decltype(&g_dbus_node_info_unref)>;
using GVariantBuilderPtr = std::unique_ptr<GVariantBuilder, decltype(&g_variant_builder_unref)>;
using GDBusProxyPtr = std::unique_ptr<GDBusProxy, decltype(&g_object_unref)>;
using GCancellablePtr = std::unique_ptr<GCancellable, decltype(&g_object_unref)>;
using GDBusMessagePtr = std::unique_ptr<GDBusMessage, decltype(&g_object_unref)>;


// 스마트 포인터 생성 헬퍼 함수 (직접 호출할 필요는 없으나 편의를 위해 제공)
inline GVariantPtr makeNullGVariantPtr() {
    return GVariantPtr(nullptr, &g_variant_unref);
}

inline GDBusMethodInvocationPtr makeNullGDBusMethodInvocationPtr() {
    return GDBusMethodInvocationPtr(nullptr, &g_object_unref);
}

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
    GVariantPtr parameters;
    GDBusMethodInvocationPtr invocation;
    
    // 기본 생성자
    DBusMethodCall() : 
        parameters(nullptr, &g_variant_unref),
        invocation(nullptr, &g_object_unref) {}
    
    // 복사 생성자/할당 연산자 (삭제 - 이 객체는 복사 불가)
    DBusMethodCall(const DBusMethodCall&) = delete;
    DBusMethodCall& operator=(const DBusMethodCall&) = delete;
    
    // 이동 생성자/할당 연산자 (허용)
    DBusMethodCall(DBusMethodCall&&) = default;
    DBusMethodCall& operator=(DBusMethodCall&&) = default;
    
    // 전체 필드 초기화 생성자
    DBusMethodCall(
        const std::string& s, 
        const std::string& i, 
        const std::string& m, 
        GVariantPtr p, 
        GDBusMethodInvocationPtr inv
    ) : sender(s), interface(i), method(m), 
        parameters(std::move(p)), invocation(std::move(inv)) {}
};

struct DBusSignal {
    std::string name;
    std::vector<DBusArgument> arguments;
};

} // namespace ggk