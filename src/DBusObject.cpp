#include "DBusObject.h"
#include "DBusInterface.h"
#include "GattApplication.h"
#include "Logger.h"

namespace ggk {

DBusObject::DBusObject(const DBusObjectPath& path, bool publish)
    : publish(publish)
    , path(path)
    , pParent(nullptr) {
}

DBusObject::DBusObject(DBusObject* pParent, const DBusObjectPath& pathElement)
    : publish(false)
    , path(pathElement)
    , pParent(pParent) {
}

bool DBusObject::isPublished() const {
    return publish;
}

const DBusObjectPath& DBusObject::getPathNode() const {
    return path;
}

DBusObjectPath DBusObject::getPath() const {
    if (pParent == nullptr) {
        return path;
    }
    
    return pParent->getPath() + path;
}

DBusObject& DBusObject::getParent() {
    if (pParent == nullptr) {
        throw std::runtime_error("No parent object");
    }
    return *pParent;
}

const std::list<DBusObject>& DBusObject::getChildren() const {
    return children;
}

DBusObject& DBusObject::addChild(const DBusObjectPath& pathElement) {
    children.emplace_back(this, pathElement);
    return children.back();
}

const DBusObject::InterfaceList& DBusObject::getInterfaces() const {
    return interfaces;
}

std::string DBusObject::generateIntrospectionXML(int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    // 노드 시작
    xml += indent + "<node name=\"" + path.toString() + "\">\n";

    // 인터페이스 XML 생성
    for (const auto& interface : interfaces) {
        xml += interface->generateIntrospectionXML(depth + 1);
    }

    // 자식 노드 XML 생성
    for (const auto& child : children) {
        xml += child.generateIntrospectionXML(depth + 1);
    }

    // 노드 종료
    xml += indent + "</node>\n";

    return xml;
}

GattApplication* DBusObject::findGattApplication() const {
    DBusObject* current = pParent;
    while (current) {
        for (const auto& interface : current->interfaces) {
            if (auto app = dynamic_cast<GattApplication*>(interface.get())) {
                return app;
            }
        }
        current = current->pParent;
    }
    return nullptr;
}

GattService& DBusObject::gattServiceBegin(const std::string& pathElement, const GattUuid& uuid) {
    // GattApplication을 통해 서비스를 등록하도록 수정
    DBusObject& serviceObject = addChild(pathElement);
    auto service = std::make_shared<GattService>(uuid);
    
    // GattApplication에도 서비스 등록
    if (auto app = findGattApplication()) {
        app->addService(service);
    }
    
    return *serviceObject.addInterface(service);
}

std::shared_ptr<const DBusInterface> DBusObject::findInterface(
    const DBusObjectPath& path,
    const std::string& interfaceName,
    const DBusObjectPath& basePath) const {

    DBusObjectPath fullPath = basePath + getPath();
    
    // 현재 객체의 경로가 찾고자 하는 경로와 일치하는지 확인
    if (fullPath == path) {
        // 인터페이스 찾기
        for (const auto& interface : interfaces) {
            if (interface->getName() == interfaceName) {
                return interface;
            }
        }
        return nullptr;
    }

    // 자식 객체들에서 재귀적으로 검색
    for (const auto& child : children) {
        auto result = child.findInterface(path, interfaceName, basePath);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

bool DBusObject::callMethod(
    const DBusObjectPath& path,
    const std::string& interfaceName,
    const std::string& methodName,
    GDBusConnection* pConnection,
    GVariant* pParameters,
    GDBusMethodInvocation* pInvocation,
    gpointer pUserData,
    const DBusObjectPath& basePath) const {

    auto interface = findInterface(path, interfaceName, basePath);
    if (!interface) {
        Logger::error("Interface not found: " + interfaceName + " at path: " + path.toString());
        return false;
    }

    return interface->callMethod(methodName, pConnection, pParameters, pInvocation, pUserData);
}

void DBusObject::emitSignal(
    GDBusConnection* pBusConnection,
    const std::string& interfaceName,
    const std::string& signalName,
    GVariant* pParameters) {
    
    if (!pBusConnection) {
        Logger::error("No bus connection for signal emission");
        return;
    }

    GError* pError = nullptr;
    g_dbus_connection_emit_signal(
        pBusConnection,
        nullptr,                    // 대상 지정하지 않음 (브로드캐스트)
        getPath().c_str(),         // 객체 경로
        interfaceName.c_str(),     // 인터페이스 이름
        signalName.c_str(),        // 시그널 이름
        pParameters,               // 시그널 파라미터
        &pError
    );

    if (pError) {
        Logger::error("Failed to emit signal: " + std::string(pError->message));
        g_error_free(pError);
    }
}

} // namespace ggk