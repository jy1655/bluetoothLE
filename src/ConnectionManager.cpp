// src/ConnectionManager.cpp
#include "ConnectionManager.h"
#include "Logger.h"

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

void ConnectionManager::shutdown() {
    if (!initialized) {
        return;
    }

    // 시그널 핸들러 해제
    for (guint handlerId : signalHandlerIds) {
        if (connection) {
            connection->removeSignalWatch(handlerId);
        }
    }
    signalHandlerIds.clear();

    // 연결된 장치 목록 초기화
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

    // 1. InterfacesAdded 시그널 - 새 장치 연결 감지
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

    // 2. InterfacesRemoved 시그널 - 장치 연결 해제 감지
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

    // 3. PropertiesChanged 시그널 - 속성 변경 감지 (연결 상태, MTU 등)
    guint id3 = connection->addSignalWatch(
        BlueZConstants::BLUEZ_SERVICE,
        BlueZConstants::PROPERTIES_INTERFACE,
        "PropertiesChanged",
        DBusObjectPath(""),  // 모든 객체 경로 감시
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
        // parameters 형식: (o, a{sa{sv}})
        // o - 객체 경로
        // a{sa{sv}} - 인터페이스 -> 속성 맵
        const char* objectPath = nullptr;
        GVariant* interfacesVariant = nullptr;
        g_variant_get(parameters.get(), "(&o@a{sa{sv}})", &objectPath, &interfacesVariant);

        if (!objectPath || !interfacesVariant) {
            return;
        }

        GVariantPtr interfaces(interfacesVariant, &g_variant_unref);

        // Device 인터페이스 확인
        GVariant* deviceInterface = g_variant_lookup_value(interfaces.get(), 
                                                         BlueZConstants::DEVICE_INTERFACE.c_str(), 
                                                         G_VARIANT_TYPE("a{sv}"));
        if (deviceInterface) {
            // Connected 속성 확인
            GVariant* connectedVariant = g_variant_lookup_value(deviceInterface, 
                                                             "Connected", 
                                                             G_VARIANT_TYPE_BOOLEAN);
            
            if (connectedVariant) {
                bool connected = g_variant_get_boolean(connectedVariant);
                g_variant_unref(connectedVariant);
                
                if (connected) {
                    // 주소 속성 가져오기
                    GVariant* addressVariant = g_variant_lookup_value(deviceInterface, 
                                                                  "Address", 
                                                                  G_VARIANT_TYPE_STRING);
                    
                    if (addressVariant) {
                        const char* address = g_variant_get_string(addressVariant, nullptr);
                        std::string deviceAddress(address);
                        
                        // 연결된 장치 추가
                        {
                            std::lock_guard<std::mutex> lock(devicesMutex);
                            connectedDevices[deviceAddress] = DBusObjectPath(objectPath);
                        }
                        
                        // 콜백 호출
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
        // parameters 형식: (o, as)
        // o - 객체 경로
        // as - 제거된 인터페이스 목록
        const char* objectPath = nullptr;
        GVariant* interfacesVariant = nullptr;
        g_variant_get(parameters.get(), "(&o@as)", &objectPath, &interfacesVariant);

        if (!objectPath || !interfacesVariant) {
            return;
        }

        GVariantPtr interfaces(interfacesVariant, &g_variant_unref);

        // 제거된 인터페이스 중에 Device 인터페이스가 있는지 확인
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
            // 연결 해제된 장치 찾기
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
        // parameters 형식: (s, a{sv}, as)
        // s - 인터페이스 이름
        // a{sv} - 변경된 속성과 값
        // as - 무효화된 속성
        const char* interface = nullptr;
        GVariant* propertiesVariant = nullptr;
        GVariant* invalidatedVariant = nullptr;
        
        g_variant_get(parameters.get(), "(&s@a{sv}@as)", 
                     &interface, &propertiesVariant, &invalidatedVariant);

        if (!interface || !propertiesVariant) {
            return;
        }

        GVariantPtr properties(propertiesVariant, &g_variant_unref);
        GVariantPtr invalidated(invalidatedVariant, &g_variant_unref);

        std::string interfaceName(interface);

        // Device 인터페이스의 Connected 속성 변화 감지
        if (interfaceName == BlueZConstants::DEVICE_INTERFACE) {
            GVariant* connectedVariant = g_variant_lookup_value(properties.get(), 
                                                             "Connected", 
                                                             G_VARIANT_TYPE_BOOLEAN);
            
            if (connectedVariant) {
                // TODO: 여기서 연결 상태 변화 처리
                // 현재는 InterfacesAdded/Removed에서 처리 중이므로 중복 처리를 방지하기 위해 생략
                g_variant_unref(connectedVariant);
            }
        }

        // 속성 변경 콜백 호출
        if (onPropertyChangedCallback) {
            // 모든 속성에 대해 개별적으로 콜백 호출
            GVariantIter iter;
            g_variant_iter_init(&iter, properties.get());
            
            const gchar* property;
            GVariant* value;
            
            while (g_variant_iter_next(&iter, "{&sv}", &property, &value)) {
                GVariantPtr valuePtr(value, &g_variant_unref);
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