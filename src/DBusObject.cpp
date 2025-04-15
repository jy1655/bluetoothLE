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
    
    // 이미 등록된 객체에 인터페이스 추가는 불가
    if (registered) {
        Logger::warn("Cannot add interface to already registered object: " + path.toString());
        return false;
    }
    
    // 인터페이스가 이미 존재하는 경우 속성 업데이트
    auto it = interfaces.find(interface);
    if (it != interfaces.end()) {
        it->second = properties;
        return true;
    }
    
    // 새 인터페이스 추가
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

bool DBusObject::addMethodWithSignature(const std::string& interface, const std::string& method, 
                                    DBusConnection::MethodHandler handler,
                                    const std::string& inSignature, 
                                    const std::string& outSignature) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (registered) {
        Logger::warn("Cannot add method to already registered object: " + path.toString());
        return false;
    }
    
    // 인터페이스가 존재하지 않으면 자동 추가
    auto ifaceIt = interfaces.find(interface);
    if (ifaceIt == interfaces.end()) {
        interfaces[interface] = {};
    }
    
    methodHandlers[interface][method] = handler;
    
    // 추가로 시그니처 정보 저장 (DBusXml 클래스가 해당 정보를 사용할 수 있도록)
    // 이 부분은 코드베이스에 따라 구현이 달라질 수 있습니다.
    // 간단한 예시를 들자면:
    
    // XML 파일을 사용하는 코드베이스인 경우 시그니처 정보를 적절히 전달
    // 또는 메서드 메타데이터를 저장하는 맵 추가
    
    Logger::debug("Added method with signature: " + interface + "." + method + 
                " (in: " + inSignature + ", out: " + outSignature + ") " + 
                "to object: " + path.toString());
    return true;
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
        GVariant* empty_tuple = g_variant_new_tuple(NULL, 0);
        GVariantPtr owned_params = makeGVariantPtr(empty_tuple, true);
        return connection.emitSignal(path, interface, name, std::move(owned_params));
    }
    
    // 원본 GVariant에 대한 참조 복제
    GVariant* param_raw = parameters.get();
    GVariantPtr owned_params = makeGVariantPtr(param_raw, false);  // false = 참조만 복사
    
    return connection.emitSignal(path, interface, name, std::move(owned_params));
}

bool DBusObject::registerObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 이미 등록된 경우 성공 반환
    if (registered) {
        Logger::debug("Object already registered: " + path.toString());
        return true;
    }
    
    // 연결 확인
    if (!connection.isConnected()) {
        Logger::error("D-Bus connection not available");
        return false;
    }
    
    // 인트로스펙션 XML 생성
    std::string xml = generateIntrospectionXml();
    
    // D-Bus에 객체 등록
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
        return true; // 이미 등록 해제됨
    }
    
    // 연결이 끊어진 경우 로컬 상태만 업데이트
    if (!connection.isConnected()) {
        Logger::warn("D-Bus connection not available, updating local registration state only");
        registered = false;
        return true;
    }
    
    // 객체 등록 해제 시도
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
    
    // 표준 인터페이스 추가
    xml += "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n";
    xml += "    <method name=\"Introspect\">\n";
    xml += "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n";
    xml += "    </method>\n";
    xml += "  </interface>\n";
    
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
    
    // ObjectManager 인터페이스 추가
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
    
    // 사용자 정의 인터페이스 추가
    for (const auto& iface : interfaces) {
        xml += "  <interface name=\"" + iface.first + "\">\n";
        
        // 인터페이스의 속성들 추가
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
        
        // 인터페이스의 메서드들 추가
        auto it = methodHandlers.find(iface.first);
        if (it != methodHandlers.end()) {
            for (const auto& method : it->second) {
                xml += "    <method name=\"" + method.first + "\">\n";
                
                // BlueZ 특정 메서드들을 위한 특별한 시그니처 추가
                if (iface.first == "org.bluez.GattCharacteristic1") {
                    if (method.first == "ReadValue") {
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                    } else if (method.first == "WriteValue") {
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                    } else if (method.first == "StartNotify" || method.first == "StopNotify") {
                        // 이 메서드들은 인자가 없음
                    }
                } else if (iface.first == "org.bluez.GattDescriptor1") {
                    if (method.first == "ReadValue") {
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                    } else if (method.first == "WriteValue") {
                        xml += "      <arg name=\"value\" type=\"ay\" direction=\"in\"/>\n";
                        xml += "      <arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>\n";
                    }
                } else if (iface.first == "org.bluez.LEAdvertisement1") {
                    if (method.first == "Release") {
                        // Release 메서드는 인자가 없음
                    }
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