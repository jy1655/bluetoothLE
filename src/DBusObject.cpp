#include "DBusObject.h"
#include "DBusXml.h"
#include "Logger.h"

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
    
    if (registered) {
        Logger::warn("Cannot add interface to already registered object: " + path.toString());
        return false;
    }
    
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
    
    // 인터페이스가 존재하지 않으면 자동 생성
    if (interfaces.find(interface) == interfaces.end()) {
        interfaces[interface] = {};
    }
    
    methodHandlers[interface][method] = handler;
    Logger::debug("Added method: " + interface + "." + method + " to object: " + path.toString());
    return true;
}

bool DBusObject::setProperty(const std::string& interface, const std::string& name, GVariantPtr value) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto ifaceIt = interfaces.find(interface);
    if (ifaceIt == interfaces.end()) {
        Logger::error("Interface not found: " + interface);
        return false;
    }
    
    // 속성 찾기
    for (auto& prop : ifaceIt->second) {
        if (prop.name == name) {
            if (!prop.writable) {
                Logger::error("Property is not writable: " + interface + "." + name);
                return false;
            }
            
            if (prop.setter) {
                return prop.setter(value.get());
            } else {
                Logger::error("No setter for property: " + interface + "." + name);
                return false;
            }
        }
    }
    
    Logger::error("Property not found: " + interface + "." + name);
    return false;
}

GVariantPtr DBusObject::getProperty(const std::string& interface, const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto ifaceIt = interfaces.find(interface);
    if (ifaceIt == interfaces.end()) {
        Logger::error("Interface not found: " + interface);
        return makeNullGVariantPtr();
    }
    
    // 속성 찾기
    for (const auto& prop : ifaceIt->second) {
        if (prop.name == name) {
            if (!prop.readable) {
                Logger::error("Property is not readable: " + interface + "." + name);
                return makeNullGVariantPtr();
            }
            
            if (prop.getter) {
                GVariant* value = prop.getter();
                if (value) {
                    return GVariantPtr(value, &g_variant_unref);
                }
            } else {
                Logger::error("No getter for property: " + interface + "." + name);
            }
            
            return makeNullGVariantPtr();
        }
    }
    
    Logger::error("Property not found: " + interface + "." + name);
    return makeNullGVariantPtr();
}

bool DBusObject::emitPropertyChanged(const std::string& interface, const std::string& name, GVariantPtr value) {
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    // 속성 변경 시그널 발생
    return connection.emitPropertyChanged(path, interface, name, std::move(value));
}

bool DBusObject::emitSignal(const std::string& interface, const std::string& name, GVariantPtr parameters) {
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    // 시그널 발생
    return connection.emitSignal(path, interface, name, std::move(parameters));
}

bool DBusObject::registerObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (registered) {
        Logger::debug("Object already registered: " + path.toString());
        return true;
    }
    
    if (!connection.isConnected()) {
        Logger::error("D-Bus connection not available");
        return false;
    }
    
    // 인트로스펙션 XML 생성
    std::string xml = generateIntrospectionXml();
    Logger::debug("Registering object with XML:\n" + xml);
    
    // 객체 등록
    registered = connection.registerObject(
        path,
        xml,
        methodHandlers,
        interfaces
    );
    
    if (registered) {
        Logger::info("Registered D-Bus object: " + path.toString());
    } else {
        Logger::error("Failed to register D-Bus object: " + path.toString());
    }
    
    return registered;
}

bool DBusObject::unregisterObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!registered) {
        return true;
    }
    
    registered = !connection.unregisterObject(path);
    if (!registered) {
        Logger::info("Unregistered D-Bus object: " + path.toString());
    } else {
        Logger::error("Failed to unregister D-Bus object: " + path.toString());
    }
    
    return !registered;
}

std::string DBusObject::generateIntrospectionXml() const {
    // 메서드 목록 수집
    std::vector<std::pair<std::string, std::string>> methods; // 인터페이스와 메서드 이름만 저장
    for (const auto& ifaceMethods : methodHandlers) {
        for (const auto& method : ifaceMethods.second) {
            methods.push_back({ifaceMethods.first, method.first});
        }
    }
    
    // 시그널 목록은 현재 지원하지 않음
    std::vector<DBusSignal> signals;
    
    // 각 인터페이스에 대한 XML 생성
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<node>\n";
    
    for (const auto& iface : interfaces) {
        // 해당 인터페이스의 메서드 필터링
        std::vector<DBusMethodCall> ifaceMethods;
        auto it = methodHandlers.find(iface.first);
        if (it != methodHandlers.end()) {
            for (const auto& method : it->second) {
                // 복사 대신 직접 생성자 호출
                DBusMethodCall call(
                    "",  // sender
                    iface.first,  // interface
                    method.first,  // method
                    makeNullGVariantPtr(),  // parameters
                    makeNullGDBusMethodInvocationPtr()  // invocation
                );
                // 이동(move) 사용하여 복사 방지
                ifaceMethods.push_back(std::move(call));
            }
        }
        
        // 인터페이스 XML 생성
        xml += DBusXml::createInterface(
            iface.first,
            iface.second,
            ifaceMethods,
            signals,
            1  // 들여쓰기 레벨
        );
    }
    
    xml += "</node>";
    return xml;
}

} // namespace ggk