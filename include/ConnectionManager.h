#pragma once

#include "DBusInterface.h"
#include "DBusObjectPath.h"
#include "BlueZConstants.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>

namespace ggk {

/**
 * @brief Class for managing BLE device connections
 * 
 * Monitors and manages connected devices via BlueZ D-Bus signals
 */
class ConnectionManager {
public:
    // Callback types
    using ConnectionCallback = std::function<void(const std::string& deviceAddress)>;
    using PropertyChangedCallback = std::function<void(const std::string& interface, 
                                                      const std::string& property, 
                                                      GVariantPtr value)>;

    /**
     * @brief Get singleton instance
     * 
     * @return Reference to singleton instance
     */
    static ConnectionManager& getInstance();

    /**
     * @brief Initialize manager
     * 
     * @param connection D-Bus connection
     * @return Success status
     */
    bool initialize(std::shared_ptr<IDBusConnection> connection);
    
    /**
     * @brief Shutdown manager
     */
    void shutdown();

    /**
     * @brief Set connection callback
     * 
     * @param callback Function to call on connection
     */
    void setOnConnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief Set disconnection callback
     * 
     * @param callback Function to call on disconnection
     */
    void setOnDisconnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief Set property changed callback
     * 
     * @param callback Function to call on property changes
     */
    void setOnPropertyChangedCallback(PropertyChangedCallback callback);

    /**
     * @brief Get connected devices
     * 
     * @return List of connected device addresses
     */
    std::vector<std::string> getConnectedDevices() const;
    
    /**
     * @brief Check if device is connected
     * 
     * @param deviceAddress Device address to check
     * @return Connection status
     */
    bool isDeviceConnected(const std::string& deviceAddress) const;
    
    /**
     * @brief Check if manager is initialized
     * 
     * @return Initialization status
     */
    bool isInitialized() const;

private:
    ConnectionManager();
    ~ConnectionManager();

    // Prevent copying
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // Signal handling
    void registerSignalHandlers();
    void handleInterfacesAddedSignal(const std::string& signalName, GVariantPtr parameters);
    void handleInterfacesRemovedSignal(const std::string& signalName, GVariantPtr parameters);
    void handlePropertiesChangedSignal(const std::string& signalName, GVariantPtr parameters);

    // Connection data
    std::map<std::string, DBusObjectPath> connectedDevices;
    mutable std::mutex devicesMutex;

    // Callbacks
    ConnectionCallback onConnectionCallback;
    ConnectionCallback onDisconnectionCallback;
    PropertyChangedCallback onPropertyChangedCallback;

    // Signal handling
    std::shared_ptr<IDBusConnection> connection;
    std::vector<guint> signalHandlerIds;
    bool initialized;
};

} // namespace ggk