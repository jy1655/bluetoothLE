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

// 정적 시그널 처리 변수
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
    
    // 로거 설정
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
        *DBusName::getInstance().getConnection(),
        "/com/example/ble"
    );
    
    // 3. 광고 객체 생성
    advertisement = std::make_unique<GattAdvertisement>(
        *DBusName::getInstance().getConnection(),
        "/com/example/ble/advertisement"
    );
    
    // 4. 광고 속성 설정
    advertisement->setLocalName(deviceName);
    advertisement->setIncludeTxPower(true);
    advertisement->setDiscoverable(true);
    
    // BlueZ 5.82 호환성: Includes 설정
    advertisement->setIncludes({"tx-power", "local-name"});
    
    // 5. ConnectionManager 초기화
    if (!ConnectionManager::getInstance().initialize(DBusName::getInstance().getConnection())) {
        Logger::warn("Failed to initialize ConnectionManager, continuing without event detection");
    } else {
        // 연결 콜백 설정
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

// src/Server.cpp (메서드 구현)
bool Server::setupInterfaces() {
    if (interfaceSetup) return true;
    
    Logger::info("BLE Server 인터페이스 설정 중...");
    
    try {
        // 1. 애플리케이션 인터페이스 설정
        if (!application || !application->setupInterfaces()) {
            Logger::error("애플리케이션 인터페이스 설정 실패");
            return false;
        }
        
        // 2. 광고 인터페이스 설정
        if (!advertisement || !advertisement->setupInterfaces()) {
            Logger::error("광고 인터페이스 설정 실패");
            return false;
        }
        
        // 설정 완료 표시
        interfaceSetup = true;
        Logger::info("BLE Server 인터페이스 설정 완료");
        return true;
    } catch (const std::exception& e) {
        Logger::error("서버 인터페이스 설정 실패: " + std::string(e.what()));
        return false;
    }
}

bool Server::bindToBlueZ() {
    if (boundToBlueZ) return true;
    
    // BlueZ 인터페이스 설정 (어댑터 초기화)
    if (!setupBlueZInterface()) {
        Logger::error("BlueZ 인터페이스 설정 실패");
        return false;
    }
    
    // 인터페이스가 설정되지 않은 경우 먼저 설정
    if (!interfaceSetup) {
        if (!setupInterfaces()) {
            return false;
        }
    }
    
    try {
        Logger::info("BlueZ에 BLE 서버 바인딩 중...");
        
        // 1. 애플리케이션 바인딩
        if (!application->bindToBlueZ()) {
            Logger::error("애플리케이션 BlueZ 바인딩 실패");
            return false;
        }
        
        // 2. 광고 바인딩 (또는 대체 방법 사용)
        bool advertisingEnabled = false;
        
        try {
            if (advertisement->bindToBlueZ()) {
                advertisingEnabled = true;
            } else {
                Logger::warn("표준 광고 바인딩 실패, 대체 방법 시도");
                advertisingEnabled = enableAdvertisingFallback();
            }
        } catch (const std::exception& e) {
            Logger::error("광고 바인딩 예외: " + std::string(e.what()));
            advertisingEnabled = enableAdvertisingFallback();
        }
        
        if (!advertisingEnabled) {
            Logger::warn("모든 광고 방법 실패! 장치가 발견되지 않을 수 있습니다.");
        }
        
        boundToBlueZ = true;
        Logger::info("BlueZ에 BLE 서버 바인딩 완료");
        return true;
    } catch (const std::exception& e) {
        Logger::error("BlueZ 바인딩 실패: " + std::string(e.what()));
        return false;
    }
}

bool Server::unbindFromBlueZ() {
    if (!boundToBlueZ) return true;
    
    try {
        Logger::info("BlueZ에서 BLE 서버 바인딩 해제 중...");
        
        // 1. 광고 바인딩 해제
        if (advertisement) {
            advertisement->unbindFromBlueZ();
        }
        
        // 2. 애플리케이션 바인딩 해제
        if (application) {
            application->unbindFromBlueZ();
        }
        
        // 3. 백업으로 광고 비활성화
        system("sudo hciconfig hci0 noleadv > /dev/null 2>&1");
        
        boundToBlueZ = false;
        Logger::info("BlueZ에서 BLE 서버 바인딩 해제 완료");
        return true;
    } catch (const std::exception& e) {
        Logger::error("BlueZ 바인딩 해제 실패: " + std::string(e.what()));
        boundToBlueZ = false; // 안전을 위해 상태 업데이트
        return false;
    }
}

// start 메서드 수정
bool Server::start(bool secureMode) {
    // 이미 실행 중인 경우
    if (running) {
        Logger::warn("Server already running");
        return true;
    }
    
    // 초기화 상태 확인
    if (!initialized) {
        Logger::error("Cannot start: Server not initialized");
        return false;
    }
    
    Logger::info("Starting BLE Server...");
    
    // 1. 인터페이스 설정
    if (!setupInterfaces()) {
        Logger::error("Failed to setup interfaces");
        return false;
    }

    auto connection = DBusName::getInstance().getConnection();
    if (!connection->isConnected()) {
        Logger::info("Reconnecting to D-Bus and starting event loop");
        if (!connection->connect()) {
            Logger::error("Failed to connect to D-Bus");
            return false;
        }
    } else {
        Logger::info("D-Bus connection already active with event loop");
    }
    
    // 2. BlueZ 바인딩
    if (!bindToBlueZ()) {
        Logger::error("Failed to bind to BlueZ");
        return false;
    }
    
    running = true;
    Logger::info("BLE Server started successfully");
    
    return true;
}

// stop 메서드 수정
void Server::stop() {
    if (!running) {
        return;
    }
    
    Logger::info("Stopping BLE Server...");
    
    running = false;
    
    try {
        // 1. BlueZ 바인딩 해제
        unbindFromBlueZ();
        
        // 2. ConnectionManager 중지
        ConnectionManager::getInstance().shutdown();
        
        // 3. 이벤트 스레드 대기
        if (eventThread.joinable()) {
            // 타임아웃 있는 조인
            auto future = std::async(std::launch::async, [&]() {
                eventThread.join();
            });
            
            auto status = future.wait_for(std::chrono::seconds(2));
            if (status != std::future_status::ready) {
                Logger::warn("Event thread did not complete in time");
            }
        }
        
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
    
    // 실행 중에는 서비스를 추가할 수 없음
    if (running) {
        Logger::error("Cannot add service while server is running. Stop the server first.");
        return false;
    }
    
    // 애플리케이션에 추가
    if (!application->addService(service)) {
        Logger::error("Failed to add service to application: " + service->getUuid().toString());
        return false;
    }
    
    // 광고에 추가
    advertisement->addServiceUUID(service->getUuid());
    
    Logger::info("Added service: " + service->getUuid().toString());
    return true;
}

GattServicePtr Server::createService(const GattUuid& uuid, bool isPrimary) {
    if (!initialized || !application) {
        Logger::error("Cannot create service: Server not initialized");
        return nullptr;
    }
    
    // 경로는 패턴을 사용하여 생성
    std::string serviceNum = "service" + std::to_string(application->getServices().size() + 1);
    std::string servicePath = application->getPath() + "/" + serviceNum;
    
    // 서비스 생성
    auto service = std::make_shared<GattService>(
        *DBusName::getInstance().getConnection(),
        servicePath,
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
    
    // 1. BlueZ 5.82 호환성: Includes 배열 설정
    std::vector<std::string> includes;
    
    // 기본 포함 항목
    if (includeTxPower) {
        includes.push_back("tx-power");
    }
    
    if (!name.empty()) {
        includes.push_back("local-name");
    }
    
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
        includes.push_back("service-uuids");
    } else {
        // 애플리케이션의 모든 서비스 추가
        for (const auto& service : application->getServices()) {
            advertisement->addServiceUUID(service->getUuid());
        }
        if (!application->getServices().empty()) {
            includes.push_back("service-uuids");
        }
    }
    
    // 5. 제조사 데이터 설정
    if (manufacturerId != 0 && !manufacturerData.empty()) {
        advertisement->setManufacturerData(manufacturerId, manufacturerData);
        includes.push_back("manufacturer-data");
    }
    
    // 6. 타임아웃 설정
    if (timeout > 0) {
        advertisement->setDuration(timeout);
    }
    
    // 7. Discoverable 설정 (BlueZ 5.82에서 필수)
    advertisement->setDiscoverable(true);
    
    // 8. Includes 설정 완료
    advertisement->setIncludes(includes);
    
    // 타임아웃 저장
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
    
    // 이벤트 루프
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
    
    // 스레드에서 이벤트 루프 시작
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
        // 연결 확인
        if (ConnectionManager::getInstance().isInitialized()) {
            ConnectionManager::getInstance().getConnectedDevices();
        }
        
        // CPU 사용량 방지
        usleep(100000);  // 100ms
    }
    
    Logger::info("BLE event loop ended");
}

void Server::setupSignalHandlers() {
    // 안전한 종료를 위해 서버 등록
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
    
    // 시그널 수신 플래그 재설정
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
    auto connection = DBusName::getInstance().getConnection();
    
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
        
        // 3. 조회 가능 설정
        if (!BlueZUtils::setAdapterDiscoverable(connection, true, advTimeout, BlueZConstants::ADAPTER_PATH)) {
            Logger::warn("Failed to set adapter as discoverable, continuing anyway");
        }
        
        // 4. 전원 상태 확인
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
    
    // 방법 1: BlueZUtils 사용
    if (BlueZUtils::tryEnableAdvertising(DBusName::getInstance().getConnection(), BlueZConstants::ADAPTER_PATH)) {
        Logger::info("Successfully enabled advertising via BlueZUtils");
        return true;
    }
    
    // 방법 2: bluetoothctl
    if (system("echo -e 'menu advertise\\non\\nback\\n' | bluetoothctl > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via bluetoothctl");
        return true;
    }
    
    // 방법 3: hciconfig
    if (system("sudo hciconfig hci0 leadv 3 > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via hciconfig");
        return true;
    }
    
    // 방법 4: btmgmt
    if (system("sudo btmgmt --index 0 power on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 connectable on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 discov on > /dev/null 2>&1 && "
               "sudo btmgmt --index 0 advertising on > /dev/null 2>&1") == 0) {
        Logger::info("Successfully enabled advertising via btmgmt");
        return true;
    }
    
    // 방법 5: D-Bus를 통해 직접 어댑터 속성 설정
    try {
        auto conn = DBusName::getInstance().getConnection();
        
        // 어댑터 프록시 생성
        auto adapterProxy = conn->createProxy(
            BlueZConstants::BLUEZ_SERVICE,
            BlueZConstants::ADAPTER_PATH
        );
        
        if (adapterProxy) {
            // Discoverable 및 Powered 활성화
            adapterProxy->callMethod("Set")
                .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
                .withArguments(BlueZConstants::ADAPTER_INTERFACE, 
                              "Discoverable", 
                              sdbus::Variant(true));
            
            usleep(100000);  // 100ms
            
            adapterProxy->callMethod("Set")
                .onInterface(BlueZConstants::PROPERTIES_INTERFACE)
                .withArguments(BlueZConstants::ADAPTER_INTERFACE, 
                              "Powered", 
                              sdbus::Variant(true));
            
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