#include "DBusObject.h"
#include "Logger.h"

namespace ggk {

DBusObject::DBusObject(const DBusObjectPath& path, bool shouldPublish)
    : state(shouldPublish ? State::INITIALIZED : State::ERROR)
    , path(path)
    , pParent(nullptr)
    , dbusConnection(nullptr) {
}

DBusObject::DBusObject(DBusObject* pParent, const DBusObjectPath& pathElement)
    : state(State::INITIALIZED)
    , path(pathElement)
    , pParent(pParent)
    , dbusConnection(nullptr) {
}

DBusObject::~DBusObject() {
    cleanup();
}

bool DBusObject::publish(GDBusConnection* connection) {
    if (!connection) {
        Logger::error("Cannot publish DBusObject: null connection");
        return false;
    }

    if (state == State::PUBLISHED) {
        return true;
    }

    if (!validatePath()) {
        Logger::error("Cannot publish DBusObject: invalid path");
        return false;
    }

    dbusConnection = connection;
    setState(State::PUBLISHED);
    return true;
}

bool DBusObject::unpublish() {
    if (state != State::PUBLISHED) {
        return true;
    }

    setState(State::INITIALIZED);
    dbusConnection = nullptr;
    return true;
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
        throw DBusObjectError("No parent object");
    }
    return *pParent;
}

const DBusObject& DBusObject::getParent() const {
    if (pParent == nullptr) {
        throw DBusObjectError("No parent object");
    }
    return *pParent;
}

const DBusObject::ChildList& DBusObject::getChildren() const {
    return children;
}

DBusObject& DBusObject::addChild(const DBusObjectPath& pathElement) {
    children.emplace_back(this, pathElement);
    onChildAdded(children.back());
    return children.back();
}

DBusObject* DBusObject::findChild(const DBusObjectPath& path) {
    for (auto& child : children) {
        if (child.getPathNode() == path) {
            return &child;
        }
    }
    return nullptr;
}

void DBusObject::removeChild(const DBusObjectPath& path) {
    children.remove_if([&path](const DBusObject& child) {
        return child.getPathNode() == path;
    });
}

const DBusObject::InterfaceList& DBusObject::getInterfaces() const {
    return interfaces;
}

std::shared_ptr<const DBusInterface> DBusObject::findInterface(
    const DBusObjectPath& path,
    const std::string& interfaceName,
    const DBusObjectPath& basePath) const {

    DBusObjectPath fullPath = basePath + getPath();
    
    if (fullPath == path) {
        for (const auto& interface : interfaces) {
            if (interface->getName() == interfaceName) {
                return interface;
            }
        }
        return nullptr;
    }

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
        nullptr,
        getPath().c_str(),
        interfaceName.c_str(),
        signalName.c_str(),
        pParameters,
        &pError
    );

    if (pError) {
        Logger::error("Failed to emit signal: " + std::string(pError->message));
        g_error_free(pError);
    }
}

std::string DBusObject::generateIntrospectionXML(int depth) const {
    std::string indent(depth * 2, ' ');
    std::string xml;

    xml += indent + "<node name=\"" + path.toString() + "\">\n";

    for (const auto& interface : interfaces) {
        xml += interface->generateIntrospectionXML(depth + 1);
    }

    for (const auto& child : children) {
        xml += child.generateIntrospectionXML(depth + 1);
    }

    xml += indent + "</node>\n";

    return xml;
}

void DBusObject::setState(State newState) {
    if (state != newState) {
        State oldState = state;
        state = newState;
        onStateChanged(oldState, newState);
    }
}

bool DBusObject::validatePath() const {
    return !path.toString().empty() && path.toString()[0] == '/';
}

void DBusObject::cleanup() {
    interfaces.clear();
    children.clear();
    dbusConnection = nullptr;
    state = State::ERROR;
}

// Protected virtual methods의 기본 구현
void DBusObject::onStateChanged(State oldState, State newState) {}
void DBusObject::onInterfaceAdded(const std::shared_ptr<DBusInterface>& interface) {}
void DBusObject::onChildAdded(DBusObject& child) {}

} // namespace ggk