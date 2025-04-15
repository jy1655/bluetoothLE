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

// 정적 신호 처리 변수
static std::atomic<bool> g_signalReceived(false);
static std::function<void()> g_shutdownCallback = nullptr;

// 시그널 핸들러
static void signalHandler(int signal) {
    static int signalCount = 0;
    
    Logger::info("Received signal: " + std::to_string(signal));
    g_signalReceived = true;
    
    // 첫 번째 시그널: 정상 종료 시도
    if (signalCount++ == 0 && g_shutdownCallback) {
        g_shutdownCallback();
    } 
    // 두 번째 시그널: 강제 종료
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
    
    // 로거 초기화
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
    
    // 1. D-Bus 이름 초기화
    if (!DBusName::getInstance().initialize("com.example.ble")) {
        Logger::error("Failed to initialize D-Bus name");
        return false;
    }
    
    // 2. GATT 애플리케이션 생성
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
    advertisement->setIncludes({"tx-power", "local-name"});
    
    // 5. ConnectionManager 초기화
    if (!ConnectionManager::getInstance().initialize(DBusName::getInstance().getConnection())) {
        Logger::warn("Failed to initialize ConnectionManager, continuing without event detection");
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
    
    // 6. 시그널 핸들러 설정
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
    
    // 1. BlueZ 인터페이스 설정
    if (!setupBlueZInterface()) {
        Logger::error("Failed to setup BlueZ interface");
        return false;
    }
    
    // 2. 애플리케이션 D-Bus 인터페이스 설정
    if (!application->setupDBusInterfaces()) {
        Logger::error("Failed to setup application D-Bus interfaces");
        return false;
    }
    
    // 3. 모든 서비스, 특성, 디스크립터 등록 확인
    if (!application->ensureInterfacesRegistered()) {
        Logger::error("Failed to register all GATT objects");
        return false;
    }
    
    // 4. GATT 애플리케이션 BlueZ 등록
    if (!application->registerWithBlueZ()) {
        Logger::error("Failed to register GATT application with BlueZ");
        return false;
    }
    
    // 5. 광고 설정 및 BlueZ 등록
    advertisement->ensureBlueZ582Compatibility();
    
    if (!advertisement->registerWithBlueZ()) {
        Logger::warn("Failed to register advertisement with BlueZ, trying alternative methods");
        if (!enableAdvertisingFallback()) {
            Logger::warn("All advertisement methods failed, but continuing...");
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
    
    // 실행 상태 플래그 변경
    running = false;
    
    try {
        // 1. ConnectionManager 종료
        ConnectionManager::getInstance().shutdown();
        
        // 2. 광고 중지
        if (advertisement) {
            Logger::info("Unregistering advertisement from BlueZ...");
            advertisement->unregisterFromBlueZ();
        }
        
        // 3. GATT 애플리케이션 등록 해제
        if (application) {
            application->unregisterFromBlueZ();
        }
        
        // 4. 백업 방법으로 광고 중지
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        
        // 5. 이벤트 스레드 종료 대기
        if (eventThread.joinable()) {
            // 제한 시간 2초로 스레드 종료 대기
            auto future = std::async(std::launch::async, [&]() {
                eventThread.join();
            });
            
            auto status = future.wait_for(std::chrono::seconds(2));
            if (status != std::future_status::ready) {
                Logger::warn("Event thread did not complete in time - continuing shutdown");
            }
        }
        
        // 6. 블루투스 어댑터 정리
        Logger::info("Performing clean Bluetooth shutdown...");
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
    
    // 서버가 실행 중일 때는 서비스 추가 불가
    if (running) {
        Logger::error("Cannot add service while server is running. Stop the server first.");
        return false;
    }
    
    // 애플리케이션에 서비스 추가
    if (!application->addService(service)) {
        Logger::error("Failed to add service to application: " + service->getUuid().toString());
        return false;
    }
    
    // 광고에 서비스 UUID 추가
    advertisement->addServiceUUID(service->getUuid());
    
    Logger::info("Added service: " + service->getUuid().toString());
    return true;
}

GattServicePtr Server::createService(const GattUuid& uuid, bool isPrimary) {
    if (!initialized || !application) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    // 경로 명명 규칙에 따라 경로 생성
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
    
    Logger::info("Configuring BLE advertisement");
    
    // 1. BlueZ 5.82 호환성을 위한 Includes 배열 설정
    std::vector<std::string> includes = {"tx-power", "local-name"};
    
    // 외관이 설정된 경우 includes에 추가
    if (advertisement->getAppearance() != 0) {
        includes.push_back("appearance");
    }
    
    // Includes 배열 설정
    advertisement->setIncludes(includes);
    
    // 2. TX Power 설정
    advertisement->setIncludeTxPower(includeTxPower);
    
    // 3. 로컬 이름 설정
    if (!name.empty()) {
        advertisement->setLocalName(name);
    } else {
        advertisement->setLocalName(deviceName);
    }
    
    // 4. 서비스 UUID 설정
    if (!serviceUuids.empty()) {
        advertisement->addServiceUUIDs(serviceUuids);
    } else {
        // 애플리케이션의 모든 서비스 UUID 추가
        for (const auto& service : application->getServices()) {
            advertisement->addServiceUUID(service->getUuid());
        }
    }
    
    // 5. 제조사 데이터 설정
    if (manufacturerId != 0 && !manufacturerData.empty()) {
        advertisement->setManufacturerData(manufacturerId, manufacturerData);
    }
    
    // 6. 제한 시간 설정
    if (timeout > 0) {
        advertisement->setDuration(timeout);
    }
    
    // 제한 시간 저장
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
    
    // 메인 이벤트 루프
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
    
    // 별도 스레드에서 이벤트 루프 시작
    if (eventThread.joinable()) {
        eventThread.join();  // 이전 스레드 정리
    }
    
    g_signalReceived = false;
    eventThread = std::thread(&Server::eventLoop, this);
    Logger::info("BLE Server started in async mode");
}

void Server::eventLoop() {
    Logger::info("BLE event loop started");
    
    while (running && !g_signalReceived) {
        // 연결 상태 확인
        if (ConnectionManager::getInstance().isInitialized()) {
            ConnectionManager::getInstance().getConnectedDevices();
        }
        
        // 과도한 CPU 사용 방지
        usleep(100000);  // 100ms
    }
    
    Logger::info("BLE event loop ended");
}

void Server::setupSignalHandlers() {
    // 안전한 종료를 위한 서버 등록
    registerShutdownHandler(this);
    
    // 시그널 핸들러 등록
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    Logger::debug("Signal handlers registered");
}

void Server::registerShutdownHandler(Server* server) {
    // 전역 종료 콜백 설정
    g_shutdownCallback = [server]() {
        if (server && server->running) {
            Logger::info("Signal handler initiating safe server shutdown...");
            server->stop();
        }
    };
    
    // 시그널 수신 플래그 초기화
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
    
    // D-Bus 연결 가져오기
    DBusConnection& connection = DBusName::getInstance().getConnection();
    
    try {
        // 1. 어댑터 초기화
        if (!BlueZUtils::resetAdapter(connection, BlueZConstants::ADAPTER_PATH)) {
            Logger::error("Failed to reset BlueZ adapter");
            return false;
        }
        
        // 2. 어댑터 이름 설정
        if (!BlueZUtils::setAdapterName(connection, deviceName, BlueZConstants::ADAPTER_PATH)) {
            Logger::warn("Failed to set adapter name, continuing anyway");
        }
        
        // 3. 어댑터 조회 가능 설정
        if (!BlueZUtils::setAdapterDiscoverable(connection, true, advTimeout, BlueZConstants::ADAPTER_PATH)) {
            Logger::warn("Failed to set adapter as discoverable, continuing anyway");
        }
        
        // 4. 어댑터 전원 상태 확인
        if (!BlueZUtils::isAdapterPowered(connection, BlueZConstants::ADAPTER_PATH)) {
            Logger::error("Failed to power on adapter");
            return false;
        }
        
        // 5. 설정 적용 대기
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
    
    // 방법 1: bluetoothctl 사용
    if (system("echo -e 'menu advertise\\non\\nback\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via bluetoothctl");
        return true;
    }
    
    // 방법 2: hciconfig 사용
    if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via hciconfig");
        return true;
    }
    
    // 방법 3: BlueZ 직접 API 사용
    try {
        if (BlueZUtils::tryEnableAdvertising(
                DBusName::getInstance().getConnection(), 
                BlueZConstants::ADAPTER_PATH)) {
            Logger::info("Successfully enabled advertising via BlueZ API");
            return true;
        }
    } catch (...) {
        // 예외 무시하고 계속 진행
    }
    
    Logger::error("All advertising methods failed");
    return false;
}

} // namespace ggk