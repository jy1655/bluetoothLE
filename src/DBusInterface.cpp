#include "DBusInterface.h"
#include "Logger.h"

namespace ggk {

DBusInterface::DBusInterface(const std::string& name)
    : name(name) {
}

void DBusInterface::addMethod(const std::shared_ptr<DBusMethod>& method) {
    if (!method) {
        Logger::error("Attempted to add null method to interface: " + name);
        return;
    }
    methods.push_back(method);
}

bool DBusInterface::callMethod(const std::string& methodName,
                             GDBusConnection* pConnection,
                             GVariant* pParameters,
                             GDBusMethodInvocation* pInvocation,
                             gpointer pUserData) const {
    for (const auto& method : methods) {
        if (method->getName() == methodName) {
            method->call<DBusInterface>(pConnection,
                                      DBusObjectPath(),
                                      name,
                                      methodName,
                                      pParameters,
                                      pInvocation,
                                      pUserData);
            return true;
        }
    }
    return false;
}

void DBusInterface::addSignal(const std::string& name,
                            const std::vector<std::string>& arguments) {
    signals.push_back({name, arguments});
}

void DBusInterface::emitSignal(GDBusConnection* pConnection,
                             const DBusObjectPath& path,
                             const std::string& signalName,
                             GVariant* pParameters) const {
    if (!pConnection) {
        Logger::error("No D-Bus connection for signal emission");
        return;
    }

    GError* pError = nullptr;
    g_dbus_connection_emit_signal(pConnection,
                                nullptr,
                                path.c_str(),
                                name.c_str(),
                                signalName.c_str(),
                                pParameters,
                                &pError);

    if (pError) {
        Logger::error("Failed to emit signal: " + std::string(pError->message));
        g_error_free(pError);
    }
}

void DBusInterface::addProperty(const std::string& name,
                              const std::string& type,
                              bool readable,
                              bool writable,
                              GVariant* (*getter)(void),
                              void (*setter)(GVariant*)) {
    properties.push_back({name, type, readable, writable, getter, setter});
}

std::string DBusInterface::generateIntrospectionXML(int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    // 인터페이스 시작
    xml += indent + "<interface name=\"" + name + "\">\n";

    // 메서드 XML 생성
    for (const auto& method : methods) {
        xml += generateMethodXML(*method, depth + 1);
    }

    // 시그널 XML 생성
    for (const auto& signal : signals) {
        xml += generateSignalXML(signal, depth + 1);
    }

    // 프로퍼티 XML 생성
    for (const auto& property : properties) {
        xml += generatePropertyXML(property, depth + 1);
    }

    // 인터페이스 종료
    xml += indent + "</interface>\n";

    return xml;
}

std::string DBusInterface::generateMethodXML(const DBusMethod& method, int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    xml += indent + "<method name=\"" + method.getName() + "\">\n";

    // 입력 인자
    for (const auto& arg : method.getInArgs()) {
        xml += indent + "  <arg type=\"" + arg + "\" direction=\"in\"/>\n";
    }

    // 출력 인자
    if (!method.getOutArgs().empty()) {
        xml += indent + "  <arg type=\"" + method.getOutArgs() + "\" direction=\"out\"/>\n";
    }

    xml += indent + "</method>\n";
    return xml;
}

std::string DBusInterface::generateSignalXML(const Signal& signal, int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    xml += indent + "<signal name=\"" + signal.name + "\">\n";
    
    for (const auto& arg : signal.arguments) {
        xml += indent + "  <arg type=\"" + arg + "\"/>\n";
    }

    xml += indent + "</signal>\n";
    return xml;
}

std::string DBusInterface::generatePropertyXML(const Property& property, int depth) const {
    std::string indent(depth * 2, ' ');
    std::string access;

    if (property.readable && property.writable) {
        access = "readwrite";
    } else if (property.readable) {
        access = "read";
    } else if (property.writable) {
        access = "write";
    }

    return indent + "<property name=\"" + property.name +
           "\" type=\"" + property.type +
           "\" access=\"" + access + "\"/>\n";
}

} // namespace ggk