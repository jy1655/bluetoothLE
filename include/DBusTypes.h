#pragma once

#include <gio/gio.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <type_traits>

namespace ggk {

/**
 * @brief D-Bus type definitions and smart pointers for GLib/GObject resources
 * 
 * This file provides RAII wrappers around GLib/GObject resources to ensure
 * proper memory management and prevent leaks. It defines custom deleters and
 * smart pointer types for various D-Bus related GLib objects.
 */

//
// 1. Resource deleters for use with smart pointers
//

/**
 * @brief Custom deleter for GVariant
 */
struct GVariantDeleter {
    void operator()(GVariant* ptr) const {
        if (ptr) g_variant_unref(ptr);
    }
};

/**
 * @brief Custom deleter for GObject-derived types
 */
struct GObjectDeleter {
    void operator()(void* ptr) const {
        if (ptr) g_object_unref(G_OBJECT(ptr));
    }
};

/**
 * @brief Custom deleter for GError
 */
struct GErrorDeleter {
    void operator()(GError* ptr) const {
        if (ptr) g_error_free(ptr);
    }
};

/**
 * @brief Custom deleter for GDBusNodeInfo
 */
struct GDBusNodeInfoDeleter {
    void operator()(GDBusNodeInfo* ptr) const {
        if (ptr) g_dbus_node_info_unref(ptr);
    }
};

/**
 * @brief Custom deleter for GVariantBuilder
 */
struct GVariantBuilderDeleter {
    void operator()(GVariantBuilder* ptr) const {
        if (ptr) g_variant_builder_unref(ptr);
    }
};

/**
 * @brief Custom deleter for GDBusInterfaceInfo
 */
struct GDBusInterfaceInfoDeleter {
    void operator()(GDBusInterfaceInfo* ptr) const {
        if (ptr) g_dbus_interface_info_unref(ptr);
    }
};

/**
 * @brief Custom deleter for GMainLoop
 */
struct GMainLoopDeleter {
    void operator()(GMainLoop* ptr) const {
        if (ptr) g_main_loop_unref(ptr);
    }
};

/**
 * @brief Custom deleter for GSource
 */
struct GSourceDeleter {
    void operator()(GSource* ptr) const {
        if (ptr) g_source_unref(ptr);
    }
};

//
// 2. Smart pointer type definitions
//

/**
 * @brief Smart pointer for GVariant
 */
using GVariantPtr = std::unique_ptr<GVariant, GVariantDeleter>;

/**
 * @brief Smart pointer for GDBusConnection
 */
using GDBusConnectionPtr = std::unique_ptr<GDBusConnection, GObjectDeleter>;

/**
 * @brief Smart pointer for GDBusMethodInvocation
 */
using GDBusMethodInvocationPtr = std::unique_ptr<GDBusMethodInvocation, GObjectDeleter>;

/**
 * @brief Smart pointer for GError
 */
using GErrorPtr = std::unique_ptr<GError, GErrorDeleter>;

/**
 * @brief Smart pointer for GDBusNodeInfo
 */
using GDBusNodeInfoPtr = std::unique_ptr<GDBusNodeInfo, GDBusNodeInfoDeleter>;

/**
 * @brief Smart pointer for GVariantBuilder
 */
using GVariantBuilderPtr = std::unique_ptr<GVariantBuilder, GVariantBuilderDeleter>;

/**
 * @brief Smart pointer for GDBusProxy
 */
using GDBusProxyPtr = std::unique_ptr<GDBusProxy, GObjectDeleter>;

/**
 * @brief Smart pointer for GCancellable
 */
using GCancellablePtr = std::unique_ptr<GCancellable, GObjectDeleter>;

/**
 * @brief Smart pointer for GDBusMessage
 */
using GDBusMessagePtr = std::unique_ptr<GDBusMessage, GObjectDeleter>;

/**
 * @brief Smart pointer for GDBusInterfaceInfo
 */
using GDBusInterfaceInfoPtr = std::unique_ptr<GDBusInterfaceInfo, GDBusInterfaceInfoDeleter>;

/**
 * @brief Smart pointer for GMainLoop
 */
using GMainLoopPtr = std::unique_ptr<GMainLoop, GMainLoopDeleter>;

/**
 * @brief Smart pointer for GSource
 */
using GSourcePtr = std::unique_ptr<GSource, GSourceDeleter>;

//
// 3. Factory functions for creating smart pointers
//

/**
 * @brief Create a null GVariantPtr
 * @return Empty GVariantPtr
 */
inline GVariantPtr makeNullGVariantPtr() {
    return GVariantPtr(nullptr);
}

/**
 * @brief Create a null GDBusMethodInvocationPtr
 * @return Empty GDBusMethodInvocationPtr
 */
inline GDBusMethodInvocationPtr makeNullGDBusMethodInvocationPtr() {
    return GDBusMethodInvocationPtr(nullptr);
}

/**
 * @brief Create a null GDBusProxyPtr
 * @return Empty GDBusProxyPtr
 */
inline GDBusProxyPtr makeNullGDBusProxyPtr() {
    return GDBusProxyPtr(nullptr);
}

/**
 * @brief Create a null GDBusMessagePtr
 * @return Empty GDBusMessagePtr
 */
inline GDBusMessagePtr makeNullGDBusMessagePtr() {
    return GDBusMessagePtr(nullptr);
}

/**
 * @brief Create a null GErrorPtr
 * @return Empty GErrorPtr
 */
inline GErrorPtr makeNullGErrorPtr() {
    return GErrorPtr(nullptr);
}

/**
 * @brief Create a GVariantPtr from a GVariant
 * 
 * Properly handles floating references by sinking them.
 * 
 * @param variant Raw GVariant pointer
 * @param take_ownership Whether to take ownership of the reference (default: true)
 * @return Smart pointer managing the GVariant
 */
inline GVariantPtr makeGVariantPtr(GVariant* variant, bool take_ownership = true) {
    if (!variant) {
        return makeNullGVariantPtr();
    }
    
    if (take_ownership) {
        // If variant has a floating reference, sink it to ensure it's owned
        return GVariantPtr(g_variant_ref_sink(variant));
    } else {
        // Just add a reference without sinking
        return GVariantPtr(g_variant_ref(variant));
    }
}

/**
 * @brief Create a GDBusConnectionPtr from a GDBusConnection
 * 
 * @param connection Raw GDBusConnection pointer
 * @return Smart pointer managing the GDBusConnection
 */
inline GDBusConnectionPtr makeGDBusConnectionPtr(GDBusConnection* connection) {
    if (!connection) {
        return GDBusConnectionPtr(nullptr);
    }
    // Take a new reference
    return GDBusConnectionPtr(static_cast<GDBusConnection*>(g_object_ref(connection)));
}

/**
 * @brief Create a GDBusMethodInvocationPtr from a GDBusMethodInvocation
 * 
 * @param invocation Raw GDBusMethodInvocation pointer
 * @return Smart pointer managing the GDBusMethodInvocation
 */
inline GDBusMethodInvocationPtr makeGDBusMethodInvocationPtr(GDBusMethodInvocation* invocation) {
    if (!invocation) {
        return makeNullGDBusMethodInvocationPtr();
    }
    // Take a new reference
    return GDBusMethodInvocationPtr(static_cast<GDBusMethodInvocation*>(g_object_ref(invocation)));
}

/**
 * @brief Create a GDBusMessagePtr from a GDBusMessage
 * 
 * @param message Raw GDBusMessage pointer
 * @return Smart pointer managing the GDBusMessage
 */
inline GDBusMessagePtr makeGDBusMessagePtr(GDBusMessage* message) {
    if (!message) {
        return makeNullGDBusMessagePtr();
    }
    // Take a new reference
    return GDBusMessagePtr(static_cast<GDBusMessage*>(g_object_ref(message)));
}

/**
 * @brief Create a GErrorPtr from a GError
 * 
 * @param error Raw GError pointer
 * @return Smart pointer managing the GError
 */
inline GErrorPtr makeGErrorPtr(GError* error) {
    return GErrorPtr(error);
}

/**
 * @brief Create a GDBusNodeInfoPtr from a GDBusNodeInfo
 * 
 * @param info Raw GDBusNodeInfo pointer
 * @return Smart pointer managing the GDBusNodeInfo
 */
inline GDBusNodeInfoPtr makeGDBusNodeInfoPtr(GDBusNodeInfo* info) {
    if (!info) {
        return GDBusNodeInfoPtr(nullptr);
    }
    return GDBusNodeInfoPtr(info);
}

/**
 * @brief Create a GVariantBuilderPtr from a GVariantBuilder
 * 
 * @param builder Raw GVariantBuilder pointer
 * @return Smart pointer managing the GVariantBuilder
 */
inline GVariantBuilderPtr makeGVariantBuilderPtr(GVariantBuilder* builder) {
    if (!builder) {
        return GVariantBuilderPtr(nullptr);
    }
    return GVariantBuilderPtr(builder);
}

/**
 * @brief Create a GCancellablePtr from a GCancellable
 * 
 * @param cancellable Raw GCancellable pointer
 * @return Smart pointer managing the GCancellable
 */
inline GCancellablePtr makeGCancellablePtr(GCancellable* cancellable) {
    if (!cancellable) {
        return GCancellablePtr(nullptr);
    }
    // Take a new reference
    return GCancellablePtr(static_cast<GCancellable*>(g_object_ref(cancellable)));
}

/**
 * @brief Create a GMainLoopPtr from a GMainLoop
 * 
 * @param loop Raw GMainLoop pointer
 * @return Smart pointer managing the GMainLoop
 */
inline GMainLoopPtr makeGMainLoopPtr(GMainLoop* loop) {
    if (!loop) {
        return GMainLoopPtr(nullptr);
    }
    return GMainLoopPtr(loop);
}

/**
 * @brief Create a GSourcePtr from a GSource
 * 
 * @param source Raw GSource pointer
 * @return Smart pointer managing the GSource
 */
inline GSourcePtr makeGSourcePtr(GSource* source) {
    if (!source) {
        return GSourcePtr(nullptr);
    }
    return GSourcePtr(source);
}

//
// 4. D-Bus type definitions
//

/**
 * @brief D-Bus message type enumeration
 */
enum class DBusMessageType {
    METHOD_CALL,    ///< Method call message
    METHOD_RETURN,  ///< Method return message
    ERROR,          ///< Error message
    SIGNAL          ///< Signal message
};

/**
 * @brief D-Bus argument definition structure
 */
struct DBusArgument {
    std::string signature;   ///< D-Bus type signature
    std::string name;        ///< Argument name
    std::string direction;   ///< Direction ("in" or "out")
    std::string description; ///< Argument description
    
    DBusArgument() = default;
    
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
 * @brief D-Bus property definition structure
 */
struct DBusProperty {
    std::string name;                     ///< Property name
    std::string signature;                ///< Type signature
    bool readable;                        ///< Whether property is readable
    bool writable;                        ///< Whether property is writable
    bool emitsChangedSignal;              ///< Whether property emits changed signal
    std::function<GVariant*()> getter;    ///< Property getter callback
    std::function<bool(GVariant*)> setter;///< Property setter callback
    
    DBusProperty() = default;
    
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
 * @brief D-Bus method call information structure
 */
struct DBusMethodCall {
    std::string sender;                     ///< Method call sender
    std::string interface;                  ///< Interface name
    std::string method;                     ///< Method name
    GVariantPtr parameters;                 ///< Method parameters
    GDBusMethodInvocationPtr invocation;    ///< Method invocation object
    
    DBusMethodCall() : 
        parameters(makeNullGVariantPtr()),
        invocation(makeNullGDBusMethodInvocationPtr()) {}
    
    // Disable copy operations
    DBusMethodCall(const DBusMethodCall&) = delete;
    DBusMethodCall& operator=(const DBusMethodCall&) = delete;
    
    // Enable move operations
    DBusMethodCall(DBusMethodCall&&) noexcept = default;
    DBusMethodCall& operator=(DBusMethodCall&&) noexcept = default;
    
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
 * @brief D-Bus signal definition structure
 */
struct DBusSignal {
    std::string name;                        ///< Signal name
    std::vector<DBusArgument> arguments;     ///< Signal arguments
    
    DBusSignal() = default;
    
    DBusSignal(std::string n, std::vector<DBusArgument> args = {})
        : name(std::move(n)), arguments(std::move(args)) {}
};

/**
 * @brief D-Bus interface constants
 */
struct DBusInterface {
    static constexpr const char* PROPERTIES = "org.freedesktop.DBus.Properties";
    static constexpr const char* INTROSPECTABLE = "org.freedesktop.DBus.Introspectable";
    static constexpr const char* OBJECTMANAGER = "org.freedesktop.DBus.ObjectManager";
};

/**
 * @brief D-Bus introspection options structure
 */
struct DBusIntrospection {
    bool includeStandardInterfaces = true;      ///< Whether to include standard interfaces
    std::map<std::string, std::string> annotations; ///< Additional annotations
    
    DBusIntrospection() = default;
    
    explicit DBusIntrospection(bool includeStd, 
                               std::map<std::string, std::string> annots = {})
        : includeStandardInterfaces(includeStd), 
          annotations(std::move(annots)) {}
};

} // namespace ggk