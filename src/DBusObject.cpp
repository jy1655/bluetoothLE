#include "DBusObject.h"
#include "DBusXml.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

DBusObject::DBusObject(std::shared_ptr<IDBusConnection> connection, const DBusObjectPath& path)
    : connection(connection)
    , path(path)
    , registered(false)
    , registrationFinished(false) {
}

DBusObject::~DBusObject() {
    unregisterObject();
}

bool DBusObject::addInterface(const std::string& interface, const std::vector<DBusProperty>& properties) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 수정 가능 여부 검사
    if (!canModify()) {
        Logger::error("Cannot add interface to finalized object: " + path.toString());
        return false;
    }
    
    // 이미 존재하는 인터페이스이면 속성 업데이트
    auto it = interfaces.find(interface);
    if (it != interfaces.end()) {
        it->second = properties;
        Logger::debug("Updated existing interface: " + interface + " on " + path.toString());
        return true;
    }
    
    // 새 인터페이스 추가
    interfaces[interface] = properties;
    Logger::debug("Added interface: " + interface + " to object: " + path.toString());
    return true;
}

bool DBusObject::addMethod(const std::string& interface, const std::string& method, IDBusConnection::MethodHandler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 수정 가능 여부 검사
    if (!canModify()) {
        Logger::error("Cannot add method to finalized object: " + path.toString());
        return false;
    }
    
    // 인터페이스가 없으면 자동 생성
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
    IDBusConnection::MethodHandler handler,
    const std::string& inSignature, 
    const std::string& outSignature)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    // 수정 가능 여부 검사
    if (!canModify()) {
        Logger::error("Cannot add method to finalized object: " + path.toString());
        return false;
    }
    
    // 인터페이스가 없으면 추가
    if (interfaces.find(interface) == interfaces.end()) {
        interfaces[interface] = {};
    }
    
    // 메서드 핸들러 저장
    methodHandlers[interface][method] = handler;
    
    // XML 생성용 시그니처 저장
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
    
    return connection->emitPropertyChanged(path, interface, name, std::move(value));
}

bool DBusObject::emitSignal(const std::string& interface, const std::string& name, GVariantPtr parameters) {
    if (!registered) {
        Logger::error("Cannot emit signals on unregistered object: " + path.toString());
        return false;
    }
    
    // 매개변수가 없으면 빈 튜플 생성
    if (!parameters) {
        GVariant* empty_tuple = g_variant_new_tuple(NULL, 0);
        GVariantPtr empty_params = makeGVariantPtr(empty_tuple, true);
        return connection->emitSignal(path, interface, name, std::move(empty_params));
    }
    
    return connection->emitSignal(path, interface, name, std::move(parameters));
}

bool DBusObject::finishRegistration() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 이미 완료된 경우
    if (registrationFinished) {
        Logger::warn("Registration already finished for object: " + path.toString());
        return registered;
    }
    
    // 표준 인터페이스 추가 (Introspectable, Properties)
    addStandardInterfaces();
    
    // 등록 실행
    bool success = registerObject();
    
    // 등록 성공 여부에 관계없이 완료로 표시
    registrationFinished = true;
    
    if (success) {
        Logger::info("Finished registration for object: " + path.toString());
    } else {
        Logger::error("Failed to register object: " + path.toString());
    }
    
    return success;
}

void DBusObject::addStandardInterfaces() {
    if (canModify()) {
        // Introspectable 인터페이스 추가
        addMethod("org.freedesktop.DBus.Introspectable", "Introspect",
                [this](const DBusMethodCall& call) { this->handleIntrospect(call); });
        
        // Properties 인터페이스는 DBusConnection에서 자동 처리
    }
}

void DBusObject::handleIntrospect(const DBusMethodCall& call) {
    if (!call.invocation) {
        Logger::error("Invalid method invocation in Introspect");
        return;
    }
    
    Logger::debug("Introspect method called for object: " + getPath().toString());
    
    try {
        std::string xml = generateIntrospectionXml();
        
        // 응답 생성
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
        
        // XML 반환
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
    
    // 이미 등록된 경우
    if (registered) {
        Logger::debug("Object already registered: " + path.toString());
        return true;
    }
    
    // 연결 확인
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot register object: D-Bus connection not available");
        return false;
    }
    
    // 인트로스펙션 XML 생성
    std::string xml = generateIntrospectionXml();
    
    // 객체 등록
    registered = connection->registerObject(
        path,
        xml,
        methodHandlers,
        interfaces
    );
    
    if (registered) {
        Logger::info("Registered D-Bus object at path: " + path.toString());
    } else {
        Logger::error("Failed to register D-Bus object: " + path.toString());
    }
    
    return registered;
}

bool DBusObject::unregisterObject() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 등록되지 않은 경우
    if (!registered) {
        return true;
    }
    
    // 연결 상태 확인
    if (!connection || !connection->isConnected()) {
        Logger::warn("D-Bus connection not available, updating local registration state only");
        registered = false;
        return true;
    }
    
    // 등록 해제 시도
    bool success = connection->unregisterObject(path);
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
    
    // Introspectable 인터페이스 추가
    xml += "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n";
    xml += "    <method name=\"Introspect\">\n";
    xml += "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n";
    xml += "    </method>\n";
    xml += "  </interface>\n";
    
    // Properties 인터페이스 추가
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
    
    // ObjectManager 인터페이스 조건부 추가
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
    
    // 사용자 정의 인터페이스 추가
    for (const auto& iface : interfaces) {
        // 이미 추가된 표준 인터페이스는 건너뜀
        if (iface.first == "org.freedesktop.DBus.Introspectable" ||
            iface.first == "org.freedesktop.DBus.Properties" ||
            (iface.first == "org.freedesktop.DBus.ObjectManager" && hasObjectManager)) {
            continue;
        }
        
        xml += "  <interface name=\"" + iface.first + "\">\n";
        
        // 속성 추가
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
        
        // 메서드 추가
        auto it = methodHandlers.find(iface.first);
        if (it != methodHandlers.end()) {
            for (const auto& method : it->second) {
                const std::string& methodName = method.first;
                
                // 시그니처 정보 확인
                bool hasSignature = methodSignatures.count(iface.first) > 0 && 
                                   methodSignatures.at(iface.first).count(methodName) > 0;
                
                xml += "    <method name=\"" + methodName + "\">\n";
                
                // 시그니처 정보가 있으면 추가
                if (hasSignature) {
                    const auto& signatures = methodSignatures.at(iface.first).at(methodName);
                    const std::string& inSig = signatures.first;
                    const std::string& outSig = signatures.second;
                    
                    // 입력 인자 추가
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
                    
                    // 출력 인자 추가
                    if (!outSig.empty()) {
                        if (outSig == "ay") {
                            xml += "      <arg name=\"value\" type=\"ay\" direction=\"out\"/>\n";
                        } else {
                            xml += "      <arg name=\"result\" type=\"" + outSig + "\" direction=\"out\"/>\n";
                        }
                    }
                }
                // BlueZ의 특수한 메서드 시그니처 처리
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
                    // Release 메서드는 인자 없음
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