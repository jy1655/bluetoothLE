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
    ggk::Logger::info("Received signal: " + std::to_string(signal));
    g_signalReceived = true;
    
    // Call the shutdown callback if registered
    if (g_shutdownCallback) {
        g_shutdownCallback();
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
    
    // Step 1: Initialize HCI adapter
    hciAdapter = std::make_unique<HciAdapter>();
    if (!hciAdapter->initialize()) {
        Logger::error("Failed to initialize HCI adapter");
        return false;
    }
    
    // Step 2: Initialize Bluetooth management
    mgmt = std::make_unique<Mgmt>(*hciAdapter);
    
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
    if (!initialized) {
        Logger::error("Cannot start: Server not initialized");
        return false;
    }
    
    if (running) {
        Logger::warn("Server already running");
        return true;
    }
    
    Logger::info("Starting BLE Server...");
    
    // 어댑터 다시 초기화
    Logger::info("Resetting Bluetooth adapter...");
    system("sudo hciconfig hci0 down");
    sleep(1);
    system("sudo hciconfig hci0 up");
    sleep(1);
    
    // Step 1: Configure BLE controller
    if (!mgmt->setPowered(true)) {
        Logger::error("Failed to power on BLE adapter");
        return false;
    }
    
    // 설정 반영될 시간 주기
    sleep(1);
    
    if (!mgmt->setBredr(false)) {
        Logger::warn("Failed to disable BR/EDR mode");
        // Not critical, continue anyway
    }
    
    if (!mgmt->setLE(true)) {
        Logger::error("Failed to enable LE mode");
        return false;
    }
    
    // 설정 반영될 시간 주기
    sleep(1);
    
    // 디바이스 이름 설정
    Logger::info("Setting device name to: " + deviceName);
    if (!mgmt->setName(deviceName, deviceName.substr(0, std::min(deviceName.length(), 
                                                               static_cast<size_t>(Mgmt::kMaxAdvertisingShortNameLength))))) {
        Logger::warn("Failed to set adapter name via Mgmt");
        // 대체 방법으로 HciAdapter 직접 사용
        if (!hciAdapter->setAdapterName(deviceName)) {
            Logger::warn("Failed to set adapter name via HciAdapter");
        }
    }
    
    // 설정 반영될 시간 주기
    sleep(1);
    
    // 설정 확인 (디버깅 목적)
    Logger::info("Checking current adapter settings...");
    system("hciconfig -a hci0");
    
    // Configure security if enabled
    if (secureMode) {
        Logger::info("Enabling BLE security features...");
        
        if (!mgmt->setSecureConnections(1)) {
            Logger::warn("Failed to enable secure connections");
            // Not critical, continue anyway
        }
        
        if (!mgmt->setBondable(true)) {
            Logger::warn("Failed to make device bondable");
            // Not critical, continue anyway
        }
    }
    
    // *** 중요: 객체 등록 순서 변경 ***
    // 1. 애플리케이션/광고 객체 초기화 - 항상 새로 시작하도록 기존 등록 해제
    if (application->isRegistered()) {
        Logger::info("Unregistering application to refresh");
        application->unregisterObject();
    }
    
    if (advertisement->isRegistered()) {
        Logger::info("Unregistering advertisement to refresh");
        advertisement->unregisterObject();
    }
    
    // 2. 서비스 설정
    for (auto& service : application->getServices()) {
        if (!service->isRegistered() && !service->setupDBusInterfaces()) {
            Logger::error("Failed to setup D-Bus interfaces for service: " + service->getUuid().toString());
            return false;
        }
    }
    
    // 3. 애플리케이션 등록
    Logger::info("Registering GATT application with BlueZ...");
    if (!application->registerWithBlueZ()) {
        Logger::error("Failed to register GATT application with BlueZ");
        return false;
    }
    
    // 설정 반영될 시간 주기
    sleep(1);
    
    // 4. 광고 등록
    Logger::info("Registering advertisement with BlueZ...");
    if (!advertisement->registerWithBlueZ()) {
        Logger::error("Failed to register advertisement with BlueZ");
        application->unregisterFromBlueZ();
        return false;
    }
    
    // 설정 반영될 시간 주기
    sleep(1);
    
    // 5. 광고 활성화
    Logger::info("Enabling advertising...");
    if (!mgmt->setAdvertising(1)) {
        Logger::error("Failed to enable advertising");
        advertisement->unregisterFromBlueZ();
        application->unregisterFromBlueZ();
        return false;
    }
    
    if (!mgmt->setDiscoverable(1, advTimeout)) {
        Logger::warn("Failed to make device discoverable");
        // Not critical, continue anyway
    }
    
    // 연결 상태 확인
    Logger::info("BLE stack initialized successfully");
    Logger::info("Checking for any connected devices...");
    auto connectedDevices = ConnectionManager::getInstance().getConnectedDevices();
    if (!connectedDevices.empty()) {
        Logger::info("Currently connected devices: " + std::to_string(connectedDevices.size()));
        for (const auto& device : connectedDevices) {
            Logger::info("  - " + device);
        }
    } else {
        Logger::info("No devices currently connected");
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
    
    // Set running to false immediately to prevent new operations
    running = false;
    
    try {
        // Step 1: Stop advertising 
        if (mgmt) {
            Logger::debug("Disabling BLE advertising...");
            mgmt->setAdvertising(0);
        }
        
        // Step 2: Unregister advertisement from BlueZ
        if (advertisement) {
            Logger::debug("Unregistering advertisement from BlueZ...");
            advertisement->unregisterFromBlueZ();
        }
        
        // Step 3: Unregister GATT application from BlueZ
        if (application) {
            Logger::debug("Unregistering GATT application from BlueZ...");
            application->unregisterFromBlueZ();
        }
        
        // Step 4: Power down BLE controller
        if (mgmt) {
            Logger::debug("Powering down BLE controller...");
            mgmt->setPowered(false);
        }
        
        // Step 5: Stop HCI adapter
        if (hciAdapter) {
            Logger::debug("Stopping HCI adapter...");
            hciAdapter->stop();
        }
        
        // Step 6: Shutdown ConnectionManager
        if (ConnectionManager::getInstance().isInitialized()) {
            Logger::debug("Shutting down ConnectionManager...");
            ConnectionManager::getInstance().shutdown();
        }
        
        Logger::info("BLE Server stopped successfully");
    } catch (const std::exception& e) {
        Logger::error("Exception during server shutdown: " + std::string(e.what()));
        // Even if there's an exception, we want to mark the server as stopped
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
    if (!initialized) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    if (!application) {
        Logger::error("Cannot create service: Application not available");
        return nullptr;
    }
    
    // Create a path based on the UUID
    std::string uuidStr = uuid.toString();
    std::string servicePath = application->getPath().toString() + "/service" + 
                              std::to_string(application->getServices().size() + 1);
    
    // Create the service
    auto service = std::make_shared<GattService>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath(servicePath),
        uuid,
        isPrimary
    );
    
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

} // namespace ggk