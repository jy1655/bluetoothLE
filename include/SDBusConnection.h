#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace ggk {

/**
 * @brief Wrapper around sdbus::IConnection
 *
 * Provides a boundary between application code and sdbus-c++ library
 */
class SDBusConnection {
public:
    /**
     * @brief Constructor for system or session bus
     * 
     * @param useSystemBus Whether to use system bus (true) or session bus (false)
     */
    explicit SDBusConnection(bool useSystemBus = true);
    
    /**
     * @brief Destructor
     */
    ~SDBusConnection();
    
    /**
     * @brief Connect to the bus
     * 
     * @return true if connection was successful
     */
    bool connect();
    
    /**
     * @brief Disconnect from the bus
     */
    void disconnect();
    
    /**
     * @brief Check if connected to the bus
     * 
     * @return true if connected
     */
    bool isConnected();
    
    /**
     * @brief Get the underlying sdbus connection
     * 
     * @return Reference to the connection
     */
    sdbus::IConnection& getSdbusConnection();
    
    /**
     * @brief Create a proxy for remote D-Bus object
     * 
     * @param destination Service name
     * @param objectPath Object path
     * @return Unique pointer to sdbus::IProxy
     */
    std::unique_ptr<sdbus::IProxy> createProxy(
        const std::string& destination,
        const std::string& objectPath);
    
    /**
     * @brief Create an object for local D-Bus server
     * 
     * @param objectPath Object path
     * @return Unique pointer to sdbus::IObject
     */
    std::unique_ptr<sdbus::IObject> createObject(
        const std::string& objectPath);
    
    /**
     * @brief Request a service name on the bus
     * 
     * @param serviceName Name to request
     * @return true if name was successfully acquired
     */
    bool requestName(const std::string& serviceName);
    
    /**
     * @brief Release a previously requested service name
     * 
     * @param serviceName Name to release
     * @return true if name was successfully released
     */
    bool releaseName(const std::string& serviceName);
    
    /**
     * @brief Enter the event loop to process messages
     * 
     * The event loop will block until the connection is closed or 
     * enterEventLoop() is called from another thread.
     */
    void enterEventLoop();
    
    /**
     * @brief Leave the event loop
     */
    void leaveEventLoop();

private:
    std::unique_ptr<sdbus::IConnection> connection;
    bool connected;
    std::mutex connectionMutex;
};

} // namespace ggk