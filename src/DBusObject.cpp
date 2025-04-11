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
    
    if (registered) {
        Logger::warn("Cannot add interface to already registered object: " + path.toString());
        return false;
    }
    
    // 인터페이스가 이미 존재하는지 확인
    auto it = interfaces.find(interface);
    if (it != interfaces.end()) {
        Logger::warn("Interface already exists: " + interface + " on object: " + path.toString());
        // 인터페이스가 이미 존재하는 경우, 속성을 병합하거나 교체할 수 있음
        // 여기서는 간단히 새 속성으로 교체
        it->second = properties;
        Logger::debug("Updated properties for interface: " + interface + " on object: " + path.toString());
        return true;
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
    
    for (const auto& prop : ifaceIt->second) {
        if (prop.name == name) {
            if (!prop.readable) {
                Logger::error("Property is not readable: " + interface + "." + name);
                return makeNullGVariantPtr();
            }
            
            if (prop.getter) {
                try {
                    GVariant* value = prop.getter();
                    if (value) {
                        // makeGVariantPtr 패턴 사용
                        return makeGVariantPtr(value, true);  // true = 소유권 가져오기
                    }
                } catch (const std::exception& e) {
                    Logger::error("Exception in property getter: " + std::string(e.what()));
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
    
    // 속성 변경 시그널 발생 - 이동 의미론 사용
    return connection.emitPropertyChanged(path, interface, name, std::move(value));
}

bool DBusObject::emitSignal(
    const std::string& interface,
    const std::string& name,
    GVariantPtr parameters)
{
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    // parameters가 nullptr이면 빈 튜플 생성
    if (!parameters) {
        // 빈 튜플 생성
        GVariant* empty_tuple = g_variant_new_tuple(NULL, 0);
        // makeGVariantPtr 패턴 사용
        GVariantPtr owned_params = makeGVariantPtr(empty_tuple, true);
        // 소유권을 connection.emitSignal()에 전달
        return connection.emitSignal(path, interface, name, std::move(owned_params));
    }
    
    // parameters가 튜플 타입인지 확인
    if (!g_variant_is_of_type(parameters.get(), G_VARIANT_TYPE_TUPLE)) {
        Logger::error("Signal parameters must be a tuple");
        return false;
    }
    
    // 원본 GVariant에 대한 새 참조 생성 (소유권 복제)
    GVariant* param_raw = parameters.get();
    // makeGVariantPtr 패턴 사용
    GVariantPtr owned_params = makeGVariantPtr(param_raw, false);  // false = 참조만 복사
    
    // 소유권을 connection.emitSignal()에 전달
    return connection.emitSignal(path, interface, name, std::move(owned_params));
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
        Logger::debug("Object not registered, nothing to unregister: " + path.toString());
        return true;
    }
    
    // 연결이 끊어진 경우 로컬 상태만 업데이트
    if (!connection.isConnected()) {
        Logger::warn("D-Bus connection not available, updating local registration state only for: " + path.toString());
        registered = false;
        return true;
    }
    
    bool unregResult = connection.unregisterObject(path);
    if (unregResult) {
        registered = false;
        Logger::info("Unregistered D-Bus object: " + path.toString());
        return true;
    } else {
        Logger::error("Failed to unregister D-Bus object: " + path.toString());
        return false;
    }
}

std::string DBusObject::generateIntrospectionXml() const {
    // 메서드 목록 수집
    std::vector<std::pair<std::string, std::string>> methods;
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
                // 수정된 부분: 기본 생성자를 통한 안전한 객체 생성
                DBusMethodCall call;
                call.interface = iface.first;
                call.method = method.first;
                
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