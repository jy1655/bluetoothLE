#include "SDBusObject.h"
#include "SDBusError.h"

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

bool SDBusObject::registerSignal(const sdbus::InterfaceName& interfaceName, const sdbus::SignalName& signalName) {
    std::lock_guard<std::mutex> lock(objectMutex);
    
    if (registered || !object) {
        return false;
    }
    
    try {
        // Create an empty signal vtable item for the signal
        sdbus::SignalVTableItem signalVTable{
            signalName,
            sdbus::Signature{""}, // Empty signature - will be determined at runtime
            {},
            {}
        };
        
        // Add vtable to object
        object->addVTable(signalVTable).forInterface(interfaceName);
        
        Logger::info("Registered signal " + std::string(signalName) + " on interface " + std::string(interfaceName));
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to register signal: " + std::string(e.what()));
        return false;
    }
}

bool SDBusObject::emitPropertyChanged(const sdbus::InterfaceName& interfaceName, const sdbus::PropertyName& propertyName) {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (!registered || !object) {
        return false;
    }

    try {
        std::map<std::string, sdbus::Variant> changedProps;
        changedProps[propertyName] = sdbus::Variant();
        std::vector<std::string> invalidatedProps;

        // PropertiesChanged signal on org.freedesktop.DBus.Properties interface
        sdbus::InterfaceName propertiesInterface{"org.freedesktop.DBus.Properties"};
        sdbus::SignalName signalName{"PropertiesChanged"};
        
        auto signal = object->createSignal(propertiesInterface, signalName);
        
        // Add arguments to signal
        signal << interfaceName;
        signal << changedProps;
        signal << invalidatedProps;
        
        // Emit the signal
        object->emitSignal(signal);

        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to emit property changed signal: " + std::string(e.what()));
        return false;
    }
}

sdbus::IObject& SDBusObject::getSdbusObject() {
    if (!object) {
        throw sdbus::Error(sdbus::Error::Name("org.freedesktop.DBus.Error.Failed"), "Object not initialized");
    }
    return *object;
}

bool SDBusObject::registerObjectManager(std::function<ManagedObjectsDict()> handler) {
    std::lock_guard<std::mutex> lock(objectMutex);

    if (!object || registered) {
        return false;
    }

    try {
        // 내장 ObjectManager 사용
        object->addObjectManager();
        
        // 핸들러는 더 이상 필요하지 않지만, 
        // 이전 코드와의 호환성을 위해 성공 값 반환
        return true;
    } catch (const std::exception& e) {
        Logger::error("ObjectManager registration failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace ggk