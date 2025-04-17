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

bool ConnectionManager::initialize(std::shared_ptr<SDBusConnection> dbusConnection) {
    if (initialized) {
        return true;
    }

    connection = dbusConnection;
    registerSignalHandlers();
    initialized = true;

    Logger::info("ConnectionManager 초기화됨");
    return true;
}

bool ConnectionManager::isInitialized() const { 
    return initialized; 
}

void ConnectionManager::shutdown() {
    if (!initialized) {
        return;
    }

    // 신호 핸들러 등록 해제 - v2에서는 다르게 처리
    // sdbus-c++에서는 현재 직접적인 시그널 핸들러 해제를 제공하지 않음
    // signalHandlerIds는 이제 사용하지 않지만 카운트 용도로 유지
    signalHandlerIds.clear();

    // 연결된 장치 목록 정리
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        connectedDevices.clear();
    }

    connection = nullptr;
    initialized = false;
    Logger::info("ConnectionManager 종료됨");
}

void ConnectionManager::registerSignalHandlers() {
    if (!connection || !connection->isConnected()) {
        Logger::error("D-Bus 신호 핸들러를 등록할 수 없음: D-Bus 연결을 사용할 수 없음");
        return;
    }

    try {
        // ObjectManager 신호 프록시 생성
        auto proxy = connection->createProxy(
            sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
            sdbus::ObjectPath{BlueZConstants::ROOT_PATH}
        );

        if (!proxy) {
            Logger::error("Object Manager 프록시 생성 실패");
            return;
        }

        // InterfacesAdded 신호 핸들러
        try {
            // v2에서는 uponSignal을 사용하고, 구체적인 타입으로 매개변수를 선언
            proxy->uponSignal(sdbus::SignalName{"InterfacesAdded"})
                .onInterface(sdbus::InterfaceName{BlueZConstants::OBJECT_MANAGER_INTERFACE})
                .call([this](sdbus::ObjectPath path, 
                            std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces) {
                    // 직접 핸들러 함수 호출, 파라미터를 이미 추출했으므로 바로 전달
                    this->handleInterfacesAddedSignal(path.c_str(), interfaces);
                });
            
            // 핸들러 ID를 저장할 수 없지만, 성공적으로 등록되었음을 추적하기 위해
            // 카운트는 유지합니다.
            signalHandlerIds.push_back(signalHandlerIds.size() + 1);
        } catch (const std::exception& e) {
            Logger::error("InterfacesAdded 신호 등록 실패: " + std::string(e.what()));
        }

        // InterfacesRemoved 신호 핸들러
        try {
            proxy->uponSignal(sdbus::SignalName{"InterfacesRemoved"})
                .onInterface(sdbus::InterfaceName{BlueZConstants::OBJECT_MANAGER_INTERFACE})
                .call([this](sdbus::ObjectPath path, std::vector<std::string> interfaces) {
                    // 직접 핸들러 함수 호출, 파라미터를 이미 추출했으므로 바로 전달
                    this->handleInterfacesRemovedSignal(path.c_str(), interfaces);
                });
            
            signalHandlerIds.push_back(signalHandlerIds.size() + 1);
        } catch (const std::exception& e) {
            Logger::error("InterfacesRemoved 신호 등록 실패: " + std::string(e.what()));
        }

        // PropertiesChanged 신호 핸들러
        try {
            auto propsProxy = connection->createProxy(
                sdbus::ServiceName{BlueZConstants::BLUEZ_SERVICE},
                sdbus::ObjectPath{BlueZConstants::ADAPTER_PATH}
            );

            if (propsProxy) {
                propsProxy->uponSignal(sdbus::SignalName{"PropertiesChanged"})
                    .onInterface(sdbus::InterfaceName{BlueZConstants::PROPERTIES_INTERFACE})
                    .call([this](std::string interfaceName, 
                                std::map<std::string, sdbus::Variant> changedProps,
                                std::vector<std::string> invalidatedProps) {
                        // 직접 핸들러 함수 호출, 파라미터를 이미 추출했으므로 바로 전달
                        this->handlePropertiesChangedSignal(interfaceName, changedProps, invalidatedProps);
                    });
                
                signalHandlerIds.push_back(signalHandlerIds.size() + 1);
            } else {
                Logger::error("PropertiesChanged용 프록시 생성 실패");
            }
        } catch (const std::exception& e) {
            Logger::error("PropertiesChanged 신호 등록 실패: " + std::string(e.what()));
        }

        Logger::info("BlueZ D-Bus 신호 핸들러 등록 완료");
    }
    catch (const std::exception& e) {
        Logger::error("신호 핸들러 등록 중 예외 발생: " + std::string(e.what()));
    }
}


void ConnectionManager::handleInterfacesAddedSignal(
    const std::string& objectPath,
    const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfaces) {
    
    try {
        // Device 인터페이스 확인
        auto it = interfaces.find(BlueZConstants::DEVICE_INTERFACE);
        if (it == interfaces.end()) {
            return; // Device 인터페이스가 없음
        }
        
        // 속성 맵 가져오기
        const auto& deviceProperties = it->second;
        
        // Connected 속성 확인
        auto connectedIt = deviceProperties.find("Connected");
        if (connectedIt != deviceProperties.end()) {
            try {
                bool connected = connectedIt->second.get<bool>();
                if (connected) {
                    // Address 속성 가져오기
                    auto addressIt = deviceProperties.find("Address");
                    if (addressIt != deviceProperties.end()) {
                        std::string deviceAddress = addressIt->second.get<std::string>();
                        
                        // 연결된 장치 목록에 추가
                        {
                            std::lock_guard<std::mutex> lock(devicesMutex);
                            connectedDevices[deviceAddress] = objectPath;
                        }
                        
                        // 콜백 실행
                        if (onConnectionCallback) {
                            onConnectionCallback(deviceAddress);
                        }
                        
                        Logger::info("장치 연결됨: " + deviceAddress + ", 경로: " + objectPath);
                    }
                }
            }
            catch (const std::exception& e) {
                Logger::error("장치 연결 처리 중 예외: " + std::string(e.what()));
            }
        }
    } catch (const std::exception& e) {
        Logger::error("InterfacesAdded 신호 처리 중 예외: " + std::string(e.what()));
    }
}

void ConnectionManager::handleInterfacesRemovedSignal(
    const std::string& objectPath,
    const std::vector<std::string>& interfaces) {
    
    try {
        // Device 인터페이스 확인
        bool deviceRemoved = false;
        for (const auto& interface : interfaces) {
            if (interface == BlueZConstants::DEVICE_INTERFACE) {
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
                    if (it->second == objectPath) {
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
                Logger::info("장치 연결 해제됨: " + deviceAddress);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("InterfacesRemoved 신호 처리 중 예외: " + std::string(e.what()));
    }
}

void ConnectionManager::handlePropertiesChangedSignal(
    const std::string& interfaceName,
    const std::map<std::string, sdbus::Variant>& changedProperties,
    const std::vector<std::string>& invalidatedProperties) {
    
    try {
        // Device 인터페이스의 속성 변경 처리
        if (interfaceName == BlueZConstants::DEVICE_INTERFACE) {
            // Connected 속성 확인
            auto connectedIt = changedProperties.find("Connected");
            if (connectedIt != changedProperties.end()) {
                // InterfacesAdded/Removed 신호로 이미 처리되므로 여기서는 중복 처리 방지
            }
        }
        
        // 속성 변경 콜백 실행
        if (onPropertyChangedCallback) {
            for (const auto& [property, value] : changedProperties) {
                onPropertyChangedCallback(interfaceName, property, value);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("PropertiesChanged 신호 처리 중 예외: " + std::string(e.what()));
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