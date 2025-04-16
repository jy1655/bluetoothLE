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

std::vector<std::string> BlueZUtils::getAdapters(std::shared_ptr<IDBusConnection> connection) {
    std::vector<std::string> adapters;
    
    try {
        // Call org.freedesktop.DBus.ObjectManager.GetManagedObjects()
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            BlueZConstants::GET_MANAGED_OBJECTS
        );
        
        if (!result) {
            Logger::error("Failed to get managed objects");
            return adapters;
        }
        
        // Parse the result (a{oa{sa{sv}}})
        GVariantIter iter;
        g_variant_iter_init(&iter, result.get());
        
        gchar* objectPath;
        GVariant* interfaces;
        
        while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &objectPath, &interfaces)) {
            if (!objectPath || !interfaces) {
                continue;
            }
            
            GVariantPtr interfacesPtr = makeGVariantPtr(interfaces, true); 
            
            // Check if this object has the Adapter1 interface
            GVariant* adapterInterface = g_variant_lookup_value(
                interfacesPtr.get(),
                BlueZConstants::ADAPTER_INTERFACE.c_str(),
                G_VARIANT_TYPE("a{sv}")
            );
            
            if (adapterInterface) {
                adapters.push_back(objectPath);
                g_variant_unref(adapterInterface);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getAdapters: " + std::string(e.what()));
    }
    
    return adapters;
}

std::string BlueZUtils::getDefaultAdapter(std::shared_ptr<IDBusConnection> connection) {
    std::vector<std::string> adapters = getAdapters(connection);
    
    if (adapters.empty()) {
        Logger::error("No Bluetooth adapters found");
        return "";
    }
    
    // Usually, the first adapter (hci0) is the default
    return adapters[0];
}

GVariantPtr BlueZUtils::getAdapterProperty(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& property,
    const std::string& adapterPath)
{
    try {
        // Call org.freedesktop.DBus.Properties.Get
        GVariant* params = g_variant_new("(ss)", 
                               BlueZConstants::ADAPTER_INTERFACE.c_str(),
                               property.c_str());
        // floating reference를 sink하여 참조 카운트 관리
        GVariantPtr parameters = makeGVariantPtr(g_variant_ref_sink(params), true); 
        
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(adapterPath),
            BlueZConstants::PROPERTIES_INTERFACE,
            "Get",
            std::move(parameters)
        );
        
        if (!result) {
            Logger::error("Failed to get adapter property: " + property);
            return makeNullGVariantPtr();
        }
        
        // Extract the variant from the result (which is a variant of variant)
        GVariant* variant = nullptr;
        g_variant_get(result.get(), "(v)", &variant);
        
        if (!variant) {
            Logger::error("Invalid result format for adapter property: " + property);
            return makeNullGVariantPtr();
        }
        
        return makeGVariantPtr(variant, true);  // take ownership
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getAdapterProperty: " + std::string(e.what()));
        return makeNullGVariantPtr();
    }
}

bool BlueZUtils::setAdapterProperty(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& property,
    GVariantPtr value,
    const std::string& adapterPath)
{
    try {
        if (!value) {
            Logger::error("Cannot set adapter property with null value: " + property);
            return false;
        }
        
        // Create parameters for Properties.Set
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add(&builder, "s", BlueZConstants::ADAPTER_INTERFACE.c_str());
        g_variant_builder_add(&builder, "s", property.c_str());
        g_variant_builder_add(&builder, "v", value.get());
        
        GVariant* params = g_variant_builder_end(&builder);
        GVariantPtr parameters = makeGVariantPtr(params, true);
        
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(adapterPath),
            BlueZConstants::PROPERTIES_INTERFACE,
            "Set",
            std::move(parameters)
        );
        
        // No return value for Set, so just check if the call succeeded
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in setAdapterProperty: " + std::string(e.what()));
        return false;
    }
}

bool BlueZUtils::isAdapterPowered(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& adapterPath)
{
    GVariantPtr result = getAdapterProperty(connection, 
                                          BlueZConstants::ADAPTER_PROPERTY_POWERED,
                                          adapterPath);
    
    if (!result) {
        return false;
    }
    
    return Utils::variantToBoolean(result.get(), false);
}

bool BlueZUtils::setAdapterPower(
    std::shared_ptr<IDBusConnection> connection,
    bool powered,
    const std::string& adapterPath)
{
    Logger::info(std::string("Setting adapter power to: ") + (powered ? "on" : "off"));
    
    GVariantPtr value = Utils::gvariantPtrFromBoolean(powered);
    
    bool success = setAdapterProperty(connection, 
                                    BlueZConstants::ADAPTER_PROPERTY_POWERED,
                                    std::move(value),
                                    adapterPath);
    
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

bool BlueZUtils::setAdapterDiscoverable(
    std::shared_ptr<IDBusConnection> connection,
    bool discoverable,
    uint16_t timeout,
    const std::string& adapterPath)
{
    Logger::info(std::string("Setting adapter discoverable to: ") + 
                 (discoverable ? "on" : "off"));
    
    // Set discoverable
    GVariantPtr discValue = Utils::gvariantPtrFromBoolean(discoverable);
    
    bool success = setAdapterProperty(connection, 
                                    BlueZConstants::ADAPTER_PROPERTY_DISCOVERABLE,
                                    std::move(discValue),
                                    adapterPath);
    
    if (!success) {
        return false;
    }
    
    // Set timeout if discoverable is enabled
    if (discoverable && timeout > 0) {
        GVariantPtr timeoutValue = Utils::gvariantPtrFromUInt(timeout);
        
        success = setAdapterProperty(connection, 
                                    BlueZConstants::ADAPTER_PROPERTY_DISCOVERABLE_TIMEOUT,
                                    std::move(timeoutValue),
                                    adapterPath);
    }
    
    return success;
}

bool BlueZUtils::setAdapterName(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& name,
    const std::string& adapterPath)
{
    Logger::info("Setting adapter alias to: " + name);
    
    // BlueZ 5.82에서는 Name 속성이 읽기 전용이므로 Alias 속성을 사용
    GVariantPtr value = Utils::gvariantPtrFromString(name);
    
    try {
        // Alias 속성 설정
        bool success = setAdapterProperty(connection, 
                                         "Alias",
                                         std::move(value),
                                         adapterPath);
        
        if (success) {
            Logger::info("Successfully set adapter alias");
            return true;
        } else {
            Logger::warn("Failed to set adapter alias, continuing anyway");
            return false;
        }
    }
    catch (const std::exception& e) {
        Logger::warn("Exception in setAdapterName (using Alias): " + std::string(e.what()) + 
                     ". Continuing anyway.");
        return false;
    }
}

bool BlueZUtils::resetAdapter(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& adapterPath)
{
    Logger::info("Resetting adapter: " + adapterPath);
    
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

bool BlueZUtils::initializeAdapter(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& name,
    const std::string& adapterPath)
{
    Logger::info("Initializing adapter for BLE peripheral mode");
    
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
    std::shared_ptr<IDBusConnection> connection,
    const std::string& adapterPath)
{
    std::vector<std::string> connectedDevices;
    
    try {
        // Call org.freedesktop.DBus.ObjectManager.GetManagedObjects()
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(BlueZConstants::ROOT_PATH),
            BlueZConstants::OBJECT_MANAGER_INTERFACE,
            BlueZConstants::GET_MANAGED_OBJECTS
        );
        
        if (!result) {
            Logger::error("Failed to get managed objects");
            return connectedDevices;
        }
        
        // Parse the result (a{oa{sa{sv}}})
        GVariantIter iter;
        g_variant_iter_init(&iter, result.get());
        
        gchar* objectPath;
        GVariant* interfaces;
        
        while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &objectPath, &interfaces)) {
            if (!objectPath || !interfaces) {
                continue;
            }
            
            // Check if this is a device under our adapter
            if (std::string(objectPath).find(adapterPath) != 0) {
                g_variant_unref(interfaces);
                continue;
            }
            
            GVariantPtr interfacesPtr = makeGVariantPtr(interfaces, true); 
            
            // Check if this object has the Device1 interface
            GVariant* deviceInterface = g_variant_lookup_value(
                interfacesPtr.get(),
                BlueZConstants::DEVICE_INTERFACE.c_str(),
                G_VARIANT_TYPE("a{sv}")
            );
            
            if (deviceInterface) {
                // Check if device is connected
                GVariant* connectedValue = g_variant_lookup_value(
                    deviceInterface,
                    BlueZConstants::PROPERTY_CONNECTED.c_str(),
                    G_VARIANT_TYPE_BOOLEAN
                );
                
                if (connectedValue && g_variant_get_boolean(connectedValue)) {
                    connectedDevices.push_back(objectPath);
                }
                
                if (connectedValue) {
                    g_variant_unref(connectedValue);
                }
                
                g_variant_unref(deviceInterface);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::error("Exception in getConnectedDevices: " + std::string(e.what()));
    }
    
    return connectedDevices;
}

bool BlueZUtils::isAdvertisingSupported(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& adapterPath)
{
    try {
        // See if LEAdvertisingManager1 interface exists
        GVariantPtr parameters = Utils::gvariantPtrFromString(BlueZConstants::LE_ADVERTISING_MANAGER_INTERFACE);
        
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(adapterPath),
            BlueZConstants::PROPERTIES_INTERFACE,
            "GetAll",
            std::move(parameters)
        );
        
        return result != nullptr;
    }
    catch (const std::exception& e) {
        // Exception is expected if interface doesn't exist
        return false;
    }
}

bool BlueZUtils::tryEnableAdvertising(
    std::shared_ptr<IDBusConnection> connection,
    const std::string& adapterPath)
{
    Logger::info("Attempting to enable BLE advertising");
    
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
        
        // Make sure adapter is powered
        if (!isAdapterPowered(connection, adapterPath)) {
            GVariantPtr poweredValue = Utils::gvariantPtrFromBoolean(true);
            setAdapterProperty(connection, "Powered", std::move(poweredValue), adapterPath);
            usleep(200000);  // 200ms to let it take effect
        }
        
        // Try to enable discoverable (similar to advertising)
        GVariantPtr discValue = Utils::gvariantPtrFromBoolean(true);
        methods.dbus = setAdapterProperty(connection, "Discoverable", std::move(discValue), adapterPath);
        
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

bool BlueZUtils::checkBlueZFeatures(std::shared_ptr<IDBusConnection> connection) {
    bool featuresOk = true;
    
    // Get default adapter
    std::string adapter = getDefaultAdapter(connection);
    if (adapter.empty()) {
        Logger::error("No Bluetooth adapter found");
        return false;
    }
    
    // Check for GATT Manager support
    try {
        GVariantPtr parameters = Utils::gvariantPtrFromString(BlueZConstants::GATT_MANAGER_INTERFACE);
        
        GVariantPtr result = connection->callMethod(
            BlueZConstants::BLUEZ_SERVICE,
            DBusObjectPath(adapter),
            BlueZConstants::PROPERTIES_INTERFACE,
            "GetAll",
            std::move(parameters)
        );
        
        if (!result) {
            Logger::error("GATT Manager interface not supported");
            featuresOk = false;
        }
    }
    catch (const std::exception&) {
        Logger::error("GATT Manager interface not supported");
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