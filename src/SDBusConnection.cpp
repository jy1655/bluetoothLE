#include "SDBusConnection.h"
#include "Logger.h"

namespace ggk {

SDBusConnection::SDBusConnection(bool useSystemBus)
    : connected(false) {
    try {
        // Create the appropriate connection type
        if (useSystemBus) {
            connection = sdbus::createSystemBusConnection();
        } else {
            connection = sdbus::createSessionBusConnection();
        }
        
        Logger::info("SDBusConnection created");
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to create D-Bus connection: " + std::string(e.what()));
        connection = nullptr;
    }
}

SDBusConnection::~SDBusConnection() {
    disconnect();
    Logger::info("SDBusConnection destroyed");
}

bool SDBusConnection::connect() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    if (connected) {
        return true;
    }
    
    if (!connection) {
        Logger::error("Cannot connect: connection is null");
        return false;
    }
    
    try {
        // Start processing D-Bus messages in a separate thread
        connection->enterEventLoopAsync();
        connected = true;
        Logger::info("Connected to D-Bus");
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to connect to D-Bus: " + std::string(e.what()));
        return false;
    }
}

void SDBusConnection::disconnect() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    if (!connected || !connection) {
        return;
    }
    
    try {
        // Stop message processing
        connection->leaveEventLoop();
        connected = false;
        Logger::info("Disconnected from D-Bus");
    }
    catch (const sdbus::Error& e) {
        Logger::error("Error during disconnect: " + std::string(e.what()));
    }
}

bool SDBusConnection::isConnected() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connected && connection != nullptr;
}

sdbus::IConnection& SDBusConnection::getSdbusConnection() {
    if (!connection) {
        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", "Connection is null");
    }
    
    return *connection;
}

std::unique_ptr<sdbus::IProxy> SDBusConnection::createProxy(
    const std::string& destination,
    const std::string& objectPath) {
    if (!connection) {
        Logger::error("Cannot create proxy: connection is null");
        return nullptr;
    }
    
    try {
        return sdbus::createProxy(*connection, destination, objectPath);
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to create proxy: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<sdbus::IObject> SDBusConnection::createObject(
    const std::string& objectPath) {
    if (!connection) {
        Logger::error("Cannot create object: connection is null");
        return nullptr;
    }
    
    try {
        return sdbus::createObject(*connection, objectPath);
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to create object: " + std::string(e.what()));
        return nullptr;
    }
}

bool SDBusConnection::requestName(const std::string& serviceName) {
    if (!connection) {
        Logger::error("Cannot request name: connection is null");
        return false;
    }
    
    try {
        connection->requestName(serviceName);
        Logger::info("Successfully acquired service name: " + serviceName);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to request name: " + std::string(e.what()));
        return false;
    }
}

bool SDBusConnection::releaseName(const std::string& serviceName) {
    if (!connection) {
        Logger::error("Cannot release name: connection is null");
        return false;
    }
    
    try {
        connection->releaseName(serviceName);
        Logger::info("Successfully released service name: " + serviceName);
        return true;
    }
    catch (const sdbus::Error& e) {
        Logger::error("Failed to release name: " + std::string(e.what()));
        return false;
    }
}

void SDBusConnection::enterEventLoop() {
    if (!connection) {
        Logger::error("Cannot enter event loop: connection is null");
        return;
    }
    
    try {
        connection->enterEventLoop();
    }
    catch (const sdbus::Error& e) {
        Logger::error("Error in event loop: " + std::string(e.what()));
    }
}

void SDBusConnection::leaveEventLoop() {
    if (!connection) {
        Logger::error("Cannot leave event loop: connection is null");
        return;
    }
    
    try {
        connection->leaveEventLoop();
    }
    catch (const sdbus::Error& e) {
        Logger::error("Error leaving event loop: " + std::string(e.what()));
    }
}

} // namespace ggk