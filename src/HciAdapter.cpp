#include "HciAdapter.h"
#include "BlueZUtils.h"
#include "Utils.h"
#include <unistd.h>

namespace ggk {

HciAdapter::HciAdapter()
    : connection(nullptr)
    , initialized(false)
    , advertising(false) {
}

HciAdapter::~HciAdapter() {
    stop();
}

bool HciAdapter::initialize(
    DBusConnection& connection,
    const std::string& adapterName,
    const std::string& adapterPath)
{
    Logger::info("Initializing HCI adapter: " + adapterPath);
    
    // Store parameters
    this->connection = &connection;
    this->adapterPath = adapterPath;
    this->adapterName = adapterName;
    
    // Check if adapter exists
    if (!verifyAdapterExists()) {
        Logger::error("Adapter does not exist: " + adapterPath);
        return false;
    }
    
    // Check if BlueZ service is running
    if (!Utils::isBlueZServiceRunning()) {
        Logger::warn("BlueZ service not running, attempting to start...");
        if (!BlueZUtils::restartBlueZService()) {
            Logger::error("Failed to start BlueZ service");
            return false;
        }
    }
    
    // Initialize adapter using BlueZUtils
    if (!BlueZUtils::initializeAdapter(connection, adapterName, adapterPath)) {
        Logger::error("Failed to initialize adapter");
        return false;
    }
    
    initialized = true;
    Logger::info("HCI adapter initialized successfully");
    return true;
}

void HciAdapter::stop() {
    if (!initialized) {
        return;
    }
    
    Logger::info("Stopping HCI adapter");
    
    // Disable advertising if it's on
    if (advertising) {
        disableAdvertising();
    }
    
    // Set adapter to default state
    try {
        // Power off the adapter
        BlueZUtils::setAdapterPower(*connection, false, adapterPath);
        
        // Clear adapter discoverable state
        BlueZUtils::setAdapterDiscoverable(*connection, false, 0, adapterPath);
    }
    catch (const std::exception& e) {
        Logger::warn("Exception during HCI adapter shutdown: " + std::string(e.what()));
    }
    
    initialized = false;
    connection = nullptr;
}

bool HciAdapter::isInitialized() const {
    return initialized && connection != nullptr;
}

const std::string& HciAdapter::getAdapterPath() const {
    return adapterPath;
}

bool HciAdapter::setName(const std::string& name) {
    if (!isInitialized()) {
        Logger::error("Cannot set name: HCI adapter not initialized");
        return false;
    }
    
    if (BlueZUtils::setAdapterName(*connection, name, adapterPath)) {
        adapterName = name;
        return true;
    }
    
    return false;
}

bool HciAdapter::setPowered(bool powered) {
    if (!isInitialized()) {
        Logger::error("Cannot set power state: HCI adapter not initialized");
        return false;
    }
    
    return BlueZUtils::setAdapterPower(*connection, powered, adapterPath);
}

bool HciAdapter::setDiscoverable(bool discoverable, uint16_t timeout) {
    if (!isInitialized()) {
        Logger::error("Cannot set discoverable state: HCI adapter not initialized");
        return false;
    }
    
    return BlueZUtils::setAdapterDiscoverable(*connection, discoverable, timeout, adapterPath);
}

bool HciAdapter::reset() {
    if (!isInitialized()) {
        Logger::error("Cannot reset: HCI adapter not initialized");
        return false;
    }
    
    // Disable advertising first if it's enabled
    if (advertising) {
        disableAdvertising();
    }
    
    return BlueZUtils::resetAdapter(*connection, adapterPath);
}

bool HciAdapter::enableAdvertising() {
    if (!isInitialized()) {
        Logger::error("Cannot enable advertising: HCI adapter not initialized");
        return false;
    }
    
    // Check if advertising is already enabled
    if (advertising) {
        Logger::debug("Advertising is already enabled");
        return true;
    }
    
    // First check if advertising is supported
    if (!isAdvertisingSupported()) {
        Logger::error("Advertising is not supported on this adapter");
        return false;
    }
    
    // Try to enable advertising using BlueZUtils
    if (BlueZUtils::tryEnableAdvertising(*connection, adapterPath)) {
        advertising = true;
        Logger::info("Advertising enabled successfully");
        return true;
    }
    
    // All methods failed
    Logger::error("Failed to enable advertising");
    return false;
}

bool HciAdapter::disableAdvertising() {
    if (!isInitialized()) {
        Logger::error("Cannot disable advertising: HCI adapter not initialized");
        return false;
    }
    
    // Check if advertising is already disabled
    if (!advertising) {
        Logger::debug("Advertising is already disabled");
        return true;
    }
    
    // Try to disable advertising using bluetoothctl
    if (runBluetoothCtlCommand({"menu advertise", "off", "back"})) {
        advertising = false;
        Logger::info("Advertising disabled successfully");
        return true;
    }
    
    // As a fallback, try setting discoverable to false
    if (BlueZUtils::setAdapterDiscoverable(*connection, false, 0, adapterPath)) {
        advertising = false;
        Logger::info("Advertising disabled by disabling discoverable mode");
        return true;
    }
    
    Logger::warn("Failed to disable advertising");
    return false;
}

bool HciAdapter::isAdvertisingSupported() const {
    if (!isInitialized()) {
        return false;
    }
    
    return BlueZUtils::isAdvertisingSupported(*connection, adapterPath);
}

DBusConnection& HciAdapter::getConnection() {
    if (!connection) {
        throw std::runtime_error("No D-Bus connection available");
    }
    
    return *connection;
}

bool HciAdapter::verifyAdapterExists() {
    // Check if adapter exists in the filesystem
    if (!Utils::isBluetoothAdapterAvailable(adapterPath.substr(adapterPath.rfind('/') + 1))) {
        Logger::error("Bluetooth adapter not found in system");
        return false;
    }
    
    // If we have a connection, check if adapter exists in BlueZ
    if (connection && connection->isConnected()) {
        auto adapters = BlueZUtils::getAdapters(*connection);
        for (const auto& adapter : adapters) {
            if (adapter == adapterPath) {
                return true;
            }
        }
        
        Logger::error("Adapter not found in BlueZ: " + adapterPath);
        return false;
    }
    
    return true;
}

bool HciAdapter::runBluetoothCtlCommand(const std::vector<std::string>& commands) {
    return BlueZUtils::runBluetoothCtlCommands(commands);
}

} // namespace ggk