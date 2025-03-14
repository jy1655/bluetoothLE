// include/ConnectionManager.h
#pragma once

#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "BlueZConstants.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>

namespace ggk {

class ConnectionManager {
public:
    // 이벤트 콜백 타입 정의
    using ConnectionCallback = std::function<void(const std::string& deviceAddress)>;
    using PropertyChangedCallback = std::function<void(const std::string& interface, 
                                                      const std::string& property, 
                                                      GVariantPtr value)>;

    static ConnectionManager& getInstance();

    // 초기화 및 정리
    bool initialize(DBusConnection& connection);
    void shutdown();

    // 이벤트 리스너 등록
    void setOnConnectionCallback(ConnectionCallback callback);
    void setOnDisconnectionCallback(ConnectionCallback callback);
    void setOnPropertyChangedCallback(PropertyChangedCallback callback);

    // 연결 관리
    std::vector<std::string> getConnectedDevices() const;
    bool isDeviceConnected(const std::string& deviceAddress) const;

private:
    ConnectionManager();
    ~ConnectionManager();

    // 복사 방지
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 시그널 핸들러 등록
    void registerSignalHandlers();
    void handleInterfacesAddedSignal(const std::string& signalName, GVariantPtr parameters);
    void handleInterfacesRemovedSignal(const std::string& signalName, GVariantPtr parameters);
    void handlePropertiesChangedSignal(const std::string& signalName, GVariantPtr parameters);

    // 실제 연결 데이터
    std::map<std::string, DBusObjectPath> connectedDevices;
    mutable std::mutex devicesMutex;

    // 콜백
    ConnectionCallback onConnectionCallback;
    ConnectionCallback onDisconnectionCallback;
    PropertyChangedCallback onPropertyChangedCallback;

    // 시그널 처리 관련
    DBusConnection* connection;
    std::vector<guint> signalHandlerIds;
    bool initialized;
};

} // namespace ggk