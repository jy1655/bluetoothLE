// src/ConnectionManager.cpp
#include "ConnectionManager.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

ConnectionManager& ConnectionManager::getInstance() {
    static ConnectionManager instance;
    return instance;
}

ConnectionManager::ConnectionManager()
    : connection(nullptr), initialized(false) {
}

ConnectionManager::~ConnectionManager() {
    shutdown();
}

bool ConnectionManager::initialize(DBusConnection& dbusConnection) {
    if (initialized) {
        return true;
    }

    connection = &dbusConnection;
    registerSignalHandlers();
    initialized = true;

    Logger::info("ConnectionManager initialized");
    return true;
}

bool ConnectionManager::isInitialized() const { 
    return initialized; 
}

void ConnectionManager::shutdown() {
    if (!initialized) {
        return;
    }

    // Unregister signal handlers
    for (guint handlerId : signalHandlerIds) {
        if (connection) {
            connection->removeSignalWatch(handlerId);
        }
    }
    signalHandlerIds.clear();

    // Clear connected devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        connectedDevices.clear();
    }

    connection = nullptr;
    initialized = false;
    Logger::info("ConnectionManager shutdown");
}

void ConnectionManager::registerSignalHandlers() {
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot register signal handlers: D-Bus connection not available");
        return;
    }

    // 1. InterfacesAdded signal - detect new device connections
    guint id1 = connection->addSignalWatch(
        BlueZConstants::BLUEZ_SERVICE,
        BlueZConstants::OBJECT_MANAGER_INTERFACE,
        "InterfacesAdded",
        DBusObjectPath(BlueZConstants::ROOT_PATH),
        [this](const std::string& signalName, GVariantPtr parameters) {
            this->handleInterfacesAddedSignal(signalName, std::move(parameters));
        }
    );
    signalHandlerIds.push_back(id1);

    // 2. InterfacesRemoved signal - detect device disconnections
    guint id2 = connection->addSignalWatch(
        BlueZConstants::BLUEZ_SERVICE,
        BlueZConstants::OBJECT_MANAGER_INTERFACE,
        "InterfacesRemoved",
        DBusObjectPath(BlueZConstants::ROOT_PATH),
        [this](const std::string& signalName, GVariantPtr parameters) {
            this->handleInterfacesRemovedSignal(signalName, std::move(parameters));
        }
    );
    signalHandlerIds.push_back(id2);

    // 3. PropertiesChanged signal - detect property changes (connection state, MTU, etc.)
    guint id3 = connection->addSignalWatch(
        BlueZConstants::BLUEZ_SERVICE,
        BlueZConstants::PROPERTIES_INTERFACE,
        "PropertiesChanged",
        DBusObjectPath(""),  // Watch all object paths
        [this](const std::string& signalName, GVariantPtr parameters) {
            this->handlePropertiesChangedSignal(signalName, std::move(parameters));
        }
    );
    signalHandlerIds.push_back(id3);

    Logger::info("Registered BlueZ D-Bus signal handlers");
}

void ConnectionManager::handleInterfacesAddedSignal(const std::string& signalName, GVariantPtr parameters) {
    if (!parameters) {
        return;
    }

    try {
        // Format: (o, a{sa{sv}})
        // o - object path
        // a{sa{sv}} - interface to properties map
        const char* objectPath = nullptr;
        GVariant* interfacesVariant = nullptr;
        g_variant_get(parameters.get(), "(&o@a{sa{sv}})", &objectPath, &interfacesVariant);

        if (!objectPath || !interfacesVariant) {
            return;
        }

        // Use makeGVariantPtr instead of direct construction
        GVariantPtr interfaces = makeGVariantPtr(interfacesVariant, true);

        // Check for Device interface
        GVariant* deviceInterface = g_variant_lookup_value(interfaces.get(), 
                                                         BlueZConstants::DEVICE_INTERFACE.c_str(), 
                                                         G_VARIANT_TYPE("a{sv}"));
        if (deviceInterface) {
            // Check Connected property
            GVariant* connectedVariant = g_variant_lookup_value(deviceInterface, 
                                                             "Connected", 
                                                             G_VARIANT_TYPE_BOOLEAN);
            
            if (connectedVariant) {
                bool connected = g_variant_get_boolean(connectedVariant);
                g_variant_unref(connectedVariant);
                
                if (connected) {
                    // Get device address
                    GVariant* addressVariant = g_variant_lookup_value(deviceInterface, 
                                                                  "Address", 
                                                                  G_VARIANT_TYPE_STRING);
                    
                    if (addressVariant) {
                        const char* address = g_variant_get_string(addressVariant, nullptr);
                        std::string deviceAddress(address);
                        
                        // Add to connected devices
                        {
                            std::lock_guard<std::mutex> lock(devicesMutex);
                            connectedDevices[deviceAddress] = DBusObjectPath(objectPath);
                        }
                        
                        // Execute callback
                        if (onConnectionCallback) {
                            onConnectionCallback(deviceAddress);
                        }
                        
                        Logger::info("Device connected: " + deviceAddress + " at " + std::string(objectPath));
                        
                        g_variant_unref(addressVariant);
                    }
                }
            }
            
            g_variant_unref(deviceInterface);
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in handleInterfacesAddedSignal: " + std::string(e.what()));
    }
}

void ConnectionManager::handleInterfacesRemovedSignal(const std::string& signalName, GVariantPtr parameters) {
    if (!parameters) {
        return;
    }

    try {
        // Format: (o, as)
        // o - object path
        // as - removed interfaces list
        const char* objectPath = nullptr;
        GVariant* interfacesVariant = nullptr;
        g_variant_get(parameters.get(), "(&o@as)", &objectPath, &interfacesVariant);

        if (!objectPath || !interfacesVariant) {
            return;
        }

        // Use makeGVariantPtr instead of direct construction
        GVariantPtr interfaces = makeGVariantPtr(interfacesVariant, true);

        // Check if Device interface was removed
        bool deviceRemoved = false;
        
        gsize numInterfaces = g_variant_n_children(interfaces.get());
        for (gsize i = 0; i < numInterfaces; i++) {
            const char* interface = nullptr;
            g_variant_get_child(interfaces.get(), i, "&s", &interface);
            
            if (interface && std::string(interface) == BlueZConstants::DEVICE_INTERFACE) {
                deviceRemoved = true;
                break;
            }
        }

        if (deviceRemoved) {
            // Find which device was disconnected
            std::string deviceAddress;
            
            {
                std::lock_guard<std::mutex> lock(devicesMutex);
                for (auto it = connectedDevices.begin(); it != connectedDevices.end(); ) {
                    if (it->second.toString() == objectPath) {
                        deviceAddress = it->first;
                        it = connectedDevices.erase(it);
                        break;
                    } else {
                        ++it;
                    }
                }
            }
            
            if (!deviceAddress.empty() && onDisconnectionCallback) {
                onDisconnectionCallback(deviceAddress);
                Logger::info("Device disconnected: " + deviceAddress);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in handleInterfacesRemovedSignal: " + std::string(e.what()));
    }
}

void ConnectionManager::handlePropertiesChangedSignal(const std::string& signalName, GVariantPtr parameters) {
    if (!parameters) {
        return;
    }

    try {
        // Format: (s, a{sv}, as)
        // s - interface name
        // a{sv} - changed properties and values
        // as - invalidated properties
        const char* interface = nullptr;
        GVariant* propertiesVariant = nullptr;
        GVariant* invalidatedVariant = nullptr;
        
        g_variant_get(parameters.get(), "(&s@a{sv}@as)", 
                     &interface, &propertiesVariant, &invalidatedVariant);

        if (!interface || !propertiesVariant) {
            return;
        }

        // Use makeGVariantPtr instead of direct construction
        GVariantPtr properties = makeGVariantPtr(propertiesVariant, true);
        GVariantPtr invalidated = makeGVariantPtr(invalidatedVariant, true);

        std::string interfaceName(interface);

        // Check for Device interface Connected property change
        if (interfaceName == BlueZConstants::DEVICE_INTERFACE) {
            GVariant* connectedVariant = g_variant_lookup_value(properties.get(), 
                                                             "Connected", 
                                                             G_VARIANT_TYPE_BOOLEAN);
            
            if (connectedVariant) {
                // Connection state change is already handled by InterfacesAdded/Removed
                // so we skip duplicate processing here
                g_variant_unref(connectedVariant);
            }
        }

        // Execute property changed callback for all properties
        if (onPropertyChangedCallback) {
            GVariantIter iter;
            g_variant_iter_init(&iter, properties.get());
            
            const gchar* property;
            GVariant* value;
            
            while (g_variant_iter_next(&iter, "{&sv}", &property, &value)) {
                // Create new GVariantPtr with proper ownership
                GVariantPtr valuePtr = makeGVariantPtr(value, true);
                onPropertyChangedCallback(interfaceName, property, std::move(valuePtr));
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in handlePropertiesChangedSignal: " + std::string(e.what()));
    }
}

void ConnectionManager::setOnConnectionCallback(ConnectionCallback callback) {
    onConnectionCallback = callback;
}

void ConnectionManager::setOnDisconnectionCallback(ConnectionCallback callback) {
    onDisconnectionCallback = callback;
}

void ConnectionManager::setOnPropertyChangedCallback(PropertyChangedCallback callback) {
    onPropertyChangedCallback = callback;
}

std::vector<std::string> ConnectionManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    std::vector<std::string> devices;
    
    for (const auto& pair : connectedDevices) {
        devices.push_back(pair.first);
    }
    
    return devices;
}

bool ConnectionManager::isDeviceConnected(const std::string& deviceAddress) const {
    std::lock_guard<std::mutex> lock(devicesMutex);
    return connectedDevices.find(deviceAddress) != connectedDevices.end();
}

} // namespace ggk