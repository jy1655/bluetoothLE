#include "Server.h"
#include "Logger.h"
#include "Utils.h"
#include "BlueZConstants.h"
#include <csignal>
#include <iostream>
#include <unistd.h>

namespace ggk {

// Static signal handling variables
static std::atomic<bool> g_signalReceived(false);
static std::function<void()> g_shutdownCallback = nullptr;

static void signalHandler(int signal) {
    static int signalCount = 0;
    
    ggk::Logger::info("Received signal: " + std::to_string(signal));
    g_signalReceived = true;
    
    // 첫 번째 신호는 정상 종료 시도
    if (signalCount++ == 0 && g_shutdownCallback) {
        g_shutdownCallback();
        
        // 종료 타이머 설정 (5초 후 강제 종료)
        std::thread([]{
            sleep(5);
            Logger::error("Forced exit due to timeout during shutdown");
            exit(1);
        }).detach();
    } 
    // 두 번째 신호부터는 즉시 종료
    else if (signalCount > 1) {
        Logger::warn("Forced exit requested");
        exit(1);
    }
}

Server::Server()
    : running(false)
    , initialized(false)
    , deviceName("JetsonBLE")
    , advTimeout(0) {
    // Initialize logger
    ggk::Logger::registerDebugReceiver([](const char* msg) { std::cout << "DEBUG: " << msg << std::endl; });
    ggk::Logger::registerInfoReceiver([](const char* msg) { std::cout << "INFO: " << msg << std::endl; });
    ggk::Logger::registerErrorReceiver([](const char* msg) { std::cerr << "ERROR: " << msg << std::endl; });
    ggk::Logger::registerWarnReceiver([](const char* msg) { std::cout << "WARN: " << msg << std::endl; });
    ggk::Logger::registerStatusReceiver([](const char* msg) { std::cout << "STATUS: " << msg << std::endl; });
    ggk::Logger::registerFatalReceiver([](const char* msg) { std::cerr << "FATAL: " << msg << std::endl; });
    ggk::Logger::registerAlwaysReceiver([](const char* msg) { std::cout << "ALWAYS: " << msg << std::endl; });
    ggk::Logger::registerTraceReceiver([](const char* msg) { std::cout << "TRACE: " << msg << std::endl; });
    
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
    
    /*
    // Step 1: Initialize HCI adapter
    hciAdapter = std::make_unique<HciAdapter>();
    if (!hciAdapter->initialize()) {
        Logger::error("Failed to initialize HCI adapter");
        return false;
    }
    

    // Step 2: Initialize Bluetooth management
    mgmt = std::make_unique<Mgmt>(*hciAdapter);
    */

    // Step 3: Setup D-Bus name and connection
    if (!DBusName::getInstance().initialize("com.example.ble")) {
        Logger::error("Failed to initialize D-Bus name");
        return false;
    }
    
    // Step 4: Create GATT application
    application = std::make_unique<GattApplication>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble/gatt")
    );
    
    // Step 5: Create advertisement
    advertisement = std::make_unique<GattAdvertisement>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble/advertisement")
    );
    
    // Set default advertisement properties
    advertisement->setLocalName(deviceName);
    advertisement->setIncludeTxPower(true);
    
    // Step 6: Initialize ConnectionManager
    if (!ConnectionManager::getInstance().initialize(DBusName::getInstance().getConnection())) {
        Logger::warn("Failed to initialize ConnectionManager, connection events may not be detected");
        // Not critical for initialization - continue anyway
    } else {
        // Set connection event callbacks
        ConnectionManager::getInstance().setOnConnectionCallback(
            [this](const std::string& deviceAddress) {
                this->handleConnectionEvent(deviceAddress);
            });
        
        ConnectionManager::getInstance().setOnDisconnectionCallback(
            [this](const std::string& deviceAddress) {
                this->handleDisconnectionEvent(deviceAddress);
            });
        
        ConnectionManager::getInstance().setOnPropertyChangedCallback(
            [this](const std::string& interface, const std::string& property, GVariantPtr value) {
                this->handlePropertyChangeEvent(interface, property, std::move(value));
            });
            
        Logger::info("ConnectionManager initialized successfully");
    }
    
    // Setup signal handlers
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
    
    // Check BlueZ service
    if (system("systemctl is-active --quiet bluetooth.service") != 0) {
        Logger::warn("BlueZ service not active, attempting restart...");
        system("sudo systemctl restart bluetooth.service");
        sleep(2);
        
        if (system("systemctl is-active --quiet bluetooth.service") != 0) {
            Logger::error("Failed to start BlueZ service");
            return false;
        }
    }
    
    // Perform HCI setup
    if (!setupHciInterface()) {
        Logger::error("Failed to setup HCI interface");
        return false;
    }
    
    // Register GATT application with BlueZ
    if (!application->registerWithBlueZ()) {
        Logger::error("Failed to register GATT application with BlueZ");
        return false;
    }
    
    // Register advertisement with BlueZ
    if (!advertisement->registerWithBlueZ()) {
        Logger::warn("Failed to register advertisement with BlueZ, trying alternative methods");
        
        // Try alternative methods to enable advertising
        if (!enableAdvertisingFallback()) {
            Logger::error("Failed to enable advertising using all methods");
            // Continue despite advertising failure - some functions will still work
        }
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
        // Unregister advertisement and application
        if (advertisement) {
            advertisement->unregisterFromBlueZ();
        }
        
        if (application) {
            application->unregisterFromBlueZ();
        }
        
        // Force stop any advertising
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
        
        // Perform a clean HCI reset before exiting
        Logger::info("Performing clean HCI shutdown...");
        system("sudo hciconfig hci0 down > /dev/null 2>&1");
        
        // Wait for thread to complete
        if (eventThread.joinable()) {
            eventThread.join();
        }
        
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
    
    // IMPORTANT: Do not set up D-Bus interfaces yet if the server is already running
    // BlueZ requires all objects to be registered at once when the application is registered
    if (running) {
        Logger::error("Cannot add service while server is running. Stop the server first.");
        return false;
    }
    
    // Add service to GATT application
    if (!application->addService(service)) {
        Logger::error("Failed to add service to application: " + service->getUuid().toString());
        return false;
    }
    
    // Add service UUID to advertisement if not already present
    const GattUuid& uuid = service->getUuid();
    advertisement->addServiceUUID(uuid);
    
    Logger::info("Added service: " + uuid.toString());
    return true;
}

GattServicePtr Server::createService(const GattUuid& uuid, bool isPrimary) {
    if (!initialized || !application) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    // 일관된 경로 명명 규칙 사용
    std::string serviceNum = "service" + std::to_string(application->getServices().size() + 1);
    std::string servicePath = application->getPath().toString() + "/" + serviceNum;
    
    // 서비스 생성
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
    
    // Set local name (if provided, otherwise use device name)
    if (!name.empty()) {
        advertisement->setLocalName(name);
    } else {
        advertisement->setLocalName(deviceName);
    }
    
    // Add service UUIDs
    if (!serviceUuids.empty()) {
        advertisement->addServiceUUIDs(serviceUuids);
    } else {
        // Add all services from the application if no specific UUIDs provided
        for (const auto& service : application->getServices()) {
            advertisement->addServiceUUID(service->getUuid());
        }
    }
    
    // Set manufacturer data if provided
    if (manufacturerId != 0 && !manufacturerData.empty()) {
        advertisement->setManufacturerData(manufacturerId, manufacturerData);
    }
    
    // Set TX power inclusion
    advertisement->setIncludeTxPower(includeTxPower);
    
    // Set advertisement duration (if supported by your GattAdvertisement class)
    if (timeout > 0) {
        advertisement->setDuration(timeout);
    }
    
    // Store timeout for discoverable mode
    advTimeout = timeout;

    system("sudo hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 00 00 00 00 00 00 00 00 00 07 00");
    system("sudo hcitool -i hci0 cmd 0x08 0x000a 01");
    
    // Additional debug printing
    Logger::info("Advertisement configured with service UUIDs:");
    for (const auto& service : application->getServices()) {
        Logger::info("  - " + service->getUuid().toString());
    }
    
    Logger::info("Advertisement configured");
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
    
    // Main event loop
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
    
    // Start event loop in a separate thread
    if (eventThread.joinable()) {
        eventThread.join();  // Join previous thread if any
    }
    
    g_signalReceived = false;
    eventThread = std::thread(&Server::eventLoop, this);
    Logger::info("BLE Server started in async mode");
}

void Server::eventLoop() {
    Logger::info("BLE event loop started");
    
    while (running && !g_signalReceived) {
        // Check connection state via ConnectionManager
        if (ConnectionManager::getInstance().isInitialized()) {
            auto devices = ConnectionManager::getInstance().getConnectedDevices();
            
            // Optional: Log periodic connection status
            static int loopCount = 0;
            if (++loopCount % 100 == 0) {  // Log every ~10 seconds
                if (!devices.empty()) {
                    Logger::debug("Connected devices: " + std::to_string(devices.size()));
                }
            }
            
            // Optional: Add adaptive advertising logic based on connection state
            // For example, increase advertising power when no devices connected
            // or reduce it when devices are connected to save power
        }
        
        // Check for connections/disconnections
        // This would ideally monitor D-Bus signals from BlueZ
        // For now, we just sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
    // Note: We don't need to call stop() here as the signal handler will do it
    // This prevents potential double-stopping issues
    
    Logger::info("BLE event loop ended");
}

void Server::setupSignalHandlers() {
    // Register the server for safe shutdown
    registerShutdownHandler(this);
    
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    Logger::debug("Signal handlers registered");
}

void Server::registerShutdownHandler(Server* server) {
    // Set the global shutdown callback to safely stop the server
    g_shutdownCallback = [server]() {
        // Only stop if running
        if (server && server->running) {
            Logger::info("Signal handler initiating safe server shutdown...");
            server->stop();
        }
    };
    
    // Reset the signal received flag
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

void Server::handlePropertyChangeEvent(const std::string& interface, 
                                      const std::string& property, 
                                      GVariantPtr value) {
    // Log property changes for debug purposes
    if (value) {
        char* valueStr = g_variant_print(value.get(), TRUE);
        Logger::debug("Property changed: " + interface + "." + property + " = " + valueStr);
        g_free(valueStr);
    }
    
    // Handle specific properties that might be interesting
    // For example, MTU size changes could affect how much data we can send
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

bool Server::setupHciInterface() {
    Logger::info("Setting up HCI interface...");
    
    // Check if HCI0 exists
    if (system("ls /sys/class/bluetooth/hci0 > /dev/null 2>&1") != 0) {
        Logger::error("HCI0 interface does not exist. Check if Bluetooth adapter is present.");
        return false;
    }
    
    // Reset HCI interface
    Logger::info("Resetting HCI interface");
    system("sudo hciconfig hci0 down > /dev/null 2>&1");
    usleep(500000);  // 500ms wait
    system("sudo hciconfig hci0 up > /dev/null 2>&1");
    usleep(500000);  // 500ms wait
    
    // Verify HCI interface is up
    if (system("hciconfig hci0 | grep 'UP RUNNING' > /dev/null 2>&1") != 0) {
        Logger::warn("HCI interface not running, attempting reset");
        system("sudo hciconfig hci0 reset > /dev/null 2>&1");
        sleep(1);
        
        if (system("hciconfig hci0 | grep 'UP RUNNING' > /dev/null 2>&1") != 0) {
            Logger::error("Failed to reset HCI interface");
            return false;
        }
    }
    
    // Set device name
    system(("sudo hciconfig hci0 name '" + deviceName + "'").c_str());
    
    return true;
}

bool Server::enableAdvertisingFallback() {
    Logger::info("Trying alternative advertising methods...");
    
    // Method 1: Using hciconfig
    if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Advertisement enabled via hciconfig");
        return true;
    }
    
    // Method 2: Using bluetoothctl
    if (system("echo -e 'menu advertise\\non\\nexit\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Advertisement enabled via bluetoothctl");
        return true;
    }
    
    // Method 3: Using direct HCI commands
    Logger::info("Trying direct HCI commands for advertising");
    system("sudo hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 00 00 00 00 00 00 00 00 00 07 00 > /dev/null 2>&1");
    usleep(100000);  // 100ms wait
    if (system("sudo hcitool -i hci0 cmd 0x08 0x000a 01 > /dev/null 2>&1") == 0) {
        Logger::info("Advertisement enabled via direct HCI commands");
        return true;
    }
    
    return false;
}

bool Server::restartBlueZService() {
    Logger::info("Restarting BlueZ service");
    
    // Stop the service
    system("sudo systemctl stop bluetooth.service");
    sleep(1);
    
    // Start the service
    system("sudo systemctl start bluetooth.service");
    sleep(2);
    
    // Check service status
    if (system("systemctl is-active --quiet bluetooth.service") != 0) {
        Logger::error("Failed to restart BlueZ service");
        return false;
    }
    
    Logger::info("BlueZ service restarted successfully");
    return true;
}

bool Server::resetHciAdapter() {
    Logger::info("Resetting HCI adapter");
    
    // Down the adapter
    system("sudo hciconfig hci0 down");
    usleep(500000);  // 500ms
    
    // Up the adapter
    system("sudo hciconfig hci0 up");
    usleep(500000);  // 500ms
    
    // Check adapter status
    if (system("hciconfig hci0 | grep 'UP RUNNING' > /dev/null 2>&1") != 0) {
        // Try more aggressive reset
        Logger::warn("Standard reset failed, trying hci reset command");
        system("sudo hciconfig hci0 reset");
        sleep(1);
        
        if (system("hciconfig hci0 | grep 'UP RUNNING' > /dev/null 2>&1") != 0) {
            Logger::error("Failed to reset HCI adapter");
            return false;
        }
    }
    
    // Set device name
    system(("sudo hciconfig hci0 name '" + deviceName + "'").c_str());
    
    Logger::info("HCI adapter reset successfully");
    return true;
}

} // namespace ggk