#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <type_traits>

namespace ggk {

//
// 1. GLib 객체 삭제자 함수 정의
//

/**
 * @brief GVariant 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gvariant_deleter(GVariant* p) {
    if (p) g_variant_unref(p);
}

/**
 * @brief GObject 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gobject_deleter(void* p) {
    if (p) g_object_unref(G_OBJECT(p));
}

/**
 * @brief GError 타입의 객체를 삭제하는 함수
 */
inline void gerror_deleter(GError* p) {
    if (p) g_error_free(p);
}

/**
 * @brief GDBusNodeInfo 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gdbusnodeinfo_deleter(GDBusNodeInfo* p) {
    if (p) g_dbus_node_info_unref(p);
}

/**
 * @brief GVariantBuilder 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gvariantbuilder_deleter(GVariantBuilder* p) {
    if (p) g_variant_builder_unref(p);
}

/**
 * @brief GDBusInterfaceInfo 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gdbusinterfaceinfo_deleter(GDBusInterfaceInfo* p) {
    if (p) g_dbus_interface_info_unref(p);
}

/**
 * @brief GMainLoop 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gmainloop_deleter(GMainLoop* p) {
    if (p) g_main_loop_unref(p);
}

/**
 * @brief GSource 타입의 객체를 삭제(참조 카운트 감소)하는 함수
 */
inline void gsource_deleter(GSource* p) {
    if (p) g_source_unref(p);
}

//
// 2. 스마트 포인터 타입 정의
//

/**
 * @brief GVariant 객체에 대한 스마트 포인터 타입
 */
using GVariantPtr = std::unique_ptr<GVariant, decltype(&gvariant_deleter)>;

/**
 * @brief GDBusConnection 객체에 대한 스마트 포인터 타입
 */
using GDBusConnectionPtr = std::unique_ptr<GDBusConnection, decltype(&gobject_deleter)>;

/**
 * @brief GDBusMethodInvocation 객체에 대한 스마트 포인터 타입
 */
using GDBusMethodInvocationPtr = std::unique_ptr<GDBusMethodInvocation, decltype(&gobject_deleter)>;

/**
 * @brief GError 객체에 대한 스마트 포인터 타입
 */
using GErrorPtr = std::unique_ptr<GError, decltype(&gerror_deleter)>;

/**
 * @brief GDBusNodeInfo 객체에 대한 스마트 포인터 타입
 */
using GDBusNodeInfoPtr = std::unique_ptr<GDBusNodeInfo, decltype(&gdbusnodeinfo_deleter)>;

/**
 * @brief GVariantBuilder 객체에 대한 스마트 포인터 타입
 */
using GVariantBuilderPtr = std::unique_ptr<GVariantBuilder, decltype(&gvariantbuilder_deleter)>;

/**
 * @brief GDBusProxy 객체에 대한 스마트 포인터 타입
 */
using GDBusProxyPtr = std::unique_ptr<GDBusProxy, decltype(&gobject_deleter)>;

/**
 * @brief GCancellable 객체에 대한 스마트 포인터 타입
 */
using GCancellablePtr = std::unique_ptr<GCancellable, decltype(&gobject_deleter)>;

/**
 * @brief GDBusMessage 객체에 대한 스마트 포인터 타입
 */
using GDBusMessagePtr = std::unique_ptr<GDBusMessage, decltype(&gobject_deleter)>;

/**
 * @brief GDBusInterfaceInfo 객체에 대한 스마트 포인터 타입
 */
using GDBusInterfaceInfoPtr = std::unique_ptr<GDBusInterfaceInfo, decltype(&gdbusinterfaceinfo_deleter)>;

/**
 * @brief GMainLoop 객체에 대한 스마트 포인터 타입
 */
using GMainLoopPtr = std::unique_ptr<GMainLoop, decltype(&gmainloop_deleter)>;

/**
 * @brief GSource 객체에 대한 스마트 포인터 타입
 */
using GSourcePtr = std::unique_ptr<GSource, decltype(&gsource_deleter)>;

//
// 3. 일반적인 GLib 객체 래핑 팩토리 함수
//

/**
 * @brief GLib 객체에 대한 스마트 포인터를 생성하는 일반적인 함수 템플릿
 * 
 * @tparam T GLib 객체 타입
 * @tparam Deleter 객체 삭제자 함수 타입
 * @param p 원시(raw) 포인터
 * @param deleter 삭제자 함수
 * @return std::unique_ptr<T, Deleter> 스마트 포인터
 */
template<typename T, typename Deleter>
std::unique_ptr<T, Deleter> make_managed_ptr(T* p, Deleter deleter) {
    return std::unique_ptr<T, Deleter>(p, deleter);
}

//
// 4. 널 스마트 포인터 생성 함수
//

/**
 * @brief 널(NULL) GVariant 포인터를 생성하는 함수
 * @return GVariantPtr 널 포인터를 가리키는 스마트 포인터
 */
inline GVariantPtr makeNullGVariantPtr() {
    return GVariantPtr(nullptr, &gvariant_deleter);
}

/**
 * @brief 널(NULL) GDBusMethodInvocation 포인터를 생성하는 함수
 * @return GDBusMethodInvocationPtr 널 포인터를 가리키는 스마트 포인터
 */
inline GDBusMethodInvocationPtr makeNullGDBusMethodInvocationPtr() {
    return GDBusMethodInvocationPtr(nullptr, &gobject_deleter);
}

/**
 * @brief 널(NULL) GDBusProxy 포인터를 생성하는 함수
 * @return GDBusProxyPtr 널 포인터를 가리키는 스마트 포인터
 */
inline GDBusProxyPtr makeNullGDBusProxyPtr() {
    return GDBusProxyPtr(nullptr, &gobject_deleter);
}

/**
 * @brief 널(NULL) GDBusMessage 포인터를 생성하는 함수
 * @return GDBusMessagePtr 널 포인터를 가리키는 스마트 포인터
 */
inline GDBusMessagePtr makeNullGDBusMessagePtr() {
    return GDBusMessagePtr(nullptr, &gobject_deleter);
}

/**
 * @brief 널(NULL) GError 포인터를 생성하는 함수
 * @return GErrorPtr 널 포인터를 가리키는 스마트 포인터
 */
inline GErrorPtr makeNullGErrorPtr() {
    return GErrorPtr(nullptr, &gerror_deleter);
}

//
// 5. GLib 객체 타입별 특화된 스마트 포인터 생성 함수
//

/**
 * @brief GVariant 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트 관리를 자동으로 처리함 (floating reference를 sink함)
 * 
 * @param variant 원시 GVariant 포인터
 * @param take_ownership true인 경우 소유권을 넘겨받음 (기본값),
 *                       false인 경우 참조만 함 (참조 카운트 증가)
 * @return GVariantPtr 스마트 포인터
 */
 inline GVariantPtr makeGVariantPtr(GVariant* variant, bool take_ownership = true) {
    if (!variant) {
        return makeNullGVariantPtr();
    }
    
    if (take_ownership) {
        // 소유권을 가져갈 때는 floating reference를 sink
        return GVariantPtr(g_variant_ref_sink(variant), &g_variant_unref);
    } else {
        // 참조만 할 때는 ref만 증가
        return GVariantPtr(g_variant_ref(variant), &g_variant_unref);
    }
}

/**
 * @brief GDBusProxy 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트를 자동으로 증가시킴
 * 
 * @param proxy 원시 GDBusProxy 포인터
 * @return GDBusProxyPtr 스마트 포인터
 */
inline GDBusProxyPtr makeGDBusProxyPtr(GDBusProxy* proxy) {
    if (!proxy) {
        return makeNullGDBusProxyPtr();
    }
    return GDBusProxyPtr(g_object_ref(proxy), &gobject_deleter);
}

/**
 * @brief GDBusMethodInvocation 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트를 자동으로 증가시킴
 * 
 * @param invocation 원시 GDBusMethodInvocation 포인터
 * @return GDBusMethodInvocationPtr 스마트 포인터
 */
inline GDBusMethodInvocationPtr makeGDBusMethodInvocationPtr(GDBusMethodInvocation* invocation) {
    if (!invocation) {
        return makeNullGDBusMethodInvocationPtr();
    }
    return GDBusMethodInvocationPtr(g_object_ref(invocation), &gobject_deleter);
}

/**
 * @brief GDBusMessage 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트를 자동으로 증가시킴
 * 
 * @param message 원시 GDBusMessage 포인터
 * @return GDBusMessagePtr 스마트 포인터
 */
inline GDBusMessagePtr makeGDBusMessagePtr(GDBusMessage* message) {
    if (!message) {
        return makeNullGDBusMessagePtr();
    }
    return GDBusMessagePtr(g_object_ref(message), &gobject_deleter);
}

/**
 * @brief GError 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 
 * @param error 원시 GError 포인터
 * @return GErrorPtr 스마트 포인터
 */
inline GErrorPtr makeGErrorPtr(GError* error) {
    return GErrorPtr(error, &gerror_deleter);
}

/**
 * @brief GDBusNodeInfo 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 
 * @param info 원시 GDBusNodeInfo 포인터
 * @return GDBusNodeInfoPtr 스마트 포인터
 */
inline GDBusNodeInfoPtr makeGDBusNodeInfoPtr(GDBusNodeInfo* info) {
    if (!info) {
        return GDBusNodeInfoPtr(nullptr, &gdbusnodeinfo_deleter);
    }
    return GDBusNodeInfoPtr(info, &gdbusnodeinfo_deleter);
}

/**
 * @brief GVariantBuilder 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 
 * @param builder 원시 GVariantBuilder 포인터
 * @return GVariantBuilderPtr 스마트 포인터
 */
inline GVariantBuilderPtr makeGVariantBuilderPtr(GVariantBuilder* builder) {
    if (!builder) {
        return GVariantBuilderPtr(nullptr, &gvariantbuilder_deleter);
    }
    return GVariantBuilderPtr(builder, &gvariantbuilder_deleter);
}

/**
 * @brief GDBusConnection 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트를 자동으로 증가시킴
 * 
 * @param connection 원시 GDBusConnection 포인터
 * @return GDBusConnectionPtr 스마트 포인터
 */
inline GDBusConnectionPtr makeGDBusConnectionPtr(GDBusConnection* connection) {
    if (!connection) {
        return GDBusConnectionPtr(nullptr, &gobject_deleter);
    }
    return GDBusConnectionPtr(g_object_ref(connection), &gobject_deleter);
}

/**
 * @brief GCancellable 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 참조 카운트를 자동으로 증가시킴
 * 
 * @param cancellable 원시 GCancellable 포인터
 * @return GCancellablePtr 스마트 포인터
 */
inline GCancellablePtr makeGCancellablePtr(GCancellable* cancellable) {
    if (!cancellable) {
        return GCancellablePtr(nullptr, &gobject_deleter);
    }
    return GCancellablePtr(g_object_ref(cancellable), &gobject_deleter);
}

/**
 * @brief GMainLoop 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 
 * @param loop 원시 GMainLoop 포인터
 * @return GMainLoopPtr 스마트 포인터
 */
inline GMainLoopPtr makeGMainLoopPtr(GMainLoop* loop) {
    if (!loop) {
        return GMainLoopPtr(nullptr, &gmainloop_deleter);
    }
    return GMainLoopPtr(loop, &gmainloop_deleter);
}

/**
 * @brief GSource 객체를 래핑하는 스마트 포인터를 생성하는 함수
 * 
 * @param source 원시 GSource 포인터
 * @return GSourcePtr 스마트 포인터
 */
inline GSourcePtr makeGSourcePtr(GSource* source) {
    if (!source) {
        return GSourcePtr(nullptr, &gsource_deleter);
    }
    return GSourcePtr(source, &gsource_deleter);
}

//
// 6. D-Bus 관련 타입 정의
//

/**
 * @brief D-Bus 인자 정의 구조체
 */
struct DBusArgument {
    std::string signature;   // D-Bus 타입 시그니처
    std::string name;        // 인자 이름
    std::string direction;   // "in" 또는 "out"
    std::string description; // 인자 설명
    
    // 기본 생성자
    DBusArgument() = default;
    
    // 생성자 - 모든 필드 초기화
    DBusArgument(
        std::string sig,
        std::string n,
        std::string dir = "",
        std::string desc = ""
    ) : signature(std::move(sig)),
        name(std::move(n)),
        direction(std::move(dir)),
        description(std::move(desc)) {}
};

/**
 * @brief D-Bus 메시지 타입 열거형
 */
enum class DBusMessageType {
    METHOD_CALL,
    METHOD_RETURN,
    ERROR,
    SIGNAL
};

/**
 * @brief D-Bus 기본 인터페이스 상수 구조체
 */
struct DBusInterface {
    static constexpr const char* PROPERTIES = "org.freedesktop.DBus.Properties";
    static constexpr const char* INTROSPECTABLE = "org.freedesktop.DBus.Introspectable";
    static constexpr const char* OBJECTMANAGER = "org.freedesktop.DBus.ObjectManager";
};

/**
 * @brief D-Bus 속성 구조체
 */
struct DBusProperty {
    std::string name;                     // 속성 이름
    std::string signature;                // 타입 시그니처
    bool readable;                        // 읽기 가능 여부
    bool writable;                        // 쓰기 가능 여부
    bool emitsChangedSignal;              // 값 변경 시 시그널 발생 여부
    std::function<GVariant*()> getter;    // getter 콜백
    std::function<bool(GVariant*)> setter;// setter 콜백
    
    // 기본 생성자
    DBusProperty() = default;
    
    // 생성자 - 모든 필드 초기화
    DBusProperty(
        std::string n,
        std::string sig,
        bool read,
        bool write,
        bool emits,
        std::function<GVariant*()> get = nullptr,
        std::function<bool(GVariant*)> set = nullptr
    ) : name(std::move(n)),
        signature(std::move(sig)),
        readable(read),
        writable(write),
        emitsChangedSignal(emits),
        getter(std::move(get)),
        setter(std::move(set)) {}
};

/**
 * @brief D-Bus 메서드 호출 구조체
 */
struct DBusMethodCall {
    std::string sender;                     // 발신자
    std::string interface;                  // 인터페이스 이름
    std::string method;                     // 메서드 이름
    GVariantPtr parameters;                 // 매개변수
    GDBusMethodInvocationPtr invocation;    // 메서드 호출 객체
    
    // 기본 생성자
    DBusMethodCall() : 
        parameters(makeNullGVariantPtr()),
        invocation(makeNullGDBusMethodInvocationPtr()) {}
    
    // 복사 생성자/할당 연산자 삭제 (복사 불가능)
    DBusMethodCall(const DBusMethodCall&) = delete;
    DBusMethodCall& operator=(const DBusMethodCall&) = delete;
    
    // 이동 생성자/할당 연산자
    DBusMethodCall(DBusMethodCall&&) noexcept = default;
    DBusMethodCall& operator=(DBusMethodCall&&) noexcept = default;
    
    // 전체 필드 초기화 생성자
    DBusMethodCall(
        std::string s,
        std::string i,
        std::string m,
        GVariantPtr p,
        GDBusMethodInvocationPtr inv
    ) : sender(std::move(s)), 
        interface(std::move(i)), 
        method(std::move(m)),
        parameters(std::move(p)), 
        invocation(std::move(inv)) {}
};

/**
 * @brief D-Bus 시그널 구조체
 */
struct DBusSignal {
    std::string name;                        // 시그널 이름
    std::vector<DBusArgument> arguments;     // 인자 목록
    
    // 기본 생성자
    DBusSignal() = default;
    
    // 생성자 - 모든 필드 초기화
    DBusSignal(std::string n, std::vector<DBusArgument> args = {})
        : name(std::move(n)), arguments(std::move(args)) {}
};

/**
 * @brief D-Bus 인트로스펙션 구조체
 */
struct DBusIntrospection {
    bool includeStandardInterfaces = true;      // 표준 인터페이스 포함 여부
    std::map<std::string, std::string> annotations; // 주석
    
    // 기본 생성자
    DBusIntrospection() = default;
    
    // 생성자 - 필드 초기화
    explicit DBusIntrospection(bool includeStd, 
                               std::map<std::string, std::string> annots = {})
        : includeStandardInterfaces(includeStd), 
          annotations(std::move(annots)) {}
};

} // namespace ggk