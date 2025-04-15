#include "DBusObject.h"
#include "DBusXml.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

DBusObject::DBusObject(DBusConnection& connection, const DBusObjectPath& path)
    : connection(connection)
    , path(path)
    , registered(false) {
}

DBusObject::~DBusObject() {
    unregisterObject();
}

bool DBusObject::addInterface(const std::string& interface, const std::vector<DBusProperty>& properties) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Cannot add interfaces to already registered object
    if (registered) {
        Logger::warn("Cannot add interface to already registered object: " + path.toString());
        return false;
    }
    
    // Update properties if interface already exists
    auto it = interfaces.find(interface);
    if (it != interfaces.end()) {
        it->second = properties;
        return true;
    }
    
    // Add new interface
    interfaces[interface] = properties;
    Logger::debug("Added interface: " + interface + " to object: " + path.toString());
    return true;
}

bool DBusObject::addMethod(const std::string& interface, const std::string& method, DBusConnection::MethodHandler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (registered) {
        Logger::warn("Cannot add method to already registered object: " + path.toString());
        return false;
    }
    
    // Auto-create interface if it doesn't exist
    if (interfaces.find(interface) == interfaces.end()) {
        interfaces[interface] = {};
    }
    
    methodHandlers[interface][method] = handler;
    Logger::debug("Added method: " + interface + "." + method + " to object: " + path.toString());
    return true;
}

bool DBusObject::addMethodWithSignature(
    const std::string& interface, 
    const std::string& method, 
    DBusConnection::MethodHandler handler,
    const std::string& inSignature, 
    const std::string& outSignature)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (registered) {
        Logger::warn("Cannot add method to already registered object: " + path.toString());
        return false;
    }
    
    // Add interface if it doesn't exist
    if (interfaces.find(interface) == interfaces.end()) {
        interfaces[interface] = {};
    }
    
    // Store method handler
    methodHandlers[interface][method] = handler;
    
    // Store method signatures for XML generation
    methodSignatures[interface][method] = std::make_pair(inSignature, outSignature);
    
    Logger::debug("Added method with signature: " + interface + "." + method + 
                 " (in: " + inSignature + ", out: " + outSignature + ") to object: " + path.toString());
    return true;
}

bool DBusObject::emitPropertyChanged(const std::string& interface, const std::string& name, GVariantPtr value) {
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    return connection.emitPropertyChanged(path, interface, name, std::move(value));
}

bool DBusObject::emitSignal(const std::string& interface, const std::string& name, GVariantPtr parameters) {
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    // Create empty tuple if no parameters provided
    if (!parameters) {
        GVariant* empty_tuple = g_variant_new_tuple(NULL, 0);
        GVariantPtr empty_params = makeGVariantPtr(empty_tuple, true);
        return connection.emitSignal(path, interface, name, std::move(empty_params));
    }
    
    return connection.emitSignal(path, interface, name, std::move(parameters));
}

void DBusObject::addStandardInterfaces() {
    // Add Introspectable interface
    addMethod("org.freedesktop.DBus.Introspectable", "Introspect",
              [this](const DBusMethodCall& call) { this->handleIntrospect(call); });
    
    // Properties interface is handled automatically by DBusConnection
}

void DBusObject::handleIntrospect(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in Introspect");
        return;
    }
    
    Logger::debug("Introspect method called for object: " + getPath().toString());
    
    try {
        std::string xml = generateIntrospectionXml();
        
        // Create response
        GVariantPtr response = Utils::gvariantPtrFromString(xml);
        if (!response) {
            Logger::error("Failed to create GVariant for introspection XML");
            g_dbus_method_invocation_return_error_literal(
                call.invocation.get(),
                G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED,
                "Internal error"
            );
            return;
        }
        
        // Return XML
        g_dbus_method_invocation_return_value(
            call.invocation.get(),
            g_variant_new("(s)", g_variant_get_string(response.get(), NULL))
        );
    }
    catch (const std::exception& e) {
        Logger::error("Exception in handleIntrospect: " + std::string(e.what()));
        g_dbus_method_invocation_return_error_literal(
            call.invocation.get(),
            G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED,
            e.what()
        );
    }
}

bool DBusObject::hasInterface(const std::string& interface) const {
    return methodHandlers.find(interface) != methodHandlers.end() ||
           interfaces.find(interface) != interfaces.end();
}

bool DBusObject::registerObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // If already registered, return success
    if (registered) {
        Logger::debug("Object already registered: " + path.toString());
        return true;
    }
    
    // Check connection
    if (!connection.isConnected()) {
        Logger::error("Cannot register object: D-Bus connection not available");
        return false;
    }
    
    // Add standard interfaces (Introspectable, Properties)
    addStandardInterfaces();
    
    // Generate introspection XML
    std::string xml = generateIntrospectionXml();
    
    // Register object with connection
    registered = connection.registerObject(
        path,
        xml,
        methodHandlers,
        interfaces
    );
    
    if (registered) {
        Logger::info("Registered D-Bus object at path: " + path.toString());
        Logger::info("Registered D-Bus object: " + path.toString());
    } else {
        Logger::error("Failed to register D-Bus object: " + path.toString());
    }
    
    return registered;
}

bool DBusObject::unregisterObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!registered) {
        return true; // Already unregistered
    }
    
    // If connection is lost, just update local state
    if (!connection.isConnected()) {
        Logger::warn("D-Bus connection not available, updating local registration state only");
        registered = false;
        return true;
    }
    
    // Try to unregister
    bool success = connection.unregisterObject(path);
    if (success) {
        registered = false;
        Logger::info("Unregistered D-Bus object: " + path.toString());
    } else {
        Logger::error("Failed to unregister D-Bus object: " + path.toString());
    }
    
    return success;
}

std::string DBusObject::generateIntrospectionXml() const {
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<node>\n";
    
    // Always add Introspectable interface
    xml += "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n";
    xml += "    <method name=\"Introspect\">\n";
    xml += "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n";
    xml += "    </method>\n";
    xml += "  </interface>\n";
    
    // Always add Properties interface
    xml += "  <interface name=\"org.freedesktop.DBus.Properties\">\n";
    xml += "    <method name=\"Get\">\n";
    xml += "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n";
    xml += "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n";
    xml += "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n";
    xml += "    </method>\n";
    xml += "    <method name=\"Set\">\n";
    xml += "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n";
    xml += "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n";
    xml += "      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n";
    xml += "    </method>\n";
    xml += "    <method name=\"GetAll\">\n";
    xml += "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n";
    xml += "      <arg name=\"properties\" type=\"a{sv}\" direction=\"out\"/>\n";
    xml += "    </method>\n";
    xml += "    <signal name=\"PropertiesChanged\">\n";
    xml += "      <arg name=\"interface_name\" type=\"s\"/>\n";
    xml += "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n";
    xml += "      <arg name=\"invalidated_properties\" type=\"as\"/>\n";
    xml += "    </signal>\n";
    xml += "  </interface>\n";
    
    // Only add ObjectManager if it has a handler defined
    bool hasObjectManager = hasInterface("org.freedesktop.DBus.ObjectManager");
    if (hasObjectManager) {
        xml += "  <interface name=\"org.freedesktop.DBus.ObjectManager\">\n";
        xml += "    <method name=\"GetManagedObjects\">\n";
        xml += "      <arg name=\"objects\" type=\"a{oa{sa{sv}}}\" direction=\"out\"/>\n";
        xml += "    </method>\n";
        xml += "    <signal name=\"InterfacesAdded\">\n";
        xml += "      <arg name=\"object_path\" type=\"o\"/>\n";
        xml += "      <arg name=\"interfaces_and_properties\" type=\"a{sa{sv}}\"/>\n";
        xml += "    </signal>\n";
        xml += "    <signal name=\"InterfacesRemoved\">\n";
        xml += "      <arg name=\"object_path\" type=\"o\"/>\n";
        xml += "      <arg name=\"interfaces\" type=\"as\"/>\n";
        xml += "    </signal>\n";
        xml += "  </interface>\n";
    }
    
    // Add custom interfaces
    for (const auto& iface : interfaces) {
        // Skip already added standard interfaces
        if (iface.first == "org.freedesktop.DBus.Introspectable" ||
            iface.first == "org.freedesktop.DBus.Properties" ||
            (iface.first == "org.freedesktop.DBus.ObjectManager" && hasObjectManager)) {
            continue;
        }
        
        xml += "  <interface name=\"" + iface.first + "\">\n";
        
        // Add properties
        for (const auto& prop : iface.second) {
            xml += "    <property name=\"" + prop.name + "\" type=\"" + prop.signature + "\" access=\"";
            
            if (prop.readable && prop.writable) {
                xml += "readwrite";
            } else if (prop.readable) {
                xml += "read";
            } else if (prop.writable) {
                xml += "write";
            }
            
            if (prop.emitsChangedSignal) {
                xml += "\">\n";
                xml += "      <annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>\n";
                xml += "    </property>\n";
            } else {
                xml += "\"/>\n";
            }
        }
        
        // Add methods
        auto it = methodHandlers.find(iface.first);
        if (it != methodHandlers.end()) {
            for (const auto& method : it->second) {
                const std::string& methodName = method.first;
                
                // Check if we have signature info
                bool hasSignature = methodSignatures.count(iface.first) > 0 && 
                                   methodSignatures.at(iface.first).count(methodName) > 0;
                
                xml += "    <method name=\"" + methodName + "\">\n";
                
                // Add signatures if available
                if (hasSignature) {
                    const auto& signatures = methodSignatures.at(iface.first).at(methodName);
                    const std::string& inSig = signatures.first;
                    const std::string& outSig = signatures.second;
                    
                    // Add input arguments
                    if (!inSig.empty()) {
                        if (inSig == "a{sv}") {
                            xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        } else if (inSig == "ay") {
                            xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                        } else if (inSig == "aya{sv}") {
                            xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                            xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        } else {
                            xml += "      <arg name=\"arg0\" type=\"" + inSig + "\" direction=\"in\"/>\n";
                        }
                    }
                    
                    // Add output arguments
                    if (!outSig.empty()) {
                        if (outSig == "ay") {
                            xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                        } else {
                            xml += "      <arg name=\"result\" type=\"" + outSig + "\" direction=\"out\"/>\n";
                        }
                    }
                }
                // Handle special BlueZ method signatures
                else if (iface.first == "org.bluez.GattCharacteristic1") {
                    if (methodName == "ReadValue") {
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                    } else if (methodName == "WriteValue") {
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                    }
                } else if (iface.first == "org.bluez.GattDescriptor1") {
                    if (methodName == "ReadValue") {
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                    } else if (methodName == "WriteValue") {
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                    }
                } else if (iface.first == "org.bluez.LEAdvertisement1" && methodName == "Release") {
                    // No arguments for Release
                }
                
                xml += "    </method>\n";
            }
        }
        
        xml += "  </interface>\n";
    }
    
    xml += "</node>";
    return xml;
}

} // namespace ggk