// include/Server.h
#pragma once

#include "GattApplication.h"
#include "GattAdvertisement.h"
#include "DBusName.h"
#include "BlueZConstants.h"
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <string>
#include <mutex>

namespace ggk {

/**
 * @brief Main BLE peripheral server class that manages the entire BLE stack
 * 
 * The Server class is responsible for:
 * - Initializing the BLE hardware via BlueZ D-Bus API
 * - Managing GATT services (via GattApplication)
 * - Handling BLE advertisements (via GattAdvertisement)
 * - Maintaining the event loop for BLE events
 * - Managing device connections via ConnectionManager
 */
class Server {
public:
    /**
     * @brief Constructor
     * 
     * Creates a Server instance with default configuration
     */
    Server();
    
    /**
     * @brief Destructor
     * 
     * Ensures proper cleanup of BLE resources
     */
    ~Server();
    
    /**
     * @brief Initializes the BLE stack
     * 
     * Sets up BlueZ, D-Bus connection, GATT application,
     * advertisement objects, and ConnectionManager
     * 
     * @param deviceName The name to advertise for this BLE peripheral
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(const std::string& deviceName = "BLEDevice");
    
    /**
     * @brief Starts the BLE peripheral
     * 
     * Enables the BLE controller, starts advertising, and registers the
     * GATT application with BlueZ
     * 
     * @param secureMode Whether to enable secure connections and pairing
     * @return true if the server started successfully, false otherwise
     */
    bool start(bool secureMode = false);
    
    /**
     * @brief Stops the BLE peripheral
     * 
     * Stops advertising, unregisters from BlueZ, and disables the BLE controller
     */
    void stop();
    
    /**
     * @brief Adds a GATT service to the server
     * 
     * Note: Services can only be added before the server is started.
     * All services, characteristics, and descriptors must be fully
     * configured before starting the server.
     * 
     * @param service Pointer to the service to add
     * @return true if the service was added successfully, false otherwise
     */
    bool addService(GattServicePtr service);
    
    /**
     * @brief Creates a new GATT service
     * 
     * Helper method to create a new service with the appropriate connection and path
     * 
     * @param uuid UUID of the service
     * @param isPrimary Whether this is a primary service
     * @return Shared pointer to the created service
     */
    GattServicePtr createService(const GattUuid& uuid, bool isPrimary = true);
    
    /**
     * @brief Configures the advertisement settings
     * 
     * @param name Local name to advertise (if empty, uses device name)
     * @param serviceUuids List of service UUIDs to include in advertisement
     * @param manufacturerId Manufacturer ID for advertisement data (0 for none)
     * @param manufacturerData Data to include with manufacturer ID
     * @param includeTxPower Whether to include TX power in advertisement
     * @param timeout Advertisement timeout in seconds (0 for no timeout)
     */
    void configureAdvertisement(
        const std::string& name = "",
        const std::vector<GattUuid>& serviceUuids = {},
        uint16_t manufacturerId = 0,
        const std::vector<uint8_t>& manufacturerData = {},
        bool includeTxPower = true,
        uint16_t timeout = 0
    );
    
    /**
     * @brief Runs the BLE event loop
     * 
     * This method blocks until stop() is called
     */
    void run();
    
    /**
     * @brief Non-blocking version that runs the event loop in a separate thread
     */
    void startAsync();
    
    /**
     * @brief Checks if the server is currently running
     * 
     * @return true if the server is running, false otherwise
     */
    bool isRunning() const { return running; }
    
    /**
     * @brief Gets the GATT application
     * 
     * @return Reference to the GATT application
     */
    GattApplication& getApplication() { return *application; }
    
    /**
     * @brief Gets the GATT advertisement
     * 
     * @return Reference to the GATT advertisement
     */
    GattAdvertisement& getAdvertisement() { return *advertisement; }
    
    /**
     * @brief Gets the device name
     * 
     * @return Current device name
     */
    const std::string& getDeviceName() const { return deviceName; }
    
    /**
     * @brief Sets a callback function to be called on client connection
     * 
     * @param callback Function to call when a client connects
     */
    void setConnectionCallback(std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        connectionCallback = callback;
    }
    
    /**
     * @brief Sets a callback function to be called on client disconnection
     * 
     * @param callback Function to call when a client disconnects
     */
    void setDisconnectionCallback(std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(callbacksMutex);
        disconnectionCallback = callback;
    }
    
    /**
     * @brief Gets the list of currently connected devices
     * 
     * @return Vector of device addresses
     */
    std::vector<std::string> getConnectedDevices() const;
    
    /**
     * @brief Checks if a specific device is connected
     * 
     * @param deviceAddress The Bluetooth address to check
     * @return true if the device is connected, false otherwise
     */
    bool isDeviceConnected(const std::string& deviceAddress) const;
    
private:
    // Core BLE components
    std::unique_ptr<GattApplication> application;
    std::unique_ptr<GattAdvertisement> advertisement;
    
    // State management
    std::atomic<bool> running;
    std::atomic<bool> initialized;
    std::thread eventThread;
    std::string deviceName;
    
    // Advertisement settings
    uint16_t advTimeout;
    
    // Callback functions
    std::function<void(const std::string&)> connectionCallback;
    std::function<void(const std::string&)> disconnectionCallback;
    mutable std::mutex callbacksMutex;
    
    // Internal methods
    void eventLoop();
    void setupSignalHandlers();
    void handleConnectionEvent(const std::string& deviceAddress);
    void handleDisconnectionEvent(const std::string& deviceAddress);
    void handlePropertyChangeEvent(const std::string& interface, 
                                  const std::string& property, 
                                  GVariantPtr value);
    
    /**
     * @brief Sets up the BlueZ interface for BLE operation
     * 
     * Resets and configures the Bluetooth adapter with proper settings
     * 
     * @return true if setup was successful, false otherwise
     */
    bool setupBlueZInterface();
    
    /**
     * @brief Fallback methods for enabling advertising
     * 
     * If the standard BlueZ D-Bus API fails, tries alternative
     * methods like hciconfig, bluetoothctl, and direct HCI commands
     * 
     * @return true if any advertising method succeeded, false otherwise
     */
    bool enableAdvertisingFallback();
    
    /**
     * @brief Static method to register for safe shutdown
     * 
     * @param server Pointer to the server instance to shut down
     */
    static void registerShutdownHandler(Server* server);

    /**
     * @brief Restart BlueZ service
     * 
     * @return true if successful, false otherwise
     */
    bool restartBlueZService();
    
    /**
     * @brief Reset Bluetooth adapter
     * 
     * @return true if successful, false otherwise
     */
    bool resetBluetoothAdapter();
};

} // namespace ggk