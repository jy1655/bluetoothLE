// src/Server.cpp
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

static void signalHandler(int signal) {
    static int signalCount = 0;
    
    ggk::Logger::info("Received signal: " + std::to_string(signal));
    g_signalReceived = true;
    
    // First signal attempts normal shutdown
    if (signalCount++ == 0 && g_shutdownCallback) {
        g_shutdownCallback();
        
        // Set shutdown timer (5 seconds before forced exit)
        std::thread([]{
            sleep(5);
            Logger::error("Forced exit due to timeout during shutdown");
            exit(1);
        }).detach();
    } 
    // Second signal forces immediate exit
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
    // Initialize logger
    ggk::Logger::registerDebugReceiver([](const char* msg) { std::cout << "DEBUG: " << msg << std::endl; });
    ggk::Logger::registerInfoReceiver([](const char* msg) { std::cout << "INFO: " << msg << std::endl; });
    ggk::Logger::registerErrorReceiver([](const char* msg) { std::cerr << "ERROR: " << msg << std::endl; });
    ggk::Logger::registerWarnReceiver([](const char* msg) { std::cout << "WARN: " << msg << std::endl; });
    ggk::Logger::registerStatusReceiver([](const char* msg) { std::cout << "STATUS: " << msg << std::endl; });
    
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
    
    // 1. D-Bus 이름 초기화
    if (!DBusName::getInstance().initialize("com.example.ble")) {
        Logger::error("Failed to initialize D-Bus name");
        return false;
    }
    
    // 2. GATT 애플리케이션 생성 (ObjectManager 인터페이스는 생성자에서 추가됨)
    application = std::make_unique<GattApplication>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble")
    );
    
    // 3. 광고 객체 생성
    advertisement = std::make_unique<GattAdvertisement>(
        DBusName::getInstance().getConnection(),
        DBusObjectPath("/com/example/ble/advertisement")
    );
    
    // 4. 기본 광고 속성 설정
    advertisement->setLocalName(deviceName);
    advertisement->setIncludeTxPower(true);
    advertisement->setDiscoverable(true);
    
    // 5. BlueZ 5.82 호환성을 위한 Includes 배열 설정
    std::vector<std::string> includes;
    includes.push_back("tx-power");
    includes.push_back("local-name");
    advertisement->setIncludes(includes);
    
    // 6. ConnectionManager 초기화
    if (!ConnectionManager::getInstance().initialize(DBusName::getInstance().getConnection())) {
        Logger::warn("Failed to initialize ConnectionManager, connection events may not be detected");
        // 중요하지 않으므로 계속 진행
    } else {
        // 연결 이벤트 콜백 설정
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
    
    // 7. 시그널 핸들러 설정
    setupSignalHandlers();
    
    initialized = true;
    Logger::info("BLE Server initialization complete");
    return true;
}

bool Server::start(bool secureMode) {
    // 이미 실행 중인지 확인
    if (running) {
        Logger::warn("Server already running");
        return true;
    }
    
    // 초기화 여부 확인
    if (!initialized) {
        Logger::error("Cannot start: Server not initialized");
        return false;
    }
    
    Logger::info("Starting BLE Server...");
    
    // 1. BlueZ 서비스 확인 및 재시작
    if (system("systemctl is-active --quiet bluetooth.service") != 0) {
        Logger::warn("BlueZ service not active, attempting restart...");
        if (!restartBlueZService()) {
            Logger::error("Failed to restart BlueZ service");
            return false;
        }
    }
    
    // 2. BlueZ 인터페이스 설정
    if (!setupBlueZInterface()) {
        Logger::error("Failed to setup BlueZ interface");
        return false;
    }
    
    // 3. 객체 등록: 애플리케이션 객체 먼저 등록
    if (!application->setupDBusInterfaces()) {
        Logger::error("Failed to setup application D-Bus interfaces");
        return false;
    }
    
    // 4. 모든 서비스, 특성 및 디스크립터 등록
    if (!application->ensureInterfacesRegistered()) {
        Logger::error("Failed to register all GATT objects");
        return false;
    }
    
    // 5. GATT 애플리케이션을 BlueZ에 등록
    if (!application->registerWithBlueZ()) {
        Logger::error("Failed to register GATT application with BlueZ");
        return false;
    }
    
    // 6. BlueZ 5.82 호환성을 위한 광고 설정
    advertisement->setupDBusInterfaces();
    
    // 7. BlueZ에 광고 등록
    if (!advertisement->registerWithBlueZ()) {
        Logger::warn("Failed to register advertisement with BlueZ, trying alternative methods");
        enableAdvertisingFallback();
    }
    
    running = true;
    Logger::info("BLE Server started successfully");
    
    // 진단 정보 출력
    
    return true;
}

void Server::stop() {
    if (!running) {
        return;
    }
    
    Logger::info("Stopping BLE Server...");
    
    // First set the running flag to false to signal threads to stop
    running = false;
    
    try {
        // 1. 먼저 ConnectionManager 종료 - BlueZ 시그널 핸들러 제거
        ConnectionManager::getInstance().shutdown();
        
        // 2. 광고 중지
        if (advertisement) {
            Logger::info("Unregistering advertisement from BlueZ...");
            // 여러 번 시도하여 확실히 등록 해제
            for (int i = 0; i < 2; i++) {
                if (advertisement->unregisterFromBlueZ()) {
                    break;
                }
                // 짧은 지연 후 재시도
                usleep(200000);  // 200ms
            }
        }
        
        // 3. GATT 애플리케이션 등록 해제
        if (application) {
            bool unregisterResult = application->unregisterFromBlueZ();
            if (!unregisterResult) {
                Logger::warn("Failed to properly unregister application - forcing cleanup");
            }
        }
        
        // 4. 추가 광고 중지를 위한 직접 명령 실행
        // 저수준 명령으로 확실히 광고 중지
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        system("echo -e 'menu advertise\\noff\\nexit\\n' | bluetoothctl > /dev/null 2>&1");
        
        // 5. Event thread 종료 대기
        if (eventThread.joinable()) {
            // 스레드 조인에 제한 시간 설정
            Logger::debug("Waiting for event thread to complete...");
            auto future = std::async(std::launch::async, [&]() {
                eventThread.join();
            });
            
            // 최대 2초 대기
            auto status = future.wait_for(std::chrono::seconds(2));
            if (status != std::future_status::ready) {
                Logger::warn("Event thread did not complete in time - continuing shutdown");
            } else {
                Logger::debug("Event thread completed successfully");
            }
        }
        
        // 6. 블루투스 어댑터 정리
        Logger::info("Performing clean Bluetooth shutdown...");
        system("sudo hciconfig hci0 down > /dev/null 2>&1");
        usleep(500000);  // 500ms
        
        // 7. DBus 연결 정리
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
    advertisement->addServiceUUID(service->getUuid());
    
    Logger::info("Added service: " + service->getUuid().toString());
    return true;
}

GattServicePtr Server::createService(const GattUuid& uuid, bool isPrimary) {
    if (!initialized || !application) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    // Use consistent path naming convention
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
    
    // BlueZ 5.82 형식의 Includes 배열 설정
    std::vector<std::string> includes;
    
    // TX Power를 Includes에 추가 (BlueZ 5.82 스타일)
    if (includeTxPower) {
        includes.push_back("tx-power");
    }
    
    // 이전 호환성을 위해 이전 방식(IncludeTxPower)도 설정
    advertisement->setIncludeTxPower(includeTxPower);
    
    // Includes 배열 설정
    advertisement->setIncludes(includes);
    
    // Discoverable 설정 (일반적으로 항상 true로 설정)
    advertisement->setDiscoverable(true);
    
    // 로컬 이름 설정 (제공된 경우, 아니면 장치 이름 사용)
    if (!name.empty()) {
        advertisement->setLocalName(name);
        // BlueZ 5.82 스타일: local-name을 Includes에 추가하지 않음 (직접 설정하므로)
    } else {
        advertisement->setLocalName(deviceName);
        // BlueZ 5.82 스타일: local-name을 Includes에 추가하지 않음 (직접 설정하므로)
    }
    
    // 서비스 UUID 추가
    if (!serviceUuids.empty()) {
        advertisement->addServiceUUIDs(serviceUuids);
    } else {
        // 애플리케이션의 모든 서비스 UUID 추가
        for (const auto& service : application->getServices()) {
            advertisement->addServiceUUID(service->getUuid());
        }
    }
    
    // 제조사 데이터 설정 (제공된 경우)
    if (manufacturerId != 0 && !manufacturerData.empty()) {
        advertisement->setManufacturerData(manufacturerId, manufacturerData);
    }
    
    // 광고 지속 시간 설정
    if (timeout > 0) {
        advertisement->setDuration(timeout);
    }
    
    // 타임아웃을 디스커버러블 모드에도 저장
    advTimeout = timeout;
    
    // 디버그 정보 출력
    Logger::info("Advertisement configured with service UUIDs:");
    for (const auto& service : application->getServices()) {
        Logger::info("  - " + service->getUuid().toString());
    }
    
    Logger::info("Advertisement configuration complete");
    
    // 디버그용: 광고 상태 문자열 출력
    Logger::debug(advertisement->getAdvertisementStateString());
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
        }
        
        // Sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
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
    
    // Check if adapter exists
    std::string adapterPath = BlueZConstants::ADAPTER_PATH;
    
    // Reset adapter if needed
    if (!BlueZUtils::resetAdapter(connection, adapterPath)) {
        Logger::error("Failed to reset BlueZ adapter");
        return false;
    }
    
    // Set adapter name
    if (!BlueZUtils::setAdapterName(connection, deviceName, adapterPath)) {
        Logger::warn("Failed to set adapter name, continuing anyway");
    }
    
    // Set adapter discoverable
    if (!BlueZUtils::setAdapterDiscoverable(connection, true, advTimeout, adapterPath)) {
        Logger::warn("Failed to set adapter as discoverable, continuing anyway");
    }
    
    // Set adapter powered
    if (!BlueZUtils::setAdapterPower(connection, true, adapterPath)) {
        Logger::error("Failed to power on adapter");
        return false;
    }
    
    Logger::info("BlueZ interface setup complete");
    return true;
}

bool Server::enableAdvertisingFallback() {
    Logger::info("Trying alternative advertising methods...");
    
    // Use BlueZUtils to try enabling advertising
    return BlueZUtils::tryEnableAdvertising(
        DBusName::getInstance().getConnection(),
        BlueZConstants::ADAPTER_PATH
    );
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

bool Server::resetBluetoothAdapter() {
    Logger::info("Resetting Bluetooth adapter");
    
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
            Logger::error("Failed to reset Bluetooth adapter");
            return false;
        }
    }
    
    // Set device name
    system(("sudo hciconfig hci0 name '" + deviceName + "'").c_str());
    
    Logger::info("Bluetooth adapter reset successfully");
    return true;
}

void Server::updateAdvertisementForBlueZ582() {
    try {
        if (!advertisement) {
            Logger::error("Cannot update advertisement: Advertisement not available");
            return;
        }
        
        Logger::info("Updating advertisement for BlueZ 5.82 compatibility");
        
        // 1. Includes 배열 설정
        std::vector<std::string> includes;
        
        // TX Power 포함
        if (advertisement->getIncludeTxPower()) {
            includes.push_back("tx-power");
        }
        
        // 로컬 이름이 있는 경우
        std::string localName = advertisement->getLocalName();
        if (!localName.empty()) {
            includes.push_back("local-name");
        }
        
        // Appearance 값이 설정된 경우
        if (advertisement->getAppearance() != 0) {
            includes.push_back("appearance");
        }
        
        // Includes 배열 업데이트
        advertisement->setIncludes(includes);
        
        // 2. Discoverable 속성 확인
        advertisement->setDiscoverable(true);
        
        // 3. ServiceUUIDs 확인 - 애플리케이션의 서비스 UUID 추가
        auto services = application->getServices();
        for (const auto& service : services) {
            advertisement->addServiceUUID(service->getUuid());
        }
        
        Logger::info("Advertisement updated for BlueZ 5.82 compatibility");
    }
    catch (const std::exception& e) {
        Logger::error("Exception in updateAdvertisementForBlueZ582: " + std::string(e.what()));
    }
}

// BlueZ ObjectManager 진단
bool Server::diagnoseBluezObjectManager() {
    try {
        Logger::info("Testing ObjectManager.GetManagedObjects...");
        
        // GetManagedObjects 메소드를 직접 호출하여 테스트
        GVariantPtr result = DBusName::getInstance().getConnection().callMethod(
            DBusName::getInstance().getBusName(),
            application->getPath(),
            "org.freedesktop.DBus.ObjectManager",
            "GetManagedObjects"
        );
        
        if (!result) {
            Logger::error("GetManagedObjects test failed: No result returned");
            
            // D-Bus 인터페이스 재등록 시도
            Logger::info("Attempting to re-register ObjectManager interface...");
            application->ensureInterfacesRegistered();
            
            return false;
        }
        
        // 결과 검증
        const char* type_str = g_variant_get_type_string(result.get());
        Logger::info("GetManagedObjects test successful, returned type: " + std::string(type_str));
        
        // 객체 수 확인
        gsize n_children = g_variant_n_children(result.get());
        Logger::info("Managed objects contains " + std::to_string(n_children) + " objects");
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception in diagnoseBluezObjectManager: " + std::string(e.what()));
        
        // D-Bus 인터페이스 재등록 시도
        Logger::info("Attempting to re-register ObjectManager interface...");
        application->ensureInterfacesRegistered();
        
        return false;
    }
}

// BlueZ 상태 진단
void Server::diagnoseBluezState() {
    Logger::info("===== BlueZ Diagnostic Information =====");
    
    // 1. BlueZ 버전 확인
    FILE* pipe = popen("bluetoothd --version 2>&1", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string version(buffer);
            version.pop_back();  // 줄바꿈 제거
            Logger::info("BlueZ version: " + version);
        }
        pclose(pipe);
    }
    
    // 2. 어댑터 상태 확인
    Logger::info("Adapter status:");
    system("hciconfig -a | grep -E 'hci|Type|BD Address|Features|Packet type|Link policy|" 
           "ACL MTU|Name|Class|Service Classes' | sed 's/^/    /'");
    
    // 3. 광고 상태 확인
    Logger::info("Advertisement status:");
    system("hcitool -i hci0 cmd 0x08 0x0006 | sed 's/^/    /'");
    
    // 4. 연결된 장치 확인
    Logger::info("Connected devices:");
    auto devices = getConnectedDevices();
    if (devices.empty()) {
        Logger::info("    No devices connected");
    } else {
        for (const auto& device : devices) {
            Logger::info("    " + device);
        }
    }
    
    // 5. D-Bus 객체 계층 구조 출력
    Logger::info("D-Bus object hierarchy:");
    
    // 애플리케이션 정보
    Logger::info("    Application: " + application->getPath().toString() + 
                " (Registered: " + (application->isRegistered() ? "Yes" : "No") + ")");
    
    // 서비스 정보
    auto services = application->getServices();
    Logger::info("    Services count: " + std::to_string(services.size()));
    
    for (const auto& service : services) {
        Logger::info("        Service: " + service->getUuid().toString() + 
                    " at " + service->getPath().toString() + 
                    " (Registered: " + (service->isRegistered() ? "Yes" : "No") + ")");
        
        // 특성 정보
        auto characteristics = service->getCharacteristics();
        Logger::info("        Characteristics count: " + std::to_string(characteristics.size()));
    }
    
    // 6. 광고 정보
    if (advertisement) {
        Logger::info("Advertisement information:");
        Logger::info("    Path: " + advertisement->getPath().toString());
        Logger::info("    Registered: " + std::string(advertisement->isRegistered() ? "Yes" : "No"));
        Logger::info("    Local Name: " + advertisement->getLocalName());
        
        std::string includesStr = "    Includes: [";
        auto includes = advertisement->getIncludes();
        for (const auto& item : includes) {
            includesStr += item + ", ";
        }
        if (!includes.empty()) {
            includesStr.pop_back();
            includesStr.pop_back();
        }
        includesStr += "]";
        Logger::info(includesStr);
    }
    
    Logger::info("===== End of BlueZ Diagnostic Information =====");
}

// 광고 속성 접근자 추가 (헤더에도 추가 필요)
std::vector<std::string> Server::getAdvertisementIncludes() {
    if (advertisement) {
        return advertisement->getIncludes();
    }
    return {};
}

uint16_t Server::getAdvertisementAppearance() {
    if (advertisement) {
        return advertisement->getAppearance();
    }
    return 0;
}

bool Server::getAdvertisementIncludeTxPower() {
    if (advertisement) {
        return advertisement->getIncludeTxPower();
    }
    return false;
}

std::string Server::getAdvertisementLocalName() {
    if (advertisement) {
        return advertisement->getLocalName();
    }
    return "";
}

} // namespace ggk