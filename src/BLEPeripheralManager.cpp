#include "BLEPeripheralManager.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace ggk {

BLEPeripheralManager::BLEPeripheralManager() 
    : hciAdapter(std::make_unique<HciAdapter>())
    , mgmt(std::make_unique<Mgmt>(0))  // 첫 번째 컨트롤러 사용
    , isAdvertising(false) {
    Logger::debug("BLE Peripheral Manager created");
}

BLEPeripheralManager::~BLEPeripheralManager() {
    stopAdvertising();
}

bool BLEPeripheralManager::initialize() {
    // HCI 어댑터 초기화
    if (!hciAdapter->initialize()) {
        Logger::error("Failed to initialize HCI adapter");
        return false;
    }

    // 블루투스 컨트롤러 설정
    if (!mgmt->setPowered(true) ||
        !mgmt->setLE(true) ||
        !mgmt->setBredr(false) ||  // BR/EDR 비활성화 (BLE only)
        !mgmt->setSecureConnections(1)) {  // Secure Connections 활성화
        Logger::error("Failed to configure bluetooth controller");
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
        HciAdapter::HciHeader header;
        uint8_t data[31];
    } __attribute__((packed)) adv_data = {};

    uint8_t offset = 0;

    // 1. Flags
    adv_data.data[offset++] = 2;  // Length
    adv_data.data[offset++] = 0x01;  // Type (Flags)
    adv_data.data[offset++] = 0x06;  // LE General Discoverable + BR/EDR Not Supported

    // 2. Complete Local Name
    if (!data.name.empty()) {
        size_t nameLen = std::min(data.name.length(), size_t(29));  // 최대 29바이트 (길이 + 타입 = 2바이트)
        adv_data.data[offset++] = nameLen + 1;  // Length
        adv_data.data[offset++] = 0x09;  // Type (Complete Local Name)
        memcpy(&adv_data.data[offset], data.name.c_str(), nameLen);
        offset += nameLen;
    }

    // 3. Service UUIDs (있는 경우)
    if (!data.serviceUUIDs.empty()) {
        std::vector<uint8_t> uuids;
        for (const auto& uuid : data.serviceUUIDs) {
            // 16-bit UUID만 지원
            if (uuid.length() == 4) {
                uint16_t uuid_val = std::stoul(uuid, nullptr, 16);
                uuids.push_back(uuid_val & 0xFF);
                uuids.push_back((uuid_val >> 8) & 0xFF);
            }
        }
        
        if (!uuids.empty()) {
            adv_data.data[offset++] = uuids.size() + 1;  // Length
            adv_data.data[offset++] = 0x03;  // Type (Complete List of 16-bit Service UUIDs)
            memcpy(&adv_data.data[offset], uuids.data(), uuids.size());
            offset += uuids.size();
        }
    }

    // 광고 데이터 설정
    adv_data.header.code = HciAdapter::CMD_SET_LE;
    adv_data.header.controllerId = 0;
    adv_data.header.dataSize = offset;

    if (!hciAdapter->sendCommand(adv_data.header)) {
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

    // 먼저 discoverable 모드 설정
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
    // TODO: GATT 서비스 구현
    // BlueZ D-Bus API를 사용하여 구현 필요
    return true;
}

} // namespace ggk