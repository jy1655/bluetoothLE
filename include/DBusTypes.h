#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ggk {

// GLib 객체를 위한 사용자 정의 삭제자 함수
inline void gvariant_deleter(GVariant* p) {
    if (p) g_variant_unref(p);
}

inline void gobject_deleter(void* p) {
    if (p) g_object_unref(G_OBJECT(p));
}

inline void gerror_deleter(GError* p) {
    if (p) g_error_free(p);
}

inline void gdbusnodeinfo_deleter(GDBusNodeInfo* p) {
    if (p) g_dbus_node_info_unref(p);
}

inline void gvariantbuilder_deleter(GVariantBuilder* p) {
    if (p) g_variant_builder_unref(p);
}

// 스마트 포인터 정의
using GVariantPtr = std::unique_ptr<GVariant, decltype(&gvariant_deleter)>;
using GDBusConnectionPtr = std::unique_ptr<GDBusConnection, decltype(&gobject_deleter)>;
using GDBusMethodInvocationPtr = std::unique_ptr<GDBusMethodInvocation, decltype(&gobject_deleter)>;
using GErrorPtr = std::unique_ptr<GError, decltype(&gerror_deleter)>;
using GDBusNodeInfoPtr = std::unique_ptr<GDBusNodeInfo, decltype(&gdbusnodeinfo_deleter)>;
using GVariantBuilderPtr = std::unique_ptr<GVariantBuilder, decltype(&gvariantbuilder_deleter)>;
using GDBusProxyPtr = std::unique_ptr<GDBusProxy, decltype(&gobject_deleter)>;
using GCancellablePtr = std::unique_ptr<GCancellable, decltype(&gobject_deleter)>;
using GDBusMessagePtr = std::unique_ptr<GDBusMessage, decltype(&gobject_deleter)>;

// NULL 스마트 포인터 생성 헬퍼 함수
inline GVariantPtr makeNullGVariantPtr() {
    return GVariantPtr(nullptr, &gvariant_deleter);
}

inline GDBusMethodInvocationPtr makeNullGDBusMethodInvocationPtr() {
    return GDBusMethodInvocationPtr(nullptr, &gobject_deleter);
}

inline GDBusProxyPtr makeNullGDBusProxyPtr() {
    return GDBusProxyPtr(nullptr, &gobject_deleter);
}

inline GDBusMessagePtr makeNullGDBusMessagePtr() {
    return GDBusMessagePtr(nullptr, &gobject_deleter);
}

inline GErrorPtr makeNullGErrorPtr() {
    return GErrorPtr(nullptr, &gerror_deleter);
}

// GVariant 관련 스마트 포인터 생성 함수 (GVariant는 참조 카운트만 관리)
inline GVariantPtr makeGVariantPtr(GVariant* p) {
    return GVariantPtr(p, &gvariant_deleter);
}

// GObject 기반 스마트 포인터 생성 함수 (참조 카운트 증가 패턴 적용)
inline GDBusProxyPtr makeGDBusProxyPtr(GDBusProxy* p) {
    return p ? GDBusProxyPtr(g_object_ref(p), &gobject_deleter) : GDBusProxyPtr(nullptr, &gobject_deleter);
}

inline GDBusMethodInvocationPtr makeGDBusMethodInvocationPtr(GDBusMethodInvocation* p) {
    return p ? GDBusMethodInvocationPtr(g_object_ref(p), &gobject_deleter) : GDBusMethodInvocationPtr(nullptr, &gobject_deleter);
}

inline GDBusMessagePtr makeGDBusMessagePtr(GDBusMessage* p) {
    return p ? GDBusMessagePtr(g_object_ref(p), &gobject_deleter) : GDBusMessagePtr(nullptr, &gobject_deleter);
}

inline GErrorPtr makeGErrorPtr(GError* p) {
    return GErrorPtr(p, &gerror_deleter);
}

inline GDBusNodeInfoPtr makeGDBusNodeInfoPtr(GDBusNodeInfo* p) {
    return GDBusNodeInfoPtr(p, &gdbusnodeinfo_deleter);
}

inline GVariantBuilderPtr makeGVariantBuilderPtr(GVariantBuilder* p) {
    return GVariantBuilderPtr(p, &gvariantbuilder_deleter);
}

inline GDBusConnectionPtr makeGDBusConnectionPtr(GDBusConnection* p) {
    return p ? GDBusConnectionPtr(g_object_ref(p), &gobject_deleter) : GDBusConnectionPtr(nullptr, &gobject_deleter);
}

inline GCancellablePtr makeGCancellablePtr(GCancellable* p) {
    return p ? GCancellablePtr(g_object_ref(p), &gobject_deleter) : GCancellablePtr(nullptr, &gobject_deleter);
}

// D-Bus 인자 정의
struct DBusArgument {
    std::string signature;   // D-Bus 타입 시그니처
    std::string name;        // 인자 이름
    std::string direction;   // "in" 또는 "out"
    std::string description; // 인자 설명
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

// D-Bus 속성 구조체
struct DBusProperty {
    std::string name;
    std::string signature;
    bool readable;
    bool writable;
    bool emitsChangedSignal;
    std::function<GVariant*()> getter;
    std::function<bool(GVariant*)> setter;
};

// D-Bus 메서드 호출 구조체
struct DBusMethodCall {
    std::string sender;
    std::string interface;
    std::string method;
    GVariantPtr parameters;
    GDBusMethodInvocationPtr invocation;
    
    // 기본 생성자
    DBusMethodCall() :
        parameters(nullptr, &gvariant_deleter),
        invocation(nullptr, &gobject_deleter) {}
    
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

// D-Bus 시그널 구조체
struct DBusSignal {
    std::string name;
    std::vector<DBusArgument> arguments;
};

// D-Bus 인트로스펙션 구조체
struct DBusIntrospection {
    bool includeStandardInterfaces = true;
    std::map<std::string, std::string> annotations;
};

} // namespace ggk