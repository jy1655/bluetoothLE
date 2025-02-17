#include "BLEPeripheralManager.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ggk {

BLEPeripheralManager::BLEPeripheralManager() 
    : hciSocket(std::make_unique<HciSocket>())
    , isAdvertising(false) {
    Logger::debug("BLE Peripheral Manager created");
}

BLEPeripheralManager::~BLEPeripheralManager() {
    stopAdvertising();
}

bool BLEPeripheralManager::initialize() {
    if (!hciSocket->connect()) {
        Logger::error("Failed to connect HCI socket");
        return false;
    }
    
    // HCI reset command
    // HCI_CMD_RESET (0x0C03)
    struct {
        uint8_t type;    // HCI Command packet type
        uint16_t opcode; // OCF = 0x03, OGF = 0x03 (0x0C03)
        uint8_t plen;    // Parameter length
    } __attribute__((packed)) reset_cmd = {
        HCI_COMMAND_PKT,
        htobs(HCI_CMD_RESET),
        0
    };

    if (!hciSocket->write(reinterpret_cast<uint8_t*>(&reset_cmd), sizeof(reset_cmd))) {
        Logger::error("Failed to send HCI reset command");
        return false;
    }

    std::vector<uint8_t> response;
    if (!hciSocket->read(response)) {
        Logger::error("Failed to read HCI reset response");
        return false;
    }

    // Check if reset was successful
    if (response.size() >= 7 && response[0] == HCI_EVENT_PKT && 
        response[1] == EVT_CMD_COMPLETE && response[6] == HCI_SUCCESS) {
        Logger::info("BLE Peripheral Manager initialized");
        return true;
    }

    Logger::error("HCI reset failed");
    return false;
}

bool BLEPeripheralManager::setAdvertisementData(const AdvertisementData& data) {
    std::vector<uint8_t> cmd;
    // LE Set Advertising Data command
    cmd.push_back(0x01);  // HCI command packet
    cmd.push_back(0x08);  // OpCode LSB
    cmd.push_back(0x20);  // OpCode MSB
    cmd.push_back(31);    // Parameter length
    
    // Build advertisement data according to BLE spec
    std::vector<uint8_t> advData;
    
    // Device name
    if (!data.name.empty()) {
        advData.push_back(data.name.length() + 1);
        advData.push_back(0x09);  // Complete Local Name
        advData.insert(advData.end(), data.name.begin(), data.name.end());
    }

    // Service UUIDs
    if (!data.serviceUUIDs.empty()) {
        std::vector<uint8_t> serviceUUIDs;
        for (const auto& uuid : data.serviceUUIDs) {
            std::vector<uint8_t> uuidBytes;
            if (convertUUIDToBytes(uuid, uuidBytes)) {
                serviceUUIDs.insert(serviceUUIDs.end(), uuidBytes.begin(), uuidBytes.end());
            }
        }
        if (!serviceUUIDs.empty()) {
            advData.push_back(serviceUUIDs.size() + 1);
            advData.push_back(0x03);  // Complete List of 16-bit Service UUIDs
            advData.insert(advData.end(), serviceUUIDs.begin(), serviceUUIDs.end());
        }
    }

    // Manufacturer Data
    for (const auto& [companyId, data] : data.manufacturerData) {
        std::vector<uint8_t> mfrData;
        mfrData.push_back(companyId & 0xFF);
        mfrData.push_back((companyId >> 8) & 0xFF);
        mfrData.insert(mfrData.end(), data.begin(), data.end());
        
        advData.push_back(mfrData.size() + 1);
        advData.push_back(0xFF);  // Manufacturer Specific Data
        advData.insert(advData.end(), mfrData.begin(), mfrData.end());
    }

    // Fill remaining space with 0x00
    advData.resize(31, 0x00);
    
    // Add advertisement data to command
    cmd.insert(cmd.end(), advData.begin(), advData.end());

    if (!sendHCICommand(cmd)) {
        Logger::error("Failed to set advertisement data");
        return false;
    }

    Logger::info("Advertisement data set successfully");
    return true;
}

bool BLEPeripheralManager::startAdvertising() {
    if (isAdvertising) {
        Logger::warn("Already advertising");
        return true;
    }

    // LE Set Advertising Parameters command
    std::vector<uint8_t> paramCmd = {
        0x01,  // HCI command packet
        0x06,  // OpCode LSB
        0x20,  // OpCode MSB
        0x0F,  // Parameter length
        0x40, 0x00,  // min interval (64 * 0.625 ms = 40 ms)
        0x80, 0x00,  // max interval (128 * 0.625 ms = 80 ms)
        0x00,  // advertising type (connectable undirected)
        0x00,  // own address type (public)
        0x00,  // peer address type
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // peer address
        0x07,  // advertising channel map (all channels)
        0x00   // filter policy
    };

    if (!sendHCICommand(paramCmd)) {
        Logger::error("Failed to set advertising parameters");
        return false;
    }

    // LE Set Advertising Enable command
    std::vector<uint8_t> enableCmd = {
        0x01,  // HCI command packet
        0x0A,  // OpCode LSB
        0x20,  // OpCode MSB
        0x01,  // Parameter length
        0x01   // Enable advertising
    };

    if (!sendHCICommand(enableCmd)) {
        Logger::error("Failed to start advertising");
        return false;
    }

    isAdvertising = true;
    Logger::info("Started advertising");
    return true;
}

void BLEPeripheralManager::stopAdvertising() {
    if (!isAdvertising) {
        return;
    }

    std::vector<uint8_t> cmd = {
        0x01,  // HCI command packet
        0x0A,  // OpCode LSB
        0x20,  // OpCode MSB
        0x01,  // Parameter length
        0x00   // Disable advertising
    };

    if (!sendHCICommand(cmd)) {
        Logger::error("Failed to stop advertising");
        return;
    }

    isAdvertising = false;
    Logger::info("Stopped advertising");
}

bool BLEPeripheralManager::addGATTService(const GATTService& service) {
    services.push_back(service);
    // TODO: Implement D-Bus communication with BlueZ for GATT service registration
    return true;
}

bool BLEPeripheralManager::sendHCICommand(const std::vector<uint8_t>& cmd) {
    if (!hciSocket->write(cmd)) {
        return false;
    }

    std::vector<uint8_t> response;
    if (!hciSocket->read(response)) {
        return false;
    }

    // Check command complete event
    if (response.size() >= 7 && response[0] == 0x04 && response[1] == 0x0E) {
        return response[6] == 0x00;  // Status code
    }

    return false;
}

bool BLEPeripheralManager::convertUUIDToBytes(const std::string& uuid, std::vector<uint8_t>& bytes) {
    std::string cleanUUID = uuid;
    cleanUUID.erase(std::remove(cleanUUID.begin(), cleanUUID.end(), '-'), cleanUUID.end());
    
    if (cleanUUID.length() != 4 && cleanUUID.length() != 32) {
        Logger::error("Invalid UUID format: " + uuid);
        return false;
    }

    std::istringstream iss(cleanUUID);
    std::string byteStr;
    
    while (iss.good()) {
        byteStr = cleanUUID.substr(0, 2);
        cleanUUID = cleanUUID.substr(2);
        bytes.push_back(std::stoul(byteStr, nullptr, 16));
    }

    return true;
}

} // namespace ggk