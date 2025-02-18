#include "BLEPeripheralManager.h"
#include <string.h>

namespace ggk {

BLEPeripheralManager::BLEPeripheralManager() 
    : hciAdapter(std::make_unique<HciAdapter>())
    , isAdvertising(false) {
    Logger::debug("BLE Peripheral Manager created");
}

BLEPeripheralManager::~BLEPeripheralManager() {
    stopAdvertising();
}

bool BLEPeripheralManager::initialize() {
    if (!hciAdapter->initialize()) {
        Logger::error("Failed to initialize HCI adapter");
        return false;
    }

    mgmt = std::make_unique<Mgmt>(*hciAdapter);

    // 기본 설정
    if (!mgmt->setPowered(true)) {
        Logger::error("Failed to power on controller");
        return false;
    }

    if (!mgmt->setLE(true)) {
        Logger::error("Failed to enable LE");
        return false;
    }

    if (!mgmt->setBredr(false)) {  // BR/EDR 비활성화 (BLE only)
        Logger::error("Failed to disable BR/EDR");
        return false;
    }

    Logger::info("BLE Peripheral Manager initialized");
    return true;
}

bool BLEPeripheralManager::setAdvertisementData(const AdvertisementData& data) {
    // 장치 이름 설정
    if (!mgmt->setName(data.name, data.name.substr(0, Mgmt::kMaxAdvertisingShortNameLength))) {
        Logger::error("Failed to set device name");
        return false;
    }

    // 광고 데이터 설정
    struct {
        uint8_t type;    // HCI Command packet type = 0x01
        uint16_t opcode; // Set Advertising Data command
        uint8_t plen;    // Parameter length
        uint8_t data[31];  // 최대 31 바이트
    } __attribute__((packed)) advData = {};

    advData.type = 0x01;  // HCI Command
    advData.opcode = htole16(HciAdapter::CMD_SET_ADVERTISING_DATA);
    advData.plen = 31;    // 항상 31바이트 전송

    uint8_t offset = 0;

    // 1. Flags
    advData.data[offset++] = 2;  // Length
    advData.data[offset++] = 0x01;  // Type (Flags)
    advData.data[offset++] = 0x06;  // LE General Discoverable + BR/EDR Not Supported

    // 2. Service UUIDs (있는 경우)
    if (!data.serviceUUIDs.empty()) {
        std::vector<uint8_t> uuids;
        for (const auto& uuid : data.serviceUUIDs) {
            // 16-bit UUID 처리
            if (uuid.length() == 4) {
                uint16_t uuid_val = std::stoul(uuid, nullptr, 16);
                uuids.push_back(uuid_val & 0xFF);
                uuids.push_back((uuid_val >> 8) & 0xFF);
            }
        }
        
        if (!uuids.empty() && offset + uuids.size() + 2 <= 31) {
            advData.data[offset++] = uuids.size() + 1;  // Length
            advData.data[offset++] = 0x03;  // Type (Complete List of 16-bit Service UUIDs)
            memcpy(&advData.data[offset], uuids.data(), uuids.size());
            offset += uuids.size();
        }
    }

    // 3. Device Name
    if (!data.name.empty() && offset + data.name.length() + 2 <= 31) {
        size_t nameLen = std::min(data.name.length(), size_t(31 - offset - 2));
        advData.data[offset++] = nameLen + 1;  // Length
        advData.data[offset++] = 0x09;  // Type (Complete Local Name)
        memcpy(&advData.data[offset], data.name.c_str(), nameLen);
        offset += nameLen;
    }

    if (!hciAdapter->sendCommand(*reinterpret_cast<HciAdapter::HciHeader*>(&advData))) {
        Logger::error("Failed to set advertising data");
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

    // Discoverable 모드 설정
    if (!mgmt->setDiscoverable(0x01, 0)) {  // General discoverable, no timeout
        Logger::error("Failed to set discoverable mode");
        return false;
    }

    // 광고 활성화
    if (!mgmt->setAdvertising(0x02)) {  // Connectable advertising
        Logger::error("Failed to enable advertising");
        return false;
    }

    isAdvertising = true;
    Logger::info("Started advertising");
    return true;
}

void BLEPeripheralManager::stopAdvertising() {
    if (!isAdvertising) return;

    if (!mgmt->setAdvertising(0x00)) {  // Disable advertising
        Logger::error("Failed to disable advertising");
        return;
    }

    isAdvertising = false;
    Logger::info("Stopped advertising");
}

bool BLEPeripheralManager::addGATTService(const GATTService& service) {
    // GATT 서비스 구현은 아직...
    return true;
}

} // namespace ggk