#include "BLEPeripheralManager.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace ggk {

BLEPeripheralManager::BLEPeripheralManager() 
    : isAdvertising(false)
    , hciDevice(-1) {
    Logger::debug("BLE Peripheral Manager created");
}

BLEPeripheralManager::~BLEPeripheralManager() {
    stopAdvertising();
    if (hciDevice >= 0) {
        hci_close_dev(hciDevice);
    }
}

bool BLEPeripheralManager::initialize() {
    hciDevice = hci_open_dev(0);
    if (hciDevice < 0) {
        Logger::error("Failed to open HCI device");
        return false;
    }
    Logger::info("BLE Peripheral Manager initialized");
    return true;
}

bool BLEPeripheralManager::setAdvertisementData(const AdvertisementData& data) {
    if (hciDevice < 0) return false;

    if (hci_write_local_name(hciDevice, "BLEtest", 1000) < 0) {
        Logger::error("Failed to set local name");
        return false;
    }

    // BLE 광고 데이터 설정 (Complete Local Name 포함)
    uint8_t advertisement_data[31] = {0}; // 최대 31바이트
    uint8_t ad_length = 0;
    uint8_t scan_response_data[31] = {0};
    uint8_t scan_length = 0;

    // 1. Complete Local Name (0x09)
    scan_response_data[scan_length++] = data.name.length() + 1;
    scan_response_data[scan_length++] = 0x09; // Complete Local Name
    memcpy(&scan_response_data[scan_length], data.name.c_str(), data.name.length());
    scan_length += data.name.length();

    // Scan Response Data 설정
    struct hci_request scan_rsp_rq = {};
    scan_rsp_rq.ogf = OGF_LE_CTL;
    scan_rsp_rq.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
    scan_rsp_rq.cparam = scan_response_data;
    scan_rsp_rq.clen = sizeof(scan_response_data);

    if (hci_send_req(hciDevice, &scan_rsp_rq, 1000) < 0) {
        Logger::error("Failed to set scan response data");
        return false;
    }
    Logger::info("Scan Response Data set successfully.");

    // 1. Flags (General Discoverable Mode)
    advertisement_data[ad_length++] = 2;  // Length
    advertisement_data[ad_length++] = 0x01;  // Flags type
    advertisement_data[ad_length++] = 0x06;  // LE General Discoverable Mode + BR/EDR Not Supported

    // 2. Complete Local Name (0x09)
    advertisement_data[ad_length++] = data.name.length() + 1; // Length
    advertisement_data[ad_length++] = 0x09; // AD Type: Complete Local Name

    // 3. 실제 장치 이름 복사
    memcpy(&advertisement_data[ad_length], data.name.c_str(), data.name.length());
    ad_length += data.name.length();

    // 광고 데이터 설정
    struct hci_request adv_data_rq = {};
    adv_data_rq.ogf = OGF_LE_CTL;
    adv_data_rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
    adv_data_rq.cparam = advertisement_data;
    adv_data_rq.clen = sizeof(advertisement_data);

    if (hci_send_req(hciDevice, &adv_data_rq, 1000) < 0) {
        Logger::error("Failed to set advertising data");
        return false;
    }

    // 광고 파라미터 설정
    le_set_advertising_parameters_cp adv_params = {};
    adv_params.min_interval = htobs(0x0800);
    adv_params.max_interval = htobs(0x0800);
    adv_params.advtype = 0x00;
    adv_params.chan_map = 7;

    struct hci_request adv_param_rq = {};
    adv_param_rq.ogf = OGF_LE_CTL;
    adv_param_rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
    adv_param_rq.cparam = &adv_params;
    adv_param_rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;

    if (hci_send_req(hciDevice, &adv_param_rq, 1000) < 0) {
        Logger::error("Failed to set advertising parameters");
        return false;
    }

    Logger::info("Advertisement data set successfully with name: " + data.name);
    return true;
}

bool BLEPeripheralManager::startAdvertising() {
    if (hciDevice < 0) return false;
    if (isAdvertising) {
        Logger::warn("Already advertising");
        return true;
    }

    le_set_advertise_enable_cp adv_enable = {};
    adv_enable.enable = 0x01;

    struct hci_request rq = {};
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &adv_enable;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;

    if (hci_send_req(hciDevice, &rq, 1000) < 0) {
        Logger::error("Failed to enable advertising");
        return false;
    }

    isAdvertising = true;
    Logger::info("Started advertising");
    return true;
}

void BLEPeripheralManager::stopAdvertising() {
    if (hciDevice < 0 || !isAdvertising) return;

    le_set_advertise_enable_cp adv_enable = {};
    adv_enable.enable = 0x00;

    struct hci_request rq = {};
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &adv_enable;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;

    if (hci_send_req(hciDevice, &rq, 1000) < 0) {
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