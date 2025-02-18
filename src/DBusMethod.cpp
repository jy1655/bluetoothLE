#include "DBusMethod.h"
#include "DBusInterface.h"

namespace ggk {

DBusMethod::DBusMethod(const DBusInterface* pOwner,
                      const std::string& name,
                      const char* pInArgs[],
                      const char* pOutArgs,
                      Callback callback)
    : pOwner(pOwner)
    , name(name)
    , outArgs(pOutArgs ? pOutArgs : "")
    , callback(callback) {
    
    if (pInArgs) {
        for (int i = 0; pInArgs[i] != nullptr; ++i) {
            inArgs.push_back(pInArgs[i]);
        }
    }
}

void DBusMethod::callAsync(GDBusConnection* pConnection,
                          const DBusObjectPath& path,
                          const std::string& interfaceName,
                          GVariant* pParameters,
                          AsyncCallback callback,
                          gpointer user_data) const {
    g_dbus_connection_call(
        pConnection,
        "org.bluez",
        path.c_str(),
        interfaceName.c_str(),
        name.c_str(),
        pParameters,
        nullptr,  // reply type
        G_DBUS_CALL_FLAGS_NONE,
        -1,       // timeout
        nullptr,  // cancellable
        reinterpret_cast<GAsyncReadyCallback>(callback),
        user_data);
}

std::string DBusMethod::generateIntrospectionXML(int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    xml += indent + "<method name=\"" + name + "\">\n";

    // 입력 파라미터
    for (const auto& arg : inArgs) {
        xml += indent + "  <arg type=\"" + arg + "\" direction=\"in\"/>\n";
    }

    // 출력 파라미터
    if (!outArgs.empty()) {
        xml += indent + "  <arg type=\"" + outArgs + "\" direction=\"out\"/>\n";
    }

    xml += indent + "</method>\n";
    return xml;
}

void DBusMethod::logMethodCall(const std::string& methodName,
                             GVariant* parameters) const {
    std::string paramStr;
    if (parameters) {
        gchar* params = g_variant_print(parameters, TRUE);
        if (params) {
            paramStr = params;
            g_free(params);
        }
    }

    Logger::debug("Method call: " + methodName + 
                 (paramStr.empty() ? "" : " with parameters: " + paramStr));
}

} // namespace ggk