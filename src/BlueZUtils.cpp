#include "BlueZUtils.h"
#include "BlueZConstants.h"
#include "Logger.h"
#include "Utils.h"
#include <unistd.h>
#include <regex>
#include <sstream>
#include <cstring>
#include <filesystem>

namespace ggk {

std::tuple<int, int, int> BlueZUtils::getBlueZVersion() {
    int major = 0, minor = 0, patch = 0;
    
    // Try to get version from bluetoothd --version
    FILE* pipe = popen("bluetoothd --version 2>/dev/null", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string version(buffer);
            
            // Parse with regex
            std::regex pattern(R"((\d+)\.(\d+)(?:\.(\d+))?)");
            std::smatch matches;
            
            if (std::regex_search(version, matches, pattern) && matches.size() > 2) {
                major = std::stoi(matches[1].str());
                minor = std::stoi(matches[2].str());
                
                if (matches.size() > 3 && matches[3].matched) {
                    patch = std::stoi(matches[3].str());
                }
            }
        }
        pclose(pipe);
    }
    
    // Fallback: try to get version from package info
    if (major == 0) {
        pipe = popen("dpkg-query -W -f='${Version}' bluez 2>/dev/null", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string version(buffer);
                
                // Parse with regex (package versions may have additional info)
                std::regex pattern(R"((\d+)\.(\d+)(?:\.(\d+))?)");
                std::smatch matches;
                
                if (std::regex_search(version, matches, pattern) && matches.size() > 2) {
                    major = std::stoi(matches[1].str());
                    minor = std::stoi(matches[2].str());
                    
                    if (matches.size() > 3 && matches[3].matched) {
                        patch = std::stoi(matches[3].str());
                    }
                }
            }
            pclose(pipe);
        }
    }
    
    return std::make_tuple(major, minor, patch);
}

bool BlueZUtils::checkRequiredPackages() {
    bool hasRequiredPackages = true;
    
    // Check for essential packages
    if (!Utils::isPackageInstalled("bluez")) {
        Logger::error("bluez package is not installed");
        hasRequiredPackages = false;
    }
    
    if (!Utils::isPackageInstalled("libbluetooth-dev")) {
        Logger::error("libbluetooth-dev package is not installed");
        hasRequiredPackages = false;
    }
    
    if (!Utils::isPackageInstalled("libglib2.0-dev")) {
        Logger::error("libglib2.0-dev package is not installed");
        hasRequiredPackages = false;
    }
    
    // Check for command-line tools
    if (!Utils::isCommandAvailable("bluetoothctl")) {
        Logger::error("bluetoothctl tool is not available");
        hasRequiredPackages = false;
    }
    
    if (!Utils::isCommandAvailable("bluetoothd")) {
        Logger::error("bluetoothd daemon is not available");
        hasRequiredPackages = false;
    }
    
    return hasRequiredPackages;
}

bool BlueZUtils::checkBlueZVersion() {
    auto [major, minor, patch] = getBlueZVersion();
    
    Logger::info("Detected BlueZ version: " + 
                std::to_string(major) + "." + 
                std::to_string(minor) + "." + 
                std::to_string(patch));
    
    if (major < BlueZConstants::BLUEZ_MINIMUM_VERSION || 
        (major == BlueZConstants::BLUEZ_MINIMUM_VERSION && 
         minor < BlueZConstants::BLUEZ_MINIMUM_MINOR_VERSION)) {
        Logger::error("BlueZ version is too old. Minimum required: " + 
                     BlueZConstants::BLUEZ_VERSION_REQUIREMENT);
        return false;
    }
    
    return true;
}

bool BlueZUtils::restartBlueZService() {
    Logger::info("Restarting BlueZ service...");
    
    // Stop the service
    int result = system("systemctl stop bluetooth.service");
    if (result != 0) {
        Logger::warn("Failed to stop bluetooth service");
    }
    
    // Give it a moment to fully stop
    usleep(500000);  // 500ms
    
    // Start the service
    result = system("systemctl start bluetooth.service");
    if (result != 0) {
        Logger::error("Failed to start bluetooth service");
        return false;
    }
    
    // Wait for the service to fully start
    usleep(1000000);  // 1s
    
    // Check if service is running
    if (!Utils::isBlueZServiceRunning()) {
        Logger::error("BlueZ service is not running after restart");
        return false;
    }
    
    Logger::info("BlueZ service restarted successfully");
    return true;
}

std::vector<std::string> BlueZUtils::getAdapters(std::shared_ptr<SDBusConnection> connection) {
    std::vector<std::string> adapters;
    
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot get adapters: invalid D-Bus connection");
        return adapters;
    }
    
    try {
        // Create proxy to ObjectManager interface
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            BlueZConstants::ROOT_PATH
        );
        
        if (!proxy) {
            Logger::error("Failed to create ObjectManager proxy");
            return adapters;
        }

        // Extract result - a{oa{sa{sv}}}
        std::map<sdbus::ObjectPath, 
                std::map<std::string, 
                std::map<std::string, sdbus::Variant>>> managedObjects;
        
        // Call GetManagedObjects method
        proxy->callMethod(BlueZConstants::GET_MANAGED_OBJECTS)
            .onInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE).storeResultsTo(managedObjects);
        
        
        // Find objects with the Adapter1 interface
        for (const auto& [objectPath, interfaces] : managedObjects) {
            if (interfaces.find(BlueZConstants::ADAPTER_INTERFACE) != interfaces.end()) {
                adapters.push_back(objectPath);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getAdapters: " + std::string(e.what()));
    }
    
    return adapters;
}

std::string BlueZUtils::getDefaultAdapter(std::shared_ptr<SDBusConnection> connection) {
    std::vector<std::string> adapters = getAdapters(connection);
    
    if (adapters.empty()) {
        Logger::error("No Bluetooth adapters found");
        return "";
    }
    
    // Usually, the first adapter (hci0) is the default
    return adapters[0];
}

sdbus::Variant BlueZUtils::getAdapterProperty(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& property,
    const std::string& adapterPath)
{
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot get adapter property: invalid D-Bus connection");
        return sdbus::Variant();
    }
    
    try {
        // Create proxy for adapter
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            adapterPath
        );
        
        if (!proxy) {
            Logger::error("Failed to create adapter proxy");
            return sdbus::Variant();
        }

        // Extract variant
        sdbus::Variant value;
        
        // Call Get method on Properties interface
        proxy->callMethod("Get")
            .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
            .withArguments(std::string(BlueZConstants::ADAPTER_INTERFACE), property)
            .storeResultsTo(value);
        
        return value;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getAdapterProperty: " + std::string(e.what()));
        return sdbus::Variant();
    }
}

bool BlueZUtils::setAdapterProperty(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& property,
    const sdbus::Variant& value,
    const std::string& adapterPath)
{
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot set adapter property: invalid D-Bus connection");
        return false;
    }
    
    try {
        // Create proxy for adapter
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            adapterPath
        );
        
        if (!proxy) {
            Logger::error("Failed to create adapter proxy");
            return false;
        }
        
        // Call Set method on Properties interface
        proxy->callMethod("Set")
            .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
            .withArguments(std::string(BlueZConstants::ADAPTER_INTERFACE), property, value);
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in setAdapterProperty: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::isAdapterPowered(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& adapterPath)
{
    try {
        sdbus::Variant result = getAdapterProperty(
            connection, 
            BlueZConstants::ADAPTER_PROPERTY_POWERED, 
            adapterPath
        );
        
        if (result.isEmpty()) {
            return false;
        }
        
        return result.get<bool>();
    }
    catch (const std::exception& e) {
        Logger::error("Exception in isAdapterPowered: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::setAdapterPower(
    std::shared_ptr<SDBusConnection> connection,
    bool powered,
    const std::string& adapterPath)
{
    Logger::info(std::string("Setting adapter power to: ") + (powered ? "on" : "off"));
    
    try {
        // Set the Powered property
        sdbus::Variant value(powered);
        bool success = setAdapterProperty(
            connection,
            BlueZConstants::ADAPTER_PROPERTY_POWERED,
            value,
            adapterPath
        );
        
        if (success) {
            // Wait a moment for the power state to change
            usleep(500000);  // 500ms
            
            // Verify the change
            bool actualPowered = isAdapterPowered(connection, adapterPath);
            if (actualPowered != powered) {
                Logger::warn("Adapter power state did not change as expected");
                return false;
            }
        }
        
        return success;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in setAdapterPower: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::setAdapterDiscoverable(
    std::shared_ptr<SDBusConnection> connection,
    bool discoverable,
    uint16_t timeout,
    const std::string& adapterPath)
{
    Logger::info(std::string("Setting adapter discoverable to: ") + 
                 (discoverable ? "on" : "off"));
    
    try {
        // Set discoverable property
        sdbus::Variant discValue(discoverable);
        bool success = setAdapterProperty(
            connection,
            BlueZConstants::ADAPTER_PROPERTY_DISCOVERABLE,
            discValue,
            adapterPath
        );
        
        if (!success) {
            return false;
        }
        
        // Set timeout if discoverable is enabled
        if (discoverable && timeout > 0) {
            sdbus::Variant timeoutValue(timeout);
            success = setAdapterProperty(
                connection,
                BlueZConstants::ADAPTER_PROPERTY_DISCOVERABLE_TIMEOUT,
                timeoutValue,
                adapterPath
            );
        }
        
        return success;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in setAdapterDiscoverable: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::setAdapterName(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& name,
    const std::string& adapterPath)
{
    Logger::info("Setting adapter alias to: " + name);
    
    try {
        // In BlueZ 5.82, Name property is read-only, use Alias instead
        sdbus::Variant value(name);
        
        // Set the Alias property
        bool success = setAdapterProperty(
            connection,
            "Alias", // use Alias instead of Name
            value,
            adapterPath
        );
        
        if (success) {
            Logger::info("Successfully set adapter alias");
            return true;
        } else {
            Logger::warn("Failed to set adapter alias, continuing anyway");
            return false;
        }
    }
    catch (const std::exception& e) {
        Logger::warn("Exception in setAdapterName: " + std::string(e.what()) + 
                    ". Continuing anyway.");
        return false;
    }
}

bool BlueZUtils::resetAdapter(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& adapterPath)
{
    Logger::info("Resetting adapter: " + adapterPath);
    
    try {
        // Power off
        if (!setAdapterPower(connection, false, adapterPath)) {
            Logger::warn("Failed to power off adapter during reset");
        }
        
        // Wait a moment
        usleep(500000);  // 500ms
        
        // Power on
        if (!setAdapterPower(connection, true, adapterPath)) {
            Logger::error("Failed to power on adapter after reset");
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in resetAdapter: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::initializeAdapter(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& name,
    const std::string& adapterPath)
{
    Logger::info("Initializing adapter for BLE peripheral mode");
    
    try {
        // Check if adapter exists
        if (!Utils::isBluetoothAdapterAvailable()) {
            Logger::error("Bluetooth adapter not available");
            return false;
        }
        
        // Reset adapter
        if (!resetAdapter(connection, adapterPath)) {
            Logger::error("Failed to reset adapter");
            return false;
        }
        
        // Set name
        if (!name.empty() && !setAdapterName(connection, name, adapterPath)) {
            Logger::warn("Failed to set adapter name");
            // Continue anyway, not critical
        }
        
        // Set discoverable
        if (!setAdapterDiscoverable(connection, true, 0, adapterPath)) {
            Logger::warn("Failed to set adapter as discoverable");
            // Continue anyway, not critical
        }
        
        Logger::info("Adapter initialization complete");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in initializeAdapter: " + std::string(e.what()));
        return false;
    }
}

std::string BlueZUtils::createBluetoothCtlScript(const std::vector<std::string>& commands) {
    std::stringstream script;
    
    script << "#!/bin/bash\n";
    script << "# Auto-generated bluetoothctl script\n\n";
    script << "(\n";
    script << "  sleep 0.5\n";  // Give bluetoothctl time to start
    
    for (const auto& cmd : commands) {
        script << "  echo \"" << cmd << "\"\n";
        script << "  sleep 0.1\n";  // Small delay between commands
    }
    
    script << "  echo \"quit\"\n";
    script << ") | bluetoothctl\n";
    
    return script.str();
}

bool BlueZUtils::runBluetoothCtlCommands(const std::vector<std::string>& commands) {
    std::string script = createBluetoothCtlScript(commands);
    return Utils::executeScript(script);
}

std::vector<std::string> BlueZUtils::getConnectedDevices(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& adapterPath)
{
    std::vector<std::string> connectedDevices;
    
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot get connected devices: invalid D-Bus connection");
        return connectedDevices;
    }
    
    try {
        // Create proxy to ObjectManager interface
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            BlueZConstants::ROOT_PATH
        );
        
        if (!proxy) {
            Logger::error("Failed to create ObjectManager proxy");
            return connectedDevices;
        }

        std::map<sdbus::ObjectPath, 
                std::map<std::string, 
                std::map<std::string, sdbus::Variant>>> managedObjects;
        
        // Call GetManagedObjects method
        proxy->callMethod(BlueZConstants::GET_MANAGED_OBJECTS)
            .onInterface(BlueZConstants::OBJECT_MANAGER_INTERFACE).storeResultsTo(managedObjects);
        
        // Find device objects under this adapter with Connected=true
        for (const auto& [objectPath, interfaces] : managedObjects) {
            // Check if this is a device under our adapter
            std::string pathStr = objectPath;
            if (pathStr.find(adapterPath) != 0) {
                continue;
            }
            
            // Check if this object has the Device1 interface
            auto deviceIt = interfaces.find(BlueZConstants::DEVICE_INTERFACE);
            if (deviceIt == interfaces.end()) {
                continue;
            }
            
            // Check if device is connected
            const auto& deviceProps = deviceIt->second;
            auto connectedIt = deviceProps.find(BlueZConstants::PROPERTY_CONNECTED);
            
            if (connectedIt != deviceProps.end() && connectedIt->second.get<bool>()) {
                connectedDevices.push_back(pathStr);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getConnectedDevices: " + std::string(e.what()));
    }
    
    return connectedDevices;
}

bool BlueZUtils::isAdvertisingSupported(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& adapterPath)
{
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot check advertising support: invalid D-Bus connection");
        return false;
    }
    
    try {
        // Create proxy for adapter
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            adapterPath
        );
        
        if (!proxy) {
            Logger::error("Failed to create adapter proxy");
            return false;
        }
        
        // Try to get properties of LEAdvertisingManager1 interface
        proxy->callMethod("GetAll")
            .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
            .withArguments(std::string(BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE));
        
        // If we got here without exception, the interface is supported
        return true;
    }
    catch (const std::exception& e) {
        // Exception is expected if interface doesn't exist
        Logger::debug("Advertising not supported: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::tryEnableAdvertising(
    std::shared_ptr<SDBusConnection> connection,
    const std::string& adapterPath)
{
    Logger::info("Attempting to enable BLE advertising");
    
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot enable advertising: invalid D-Bus connection");
        return false;
    }
    
    // Track success for each method
    struct {
        bool dbus = false;
        bool bluetoothctl = false;
        bool hciconfig = false;
        bool direct = false;
    } methods;
    
    // Method 1: Try using bluetoothctl (most reliable)
    Logger::debug("Method 1: Enabling advertising via bluetoothctl");
    try {
        methods.bluetoothctl = runBluetoothCtlCommands({
            "menu advertise",
            "on",
            "back"
        });
        
        if (methods.bluetoothctl) {
            Logger::info("Successfully enabled advertising via bluetoothctl");
            return true;
        }
    } catch (...) {
        Logger::warn("bluetoothctl method failed with exception");
    }
    
    // Method 2: Try using hciconfig
    Logger::debug("Method 2: Enabling advertising via hciconfig");
    try {
        int hciResult = system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1");
        methods.hciconfig = (hciResult == 0);
        
        if (methods.hciconfig) {
            Logger::info("Successfully enabled advertising via hciconfig");
            return true;
        }
    } catch (...) {
        Logger::warn("hciconfig method failed with exception");
    }
    
    // Method 3: Try setting adapter properties directly via D-Bus
    try {
        Logger::debug("Method 3: Enabling advertising via D-Bus properties");
        
        // Create proxy for adapter
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            adapterPath
        );
        
        if (!proxy) {
            Logger::error("Failed to create adapter proxy for advertising");
            return false;
        }
        
        // Make sure adapter is powered
        if (!isAdapterPowered(connection, adapterPath)) {
            sdbus::Variant poweredValue(true);
            setAdapterProperty(connection, BlueZConstants::ADAPTER_PROPERTY_POWERED, poweredValue, adapterPath);
            usleep(200000);  // 200ms to let it take effect
        }
        
        // Try to enable discoverable (similar to advertising)
        sdbus::Variant discValue(true);
        methods.dbus = setAdapterProperty(
            connection, 
            BlueZConstants::ADAPTER_PROPERTY_DISCOVERABLE, 
            discValue, 
            adapterPath
        );
        
        if (methods.dbus) {
            Logger::info("Successfully enabled advertising via D-Bus properties");
            return true;
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception when enabling advertising via D-Bus: " + std::string(e.what()));
    }
    
    // Method 4: Direct HCI commands as last resort
    Logger::debug("Method 4: Enabling advertising via direct HCI commands");
    try {
        int directResult = system("sudo hcitool -i hci0 cmd 0x08 0x000a 01 > /dev/null 2>&1");
        methods.direct = (directResult == 0);
        
        if (methods.direct) {
            Logger::info("Successfully enabled advertising via direct HCI commands");
            return true;
        }
    } catch (...) {
        Logger::warn("Direct HCI commands failed with exception");
    }
    
    // Log all failures
    Logger::error("All advertising methods failed: " +
                 std::string(methods.bluetoothctl ? "" : "bluetoothctl, ") +
                 std::string(methods.hciconfig ? "" : "hciconfig, ") +
                 std::string(methods.dbus ? "" : "D-Bus properties, ") +
                 std::string(methods.direct ? "" : "direct HCI commands"));
    
    return false;
}

bool BlueZUtils::checkBlueZFeatures(std::shared_ptr<SDBusConnection> connection) {
    if (!connection || !connection->isConnected()) {
        Logger::error("Cannot check BlueZ features: invalid D-Bus connection");
        return false;
    }
    
    bool featuresOk = true;
    
    // Get default adapter
    std::string adapter = getDefaultAdapter(connection);
    if (adapter.empty()) {
        Logger::error("No Bluetooth adapter found");
        return false;
    }
    
    // Check for GATT Manager support
    try {
        auto proxy = connection->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            adapter
        );
        
        if (!proxy) {
            Logger::error("Failed to create adapter proxy");
            return false;
        }
        
        // Check for GattManager1 interface
        proxy->callMethod("GetAll")
            .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
            .withArguments(std::string(BlueZConstants::GATT_MANAGER_INTERFACE));
        
        // If we get here, the interface exists
    }
    catch (const std::exception& e) {
        Logger::error("GATT Manager interface not supported: " + std::string(e.what()));
        featuresOk = false;
    }
    
    // Check for LEAdvertisingManager support
    if (!isAdvertisingSupported(connection, adapter)) {
        Logger::error("LE Advertising Manager interface not supported");
        featuresOk = false;
    }
    
    return featuresOk;
}

} // namespace ggk