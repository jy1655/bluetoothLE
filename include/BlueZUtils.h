#pragma once

#include <string>
#include <vector>
#include <tuple>
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include "GattTypes.h"
#include "DBusTypes.h"

namespace ggk {

/**
 * @brief Utility class for BlueZ operations using D-Bus API
 * 
 * This class provides helper methods to perform common BlueZ operations
 * using the D-Bus API instead of command-line tools like hciconfig.
 */
class BlueZUtils {
public:
    /**
     * @brief Get BlueZ version
     * 
     * @return std::tuple<int, int, int> Version as (major, minor, patch)
     */
    static std::tuple<int, int, int> getBlueZVersion();
    
    /**
     * @brief Check if required BlueZ packages are installed
     * 
     * @return true if all required packages are installed
     */
    static bool checkRequiredPackages();
    
    /**
     * @brief Check if BlueZ version is sufficient
     * 
     * @return true if BlueZ version meets the minimum requirements
     */
    static bool checkBlueZVersion();
    
    /**
     * @brief Restart BlueZ service
     * 
     * @return true if service was successfully restarted
     */
    static bool restartBlueZService();
    
    /**
     * @brief Get list of available Bluetooth adapters
     * 
     * @param connection D-Bus connection to use
     * @return std::vector<std::string> List of adapter paths
     */
    static std::vector<std::string> getAdapters(DBusConnection& connection);
    
    /**
     * @brief Get default adapter path
     * 
     * @param connection D-Bus connection to use
     * @return std::string Path to default adapter (usually /org/bluez/hci0)
     */
    static std::string getDefaultAdapter(DBusConnection& connection);
    
    /**
     * @brief Check if adapter is powered on
     * 
     * @param connection D-Bus connection to use
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if adapter is powered on
     */
    static bool isAdapterPowered(DBusConnection& connection, 
                                const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Power on/off an adapter
     * 
     * @param connection D-Bus connection to use
     * @param powered true to power on, false to power off
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if operation was successful
     */
    static bool setAdapterPower(DBusConnection& connection, 
                               bool powered, 
                               const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Set adapter discoverable state
     * 
     * @param connection D-Bus connection to use
     * @param discoverable true to make discoverable, false otherwise
     * @param timeout Discoverable timeout in seconds (0 = no timeout)
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if operation was successful
     */
    static bool setAdapterDiscoverable(DBusConnection& connection, 
                                      bool discoverable,
                                      uint16_t timeout = 0,
                                      const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Set adapter name
     * 
     * @param connection D-Bus connection to use
     * @param name New adapter name
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if operation was successful
     */
    static bool setAdapterName(DBusConnection& connection, 
                              const std::string& name,
                              const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Reset adapter to default state
     * 
     * @param connection D-Bus connection to use
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if operation was successful
     */
    static bool resetAdapter(DBusConnection& connection,
                            const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Initialize adapter for BLE peripheral mode
     * 
     * @param connection D-Bus connection to use
     * @param name Name to set for the adapter
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if initialization was successful
     */
    static bool initializeAdapter(DBusConnection& connection,
                                 const std::string& name,
                                 const std::string& adapterPath = "/org/bluez/hci0");
    
    /**
     * @brief Create a BlueZ script for bluetoothctl
     * 
     * Creates a shell script that can be executed to run bluetoothctl commands
     * 
     * @param commands List of bluetoothctl commands
     * @return std::string Script content
     */
    static std::string createBluetoothCtlScript(const std::vector<std::string>& commands);
    
    /**
     * @brief Run bluetoothctl commands
     * 
     * @param commands List of bluetoothctl commands
     * @return true if all commands executed successfully
     */
    static bool runBluetoothCtlCommands(const std::vector<std::string>& commands);

    /**
     * @brief Get connected devices
     * 
     * @param connection D-Bus connection to use
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return std::vector<std::string> List of connected device paths
     */
    static std::vector<std::string> getConnectedDevices(
        DBusConnection& connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief Check if advertising is supported
     * 
     * @param connection D-Bus connection to use
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if advertising is supported
     */
    static bool isAdvertisingSupported(
        DBusConnection& connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief Try enabling advertising using various methods
     * 
     * Falls back through different methods:
     * 1. D-Bus API
     * 2. bluetoothctl
     * 3. Direct D-Bus property manipulation
     * 
     * @param connection D-Bus connection to use
     * @param adapterPath Adapter path (default: /org/bluez/hci0)
     * @return true if advertising was enabled
     */
    static bool tryEnableAdvertising(
        DBusConnection& connection,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    /**
     * @brief Check if required BlueZ features are available
     * 
     * @param connection D-Bus connection to use
     * @return true if all required features are available
     */
    static bool checkBlueZFeatures(DBusConnection& connection);


    // Helper for getting adapter properties
    static GVariantPtr getAdapterProperty(
        DBusConnection& connection,
        const std::string& property,
        const std::string& adapterPath = "/org/bluez/hci0");
        
    // Helper for setting adapter properties
    static bool setAdapterProperty(
        DBusConnection& connection,
        const std::string& property,
        GVariantPtr value,
        const std::string& adapterPath = "/org/bluez/hci0");
};

} // namespace ggk