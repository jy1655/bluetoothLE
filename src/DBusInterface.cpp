// DBusInterface.cpp
#include "DBusInterface.h"
#include "Logger.h"

namespace ggk {

DBusInterface::DBusInterface(const std::string& name)
    : name(name) {
    if (name.empty()) {
        throw std::invalid_argument("Interface name cannot be empty");
    }
}

// Method Management
void DBusInterface::addMethod(std::shared_ptr<DBusMethod> method) {
    if (!method) {
        throw std::invalid_argument("Cannot add null method to interface: " + name);
    }

    const auto& methodName = method->getName();
    if (methods.find(methodName) != methods.end()) {
        throw std::runtime_error("Method already exists: " + methodName);
    }

    methods[methodName] = method;
    Logger::debug("Added method '" + methodName + "' to interface '" + name + "'");
}

void DBusInterface::removeMethod(const std::string& methodName) {
    auto it = methods.find(methodName);
    if (it != methods.end()) {
        methods.erase(it);
        Logger::debug("Removed method '" + methodName + "' from interface '" + name + "'");
    }
}

std::shared_ptr<DBusMethod> DBusInterface::findMethod(const std::string& methodName) const {
    auto it = methods.find(methodName);
    return (it != methods.end()) ? it->second : nullptr;
}

bool DBusInterface::invokeMethod(const DBusMethodCall& call) const {
    auto method = findMethod(call.method);
    if (!method) {
        handleError("UnknownMethod", "Method not found: " + call.method);
        return false;
    }

    if (!validatePropertyAccess(call.method, call.sender, true)) {
        handleError("AccessDenied", "Access denied to method: " + call.method);
        return false;
    }

    try {
        method->invoke(call);
        onMethodCalled(call);
        return true;
    } catch (const std::exception& e) {
        handleError("Failed", std::string("Method invocation failed: ") + e.what());
        return false;
    }
}

// Signal Management
void DBusInterface::addSignal(const DBusSignal& signal) {
    if (signal.name.empty()) {
        throw std::invalid_argument("Signal name cannot be empty");
    }

    if (signals.find(signal.name) != signals.end()) {
        throw std::runtime_error("Signal already exists: " + signal.name);
    }

    signals[signal.name] = signal;
    Logger::debug("Added signal '" + signal.name + "' to interface '" + name + "'");
}

void DBusInterface::removeSignal(const std::string& signalName) {
    auto it = signals.find(signalName);
    if (it != signals.end()) {
        signals.erase(it);
        Logger::debug("Removed signal '" + signalName + "' from interface '" + name + "'");
    }
}

void DBusInterface::emitSignal(const DBusSignalEmission& emission) const {
    if (!emission.connection) {
        throw std::invalid_argument("No D-Bus connection for signal emission");
    }

    auto signalIt = signals.find(emission.interface);
    if (signalIt == signals.end()) {
        throw std::runtime_error("Signal not found: " + emission.interface);
    }

    GError* error = nullptr;
    g_dbus_connection_emit_signal(emission.connection,
                                emission.destination.c_str(),
                                getPath().c_str(),
                                name.c_str(),
                                emission.interface.c_str(),
                                emission.parameters,
                                &error);

    if (error) {
        std::string errorMsg = error->message;
        g_error_free(error);
        throw std::runtime_error("Failed to emit signal: " + errorMsg);
    }

    onSignalEmitted(emission);
}

// Property Management
void DBusInterface::addProperty(const DBusProperty& property) {
    if (property.name.empty()) {
        throw std::invalid_argument("Property name cannot be empty");
    }

    if (properties.find(property.name) != properties.end()) {
        throw std::runtime_error("Property already exists: " + property.name);
    }

    if (!property.readable && !property.writable) {
        throw std::invalid_argument("Property must be either readable or writable");
    }

    properties[property.name] = property;
    Logger::debug("Added property '" + property.name + "' to interface '" + name + "'");
}

void DBusInterface::removeProperty(const std::string& propertyName) {
    auto it = properties.find(propertyName);
    if (it != properties.end()) {
        properties.erase(it);
        Logger::debug("Removed property '" + propertyName + "' from interface '" + name + "'");
    }
}

GVariant* DBusInterface::getProperty(const std::string& propertyName) const {
    auto it = properties.find(propertyName);
    if (it == properties.end()) {
        throw std::runtime_error("Property not found: " + propertyName);
    }

    if (!it->second.readable || !it->second.getter) {
        throw std::runtime_error("Property is not readable: " + propertyName);
    }

    return it->second.getter();
}

bool DBusInterface::setProperty(const std::string& propertyName,
                              GVariant* value,
                              const std::string& sender) {
    auto it = properties.find(propertyName);
    if (it == properties.end()) {
        handleError("UnknownProperty", "Property not found: " + propertyName);
        return false;
    }

    if (!it->second.writable || !it->second.setter) {
        handleError("ReadOnlyProperty", "Property is not writable: " + propertyName);
        return false;
    }

    if (!validatePropertyAccess(propertyName, sender, true)) {
        handleError("AccessDenied", "Access denied to property: " + propertyName);
        return false;
    }

    try {
        GVariant* oldValue = it->second.getter ? it->second.getter() : nullptr;
        it->second.setter(value);
        notifyPropertyChanged(propertyName, oldValue, value);
        return true;
    } catch (const std::exception& e) {
        handleError("Failed", std::string("Failed to set property: ") + e.what());
        return false;
    }
}

// Protected Methods
void DBusInterface::onPropertyChanged(const std::string& propertyName,
                                    GVariant* oldValue,
                                    GVariant* newValue) const {
    Logger::debug("Property '" + propertyName + "' changed on interface '" + name + "'");
}

void DBusInterface::onMethodCalled(const DBusMethodCall& call) const {
    Logger::debug("Method '" + call.method + "' called on interface '" + name + "'");
}

void DBusInterface::onSignalEmitted(const DBusSignalEmission& emission) const {
    Logger::debug("Signal '" + emission.interface + "' emitted on interface '" + name + "'");
}

// Private Methods
void DBusInterface::notifyPropertyChanged(const std::string& propertyName,
                                        GVariant* oldValue,
                                        GVariant* newValue) const {
    auto it = properties.find(propertyName);
    if (it != properties.end() && it->second.emitsChangedSignal) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&builder, "{sv}", propertyName.c_str(), newValue);

        GVariant* params = g_variant_new("(sa{sv}as)",
                                       name.c_str(),
                                       &builder,
                                       nullptr);

        DBusSignalEmission emission{
            nullptr,  // connection will be set by caller
            "",      // broadcast
            G_DBUS_SIGNAL_FLAGS_NONE,
            params,
            "org.freedesktop.DBus.Properties"
        };

        try {
            emitSignal(emission);
            onPropertyChanged(propertyName, oldValue, newValue);
        } catch (const std::exception& e) {
            Logger::error("Failed to notify property change: " + std::string(e.what()));
        }
    }
}

bool DBusInterface::validatePropertyAccess(const std::string& propertyName,
                                         const std::string& sender,
                                         bool isWrite) const {
    auto it = properties.find(propertyName);
    if (it == properties.end()) {
        return false;
    }

    if (it->second.accessControl) {
        return it->second.accessControl(sender);
    }

    return true;
}

void DBusInterface::handleError(const std::string& errorName,
                              const std::string& errorMessage) const {
    std::string fullErrorName = "org.freedesktop.DBus.Error." + errorName;
    Logger::error(fullErrorName + ": " + errorMessage);
}

// XML Generation

std::string DBusInterface::escapeXml(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length());
    
    for (char c : str) {
        switch (c) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&apos;";
                break;
            default:
                escaped += c;
                break;
        }
    }
    
    return escaped;
}

std::string DBusInterface::generateIntrospectionXML(
    const DBusIntrospection& config) const {
    std::string xml;
    
    // 인터페이스 시작
    xml += "<interface name=\"" + name + "\">\n";

    // 기본 구성요소들 XML 생성
    xml += generateMethodsXML(config);
    xml += generateSignalsXML(config);
    xml += generatePropertiesXML(config);

    // 애노테이션 추가
    if (!config.annotations.empty()) {
        for (const auto& [key, value] : config.annotations) {
            xml += "  <annotation name=\"" + escapeXml(key) + 
                   "\" value=\"" + escapeXml(value) + "\"/>\n";
        }
    }

    // 인터페이스 설명 추가
    if (!config.description.empty()) {
        xml += "  <annotation name=\"org.freedesktop.DBus.Description\"" 
               " value=\"" + escapeXml(config.description) + "\"/>\n";
    }

    // 버전 정보 추가
    if (!config.version.empty()) {
        xml += "  <annotation name=\"org.freedesktop.DBus.Version\"" 
               " value=\"" + escapeXml(config.version) + "\"/>\n";
    }

    xml += "</interface>\n";
    return xml;
}

std::string DBusInterface::generateMethodsXML(
    const DBusIntrospection& config) const {
    std::string xml;
    
    for (const auto& [methodName, method] : methods) {
        // 각 메서드의 XML 생성을 위임
        xml += method->generateIntrospectionXML(config);
    }
    
    return xml;
}

std::string DBusInterface::generateSignalsXML(
    const DBusIntrospection& config) const {
    std::string xml;
    
    for (const auto& [signalName, signal] : signals) {
        xml += "  <signal name=\"" + escapeXml(signal.name) + "\">\n";
        
        for (const auto& arg : signal.arguments) {
            xml += "    <arg";
            if (!arg.name.empty()) {
                xml += " name=\"" + escapeXml(arg.name) + "\"";
            }
            xml += " type=\"" + escapeXml(arg.signature) + "\"";
            
            if (!arg.description.empty()) {
                xml += ">\n";
                xml += "      <annotation name=\"org.freedesktop.DBus.Description\"" 
                       " value=\"" + escapeXml(arg.description) + "\"/>\n";
                xml += "    </arg>\n";
            } else {
                xml += "/>\n";
            }
        }
        
        if (!signal.description.empty()) {
            xml += "    <annotation name=\"org.freedesktop.DBus.Description\"" 
                   " value=\"" + escapeXml(signal.description) + "\"/>\n";
        }
        
        xml += "  </signal>\n";
    }
    
    return xml;
}

std::string DBusInterface::generatePropertiesXML(
    const DBusIntrospection& config) const {
    std::string xml;
    
    for (const auto& [propertyName, property] : properties) {
        xml += "  <property name=\"" + escapeXml(property.name) + 
               "\" type=\"" + escapeXml(property.signature) + "\"";
        
        std::string access;
        if (property.readable && property.writable) {
            access = "readwrite";
        } else if (property.readable) {
            access = "read";
        } else if (property.writable) {
            access = "write";
        }
        xml += " access=\"" + access + "\"";
        
        bool hasAnnotations = !property.description.empty() || 
                            property.emitsChangedSignal;
        
        if (hasAnnotations) {
            xml += ">\n";
            
            if (!property.description.empty()) {
                xml += "    <annotation name=\"org.freedesktop.DBus.Description\"" 
                       " value=\"" + escapeXml(property.description) + "\"/>\n";
            }
            
            if (property.emitsChangedSignal) {
                xml += "    <annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\"" 
                       " value=\"true\"/>\n";
            }
            
            xml += "  </property>\n";
        } else {
            xml += "/>\n";
        }
    }
    
    return xml;
}

} // namespace ggk