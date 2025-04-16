#pragma once

#include <atomic>
#include <string>
#include "DBusInterface.h"
#include "Logger.h"

namespace ggk {

/**
 * @brief Modern HCI Adapter class for BlueZ 5.82+
 * 
 * This class manages the Bluetooth HCI adapter using modern
 * BlueZ D-Bus APIs instead of direct HCI control. No direct
 * socket communication is used - everything is via D-Bus.
 */
class HciAdapter {
public:
    /**
     * @brief Default constructor
     */
    HciAdapter();
    
    /**
     * @brief Destructor
     */
    ~HciAdapter();
    
    /**
     * @brief Initialize the adapter
     * 
     * @param connection D-Bus connection to use
     * @param adapterName Name to set for the adapter
     * @param adapterPath Path to the adapter (default: /org/bluez/hci0)
     * @return true if initialization was successful
     */
    bool initialize(
        std::shared_ptr<IDBusConnection> connection,
        const std::string& adapterName = "BluetoothDevice",
        const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Stop and clean up
     */
    void stop();
    
    /**
     * @brief Check if adapter is initialized
     * 
     * @return true if adapter is initialized
     */
    bool isInitialized() const;
    
    /**
     * @brief Get adapter path
     * 
     * @return Adapter path (e.g., /org/bluez/hci0)
     */
    const std::string& getAdapterPath() const;
    
    /**
     * @brief Set adapter name
     * 
     * @param name New name for the adapter
     * @return true if successful
     */
    bool setName(const std::string& name);
    
    /**
     * @brief Set adapter power state
     * 
     * @param powered true to power on, false to power off
     * @return true if successful
     */
    bool setPowered(bool powered);
    
    /**
     * @brief Set adapter discoverable state
     * 
     * @param discoverable true to make discoverable, false otherwise
     * @param timeout Timeout in seconds (0 for no timeout)
     * @return true if successful
     */
    bool setDiscoverable(bool discoverable, uint16_t timeout = 0);
    
    /**
     * @brief Reset adapter to default state
     * 
     * @return true if successful
     */
    bool reset();
    
    /**
     * @brief Enable advertising
     * 
     * @return true if successful
     */
    bool enableAdvertising();
    
    /**
     * @brief Disable advertising
     * 
     * @return true if successful
     */
    bool disableAdvertising();
    
    /**
     * @brief Check if the adapter supports advertising
     * 
     * @return true if advertising is supported
     */
    bool isAdvertisingSupported() const;
    
    /**
     * @brief Get the D-Bus connection
     * 
     * @return Reference to the D-Bus connection
     */
    std::shared_ptr<IDBusConnection> getConnection();
    
private:
    // D-Bus connection
    std::shared_ptr<IDBusConnection> connection;
    
    // Adapter state
    std::string adapterPath;
    std::string adapterName;
    bool initialized;
    bool advertising;
    
    // Helper methods
    bool verifyAdapterExists();
    bool runBluetoothCtlCommand(const std::vector<std::string>& commands);
};

} // namespace ggk