#include "Server.h"
#include "Logger.h"
#include "Utils.h"
#include "ConnectionManager.h"
#include "BlueZUtils.h"
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <future>

namespace ggk {

// Static signal handling variables
static std::atomic<bool> g_signalReceived(false);
static std::function<void()> g_shutdownCallback = nullptr;

// Signal handler
static void signalHandler(int signal) {
    static int signalCount = 0;
    
    Logger::info("Received signal: " + std::to_string(signal));
    g_signalReceived = true;
    
    // First signal: try graceful shutdown
    if (signalCount++ == 0 && g_shutdownCallback) {
        g_shutdownCallback();
    } 
    // Second signal: force exit
    else if (signalCount > 1) {
        Logger::warn("Forced exit requested");
        exit(1);
    }
}

Server::Server()
    : running(false)
    , initialized(false)
    , deviceName("BLEDevice")
    , advTimeout(0) {
    
    // Setup logger
    Logger::registerDebugReceiver([](const char* msg) { std::cout << "DEBUG: " << msg << std::endl; });
    Logger::registerInfoReceiver([](const char* msg) { std::cout << "INFO: " << msg << std::endl; });
    Logger::registerErrorReceiver([](const char* msg) { std::cerr << "ERROR: " << msg << std::endl; });
    Logger::registerWarnReceiver([](const char* msg) { std::cout << "WARN: " << msg << std::endl; });
    Logger::registerStatusReceiver([](const char* msg) { std::cout << "STATUS: " << msg << std::endl; });
    
    Logger::info("BLE Server instance created");
}

Server::~Server() {
    if (running) {
        stop();
    }
    
    if (eventThread.joinable()) {
        eventThread.join();
    }
    
    Logger::info("BLE Server instance destroyed");
}

bool Server::initialize(const std::string& name) {
    if (initialized) {
        Logger::warn("Server already initialized");
        return true;
    }
    
    deviceName = name;
    Logger::info("Initializing BLE Server with name: " + deviceName);
    
    // 1. Initialize D-Bus name
    if (!DBusName::getInstance().initialize("com.example.ble")) {
        Logger::error("Failed to initialize D-Bus name");
        return false;
    }
    
    // 2. Create GATT application
    application = std::make_unique<GattApplication>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble")
    );
    
    // 3. Create advertisement object
    advertisement = std::make_unique<GattAdvertisement>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble/advertisement")
    );
    
    // 4. Setup advertisement properties
    advertisement->setLocalName(deviceName);
    advertisement->setIncludeTxPower(true);
    advertisement->setDiscoverable(true);
    advertisement->setIncludes({"tx-power", "local-name"});  // Required for BlueZ 5.82
    
    // 5. Initialize ConnectionManager
    if (!ConnectionManager::getInstance().initialize(DBusName::getInstance().getConnection())) {
        Logger::warn("Failed to initialize ConnectionManager, continuing without event detection");
    } else {
        // Set connection callbacks
        ConnectionManager::getInstance().setOnConnectionCallback(
            [this](const std::string& deviceAddress) {
                this->handleConnectionEvent(deviceAddress);
            });
        
        ConnectionManager::getInstance().setOnDisconnectionCallback(
            [this](const std::string& deviceAddress) {
                this->handleDisconnectionEvent(deviceAddress);
            });
        
        Logger::info("ConnectionManager initialized successfully");
    }
    
    // 6. Setup signal handlers
    setupSignalHandlers();
    
    initialized = true;
    Logger::info("BLE Server initialization complete");
    return true;
}

bool Server::start(bool secureMode) {
    // Check if already running
    if (running) {
        Logger::warn("Server already running");
        return true;
    }
    
    // Check if initialized
    if (!initialized) {
        Logger::error("Cannot start: Server not initialized");
        return false;
    }
    
    Logger::info("Starting BLE Server...");
    
    // 1. Configure and reset adapter
    if (!setupBlueZInterface()) {
        Logger::error("Failed to setup BlueZ interface");
        return false;
    }
    
    // 2. Register objects with D-Bus and BlueZ in sequence
    
    // 2.1 Register application and objects with D-Bus
    if (!application->ensureInterfacesRegistered()) {
        Logger::error("Failed to register GATT objects with D-Bus");
        return false;
    }
    
    // 2.2 Register application with BlueZ
    if (!application->registerWithBlueZ()) {
        Logger::error("Failed to register GATT application with BlueZ");
        return false;
    }
    
    // 2.3 Register advertisement with D-Bus
    if (!advertisement->setupDBusInterfaces()) {
        Logger::error("Failed to setup advertisement D-Bus interfaces");
        return false;
    }
    
    // 2.4 Register advertisement with BlueZ (or use fallback)
    bool advertisingEnabled = false;
    
    try {
        if (advertisement->registerWithBlueZ()) {
            advertisingEnabled = true;
        } else {
            Logger::warn("Standard advertisement registration failed, trying fallbacks");
            advertisingEnabled = enableAdvertisingFallback();
        }
    } catch (const std::exception& e) {
        Logger::error("Exception in advertisement registration: " + std::string(e.what()));
        advertisingEnabled = enableAdvertisingFallback();
    }
    
    if (!advertisingEnabled) {
        Logger::warn("All advertisement methods failed! Device may not be discoverable.");
    }
    
    running = true;
    Logger::info("BLE Server started successfully");
    
    return true;
}

void Server::stop() {
    if (!running) {
        return;
    }
    
    Logger::info("Stopping BLE Server...");
    
    running = false;
    
    try {
        // 1. Stop ConnectionManager
        ConnectionManager::getInstance().shutdown();
        
        // 2. Unregister advertisement
        if (advertisement) {
            Logger::info("Unregistering advertisement from BlueZ...");
            advertisement->unregisterFromBlueZ();
        }
        
        // 3. Unregister GATT application
        if (application) {
            application->unregisterFromBlueZ();
        }
        
        // 4. Disable advertising as backup
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        
        // 5. Wait for event thread
        if (eventThread.joinable()) {
            // Join with timeout
            auto future = std::async(std::launch::async, [&]() {
                eventThread.join();
            });
            
            auto status = future.wait_for(std::chrono::seconds(2));
            if (status != std::future_status::ready) {
                Logger::warn("Event thread did not complete in time");
            }
        }
        
        // 6. Cleanup adapter
        Logger::info("Resetting Bluetooth adapter...");
        system("sudo hciconfig hci0 down > /dev/null 2>&1");
        
        initialized = false;
        
        Logger::info("BLE Server stopped successfully");
    } catch (const std::exception& e) {
        Logger::error("Exception during server shutdown: " + std::string(e.what()));
    }
}

bool Server::addService(GattServicePtr service) {
    if (!initialized) {
        Logger::error("Cannot add service: Server not initialized");
        return false;
    }
    
    if (!application) {
        Logger::error("Cannot add service: Application not available");
        return false;
    }
    
    // Cannot add services while running
    if (running) {
        Logger::error("Cannot add service while server is running. Stop the server first.");
        return false;
    }
    
    // Add to application
    if (!application->addService(service)) {
        Logger::error("Failed to add service to application: " + service->getUuid().toString());
        return false;
    }
    
    // Add to advertisement
    advertisement->addServiceUUID(service->getUuid());
    
    Logger::info("Added service: " + service->getUuid().toString());
    return true;
}

GattServicePtr Server::createService(const GattUuid& uuid, bool isPrimary) {
    if (!initialized || !application) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    // Create path using a naming pattern
    std::string serviceNum = "service" + std::to_string(application->getServices().size() + 1);
    std::string servicePath = application->getPath().toString() + "/" + serviceNum;
    
    // Create service
    auto service = std::make_shared<GattService>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath(servicePath),
        uuid,
        isPrimary
    );
    
    if (!service) {
        Logger::error("Failed to create service: " + uuid.toString());
        return nullptr;
    }
    
    Logger::info("Created service: " + uuid.toString() + " at path: " + servicePath);
    return service;
}

void Server::configureAdvertisement(
    const std::string& name,
    const std::vector<GattUuid>& serviceUuids,
    uint16_t manufacturerId,
    const std::vector<uint8_t>& manufacturerData,
    bool includeTxPower,
    uint16_t timeout)
{
    if (!advertisement) {
        Logger::error("Cannot configure advertisement: Advertisement not available");
        return;
    }
    
    Logger::info("Configuring BLE advertisement");
    
    // 1. Set includes array for BlueZ 5.82
    std::vector<std::string> includes = {"tx-power", "local-name"};
    
    // Add appearance if set
    if (advertisement->getAppearance() != 0) {
        includes.push_back("appearance");
    }
    
    advertisement->setIncludes(includes);
    
    // 2. Set TX Power
    advertisement->setIncludeTxPower(includeTxPower);
    
    // 3. Set local name
    if (!name.empty()) {
        advertisement->setLocalName(name);
    } else {
        advertisement->setLocalName(deviceName);
    }
    
    // 4. Set service UUIDs
    if (!serviceUuids.empty()) {
        advertisement->addServiceUUIDs(serviceUuids);
    } else {
        // Add all services from application
        for (const auto& service : application->getServices()) {
            advertisement->addServiceUUID(service->getUuid());
        }
    }
    
    // 5. Set manufacturer data
    if (manufacturerId != 0 && !manufacturerData.empty()) {
        advertisement->setManufacturerData(manufacturerId, manufacturerData);
    }
    
    // 6. Set timeout
    if (timeout > 0) {
        advertisement->setDuration(timeout);
    }
    
    // Store timeout
    advTimeout = timeout;
    
    Logger::info("Advertisement configured: LocalName=" + advertisement->getLocalName() + 
                ", TxPower=" + (includeTxPower ? "Yes" : "No") + 
                ", Discoverable=Yes, Duration=" + std::to_string(timeout) + "s");
}

void Server::run() {
    if (!initialized) {
        Logger::error("Cannot run: Server not initialized");
        return;
    }
    
    if (!running) {
        if (!start()) {
            Logger::error("Failed to start server");
            return;
        }
    }
    
    Logger::info("Entering BLE Server main loop");
    g_signalReceived = false;
    
    // Event loop
    eventLoop();
    
    Logger::info("Exited BLE Server main loop");
}

void Server::startAsync() {
    if (!initialized) {
        Logger::error("Cannot start async: Server not initialized");
        return;
    }
    
    if (!running) {
        if (!start()) {
            Logger::error("Failed to start server");
            return;
        }
    }
    
    // Start event loop in thread
    if (eventThread.joinable()) {
        eventThread.join();  // Clean up previous thread
    }
    
    g_signalReceived = false;
    eventThread = std::thread(&Server::eventLoop, this);
    Logger::info("BLE Server started in async mode");
}

void Server::eventLoop() {
    Logger::info("BLE event loop started");
    
    while (running && !g_signalReceived) {
        // Check connections
        if (ConnectionManager::getInstance().isInitialized()) {
            ConnectionManager::getInstance().getConnectedDevices();
        }
        
        // Prevent high CPU usage
        usleep(100000);  // 100ms
    }
    
    Logger::info("BLE event loop ended");
}

void Server::setupSignalHandlers() {
    // Register server for safe shutdown
    registerShutdownHandler(this);
    
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    Logger::debug("Signal handlers registered");
}

void Server::registerShutdownHandler(Server* server) {
    // Set global shutdown callback
    g_shutdownCallback = [server]() {
        if (server && server->running) {
            Logger::info("Signal handler initiating safe server shutdown...");
            server->stop();
        }
    };
    
    // Reset signal received flag
    g_signalReceived = false;
}

void Server::handleConnectionEvent(const std::string& deviceAddress) {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    Logger::info("Client connected: " + deviceAddress);
    
    if (connectionCallback) {
        try {
            connectionCallback(deviceAddress);
        } catch (const std::exception& e) {
            Logger::error("Exception in connection callback: " + std::string(e.what()));
        }
    }
}

void Server::handleDisconnectionEvent(const std::string& deviceAddress) {
    std::lock_guard<std::mutex> lock(callbacksMutex);
    Logger::info("Client disconnected: " + deviceAddress);
    
    if (disconnectionCallback) {
        try {
            disconnectionCallback(deviceAddress);
        } catch (const std::exception& e) {
            Logger::error("Exception in disconnection callback: " + std::string(e.what()));
        }
    }
}

std::vector<std::string> Server::getConnectedDevices() const {
    if (ConnectionManager::getInstance().isInitialized()) {
        return ConnectionManager::getInstance().getConnectedDevices();
    }
    return {};
}

bool Server::isDeviceConnected(const std::string& deviceAddress) const {
    if (ConnectionManager::getInstance().isInitialized()) {
        return ConnectionManager::getInstance().isDeviceConnected(deviceAddress);
    }
    return false;
}

bool Server::setupBlueZInterface() {
    Logger::info("Setting up BlueZ interface...");
    
    // Get D-Bus connection
    DBusConnection& connection = DBusName::getInstance().getConnection();
    
    try {
        // 1. Reset adapter
        if (!BlueZUtils::resetAdapter(connection, BlueZConstants::ADAPTER_PATH)) {
            Logger::error("Failed to reset BlueZ adapter");
            return false;
        }
        
        // 2. Set adapter name
        if (!BlueZUtils::setAdapterName(connection, deviceName, BlueZConstants::ADAPTER_PATH)) {
            Logger::warn("Failed to set adapter name, continuing anyway");
        }
        
        // 3. Set discoverable
        if (!BlueZUtils::setAdapterDiscoverable(connection, true, advTimeout, BlueZConstants::ADAPTER_PATH)) {
            Logger::warn("Failed to set adapter as discoverable, continuing anyway");
        }
        
        // 4. Check power state
        if (!BlueZUtils::isAdapterPowered(connection, BlueZConstants::ADAPTER_PATH)) {
            Logger::error("Failed to power on adapter");
            return false;
        }
        
        // 5. Wait for settings to apply
        usleep(500000);  // 500ms
        
        Logger::info("BlueZ interface setup complete");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in setupBlueZInterface: " + std::string(e.what()));
        return false;
    }
}

bool Server::enableAdvertisingFallback() {
    Logger::info("Trying alternative advertising methods...");
    
    // Method 1: Try using BlueZUtils
    if (BlueZUtils::tryEnableAdvertising(DBusName::getInstance().getConnection(), BlueZConstants::ADAPTER_PATH)) {
        Logger::info("Successfully enabled advertising via BlueZUtils");
        return true;
    }
    
    // Method 2: Try bluetoothctl
    if (system("echo -e 'menu advertise\\non\\nback\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via bluetoothctl");
        return true;
    }
    
    // Method 3: Try hciconfig
    if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via hciconfig");
        return true;
    }
    
    // Method 4: Try btmgmt
    if (system("sudo btmgmt --index 0 power on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 connectable on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 discov on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 advertising on > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via btmgmt");
        return true;
    }
    
    // Method 5: Set adapter properties directly via D-Bus
    try {
        DBusConnection& conn = DBusName::getInstance().getConnection();
        
        // Set discoverable on and powered on
        GVariantPtr discValue = Utils::gvariantPtrFromBoolean(true);
        GVariantPtr powValue = Utils::gvariantPtrFromBoolean(true);
        
        bool setPow = BlueZUtils::setAdapterProperty(conn, "Powered", std::move(powValue), BlueZConstants::ADAPTER_PATH);
        usleep(100000);  // 100ms
        bool setDisc = BlueZUtils::setAdapterProperty(conn, "Discoverable", std::move(discValue), BlueZConstants::ADAPTER_PATH);
        
        if (setPow && setDisc) {
            Logger::info("Successfully enabled advertising via D-Bus properties");
            return true;
        }
    }
    catch (const std::exception& e) {
        Logger::warn("D-Bus property method failed: " + std::string(e.what()));
    }
    
    Logger::error("All advertising methods failed");
    return false;
}

} // namespace ggk