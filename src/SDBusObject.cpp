#include "SDBusObject.h"

namespace ggk {

SDBusObject::SDBusObject(SDBusConnection& connection, const std::string& objectPath)
    : connection(connection),
      objectPath(objectPath),
      registered(false) {
    try {
        object = connection.createObject(objectPath);
        Logger::info("Created D-Bus object at path: " + objectPath);
    } catch (const std::exception& e) {
        Logger::error("Failed to create D-Bus object: " + std::string(e.what()));
        object = nullptr;
    }
}

SDBusObject::~SDBusObject() {
    if (registered) {
        unregisterObject();
    }
}

bool SDBusObject::registerObject() {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (registered) {
        Logger::debug("Object already registered: " + objectPath);
        return true;
    }

    if (!object) {
        Logger::error("Cannot register null object");
        return false;
    }

    registered = true;
    Logger::info("Registered D-Bus object: " + objectPath);
    return true;
}

bool SDBusObject::unregisterObject() {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (!registered) {
        return true;
    }

    if (!object) {
        registered = false;
        return true;
    }

    try {
        object->unregister();
        registered = false;
        Logger::info("Unregistered D-Bus object: " + objectPath);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to unregister D-Bus object: " + std::string(e.what()));
        return false;
    }
}

bool SDBusObject::registerSignal(const std::string& interfaceName, const std::string& signalName) {
    Logger::warn("registerSignal is no longer supported directly in sdbus-c++ 2.1.0. This method will need to be reworked using a custom signal structure.");
    return false;
}

bool SDBusObject::emitPropertyChanged(const std::string& interfaceName, const std::string& propertyName) {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (!registered || !object) {
        return false;
    }

    try {
        std::map<std::string, sdbus::Variant> changedProps;
        changedProps[propertyName] = sdbus::Variant();
        std::vector<std::string> invalidatedProps;

        object->emitSignal(
            sdbus::Signal{
                sdbus::InterfaceName{"org.freedesktop.DBus.Properties"},
                sdbus::MemberName{"PropertiesChanged"},
                interfaceName,
                changedProps,
                invalidatedProps
            }
        );

        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to emit property changed signal: " + std::string(e.what()));
        return false;
    }
}


sdbus::IObject* SDBusObject::getObject() {
    if (!object) {
        throw sdbus::Error(sdbus::Error::Name("org.freedesktop.DBus.Error.Failed"), "Object not initialized");
    }
    return object.get();
}

bool SDBusObject::registerObjectManager(std::function<ManagedObjectsDict()> handler) {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (!object || registered) {
        return false;
    }

    try {
        object->addVTable(
            "org.freedesktop.DBus.ObjectManager",
            {
                sdbus::MethodVTableItem(
                    sdbus::MemberName{"GetManagedObjects"},
                    sdbus::Signature{""},
                    sdbus::Signature{"a{oa{sa{sv}}}"},
                    [handler = std::move(handler)](sdbus::MethodCall& call) {
                        try {
                            auto reply = call.createReply();
                            reply << handler();
                            reply.send();
                        } catch (const std::exception& e) {
                            Logger::error("Exception in GetManagedObjects handler: " + std::string(e.what()));
                            throw sdbus::Error(sdbus::Error::Name{"org.freedesktop.DBus.Error.Failed"}, e.what());
                        }
                    }
                ),
                sdbus::SignalVTableItem{
                    sdbus::MemberName{"InterfacesAdded"},
                    sdbus::Signature{"oa{sa{sv}}"}
                },
                sdbus::SignalVTableItem{
                    sdbus::MemberName{"InterfacesRemoved"},
                    sdbus::Signature{"oas"}
                }
            }
        );

        return true;
    } catch (const std::exception& e) {
        Logger::error("ObjectManager registration failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace ggk
