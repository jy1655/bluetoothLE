#pragma once

#include <memory>
#include <map>
#include <string>
#include <vector>
#include "HciAdapter.h"
#include "Mgmt.h"
#include "Logger.h"

namespace ggk {

class BLEPeripheralManager {
public:
    struct InitConfig {
        int deviceIndex = 0;  // hci 디바이스 인덱스
        bool forcePowercycle = false;  // 강제 전원 사이클
        int initRetryCount = 3;  // 초기화 재시도 횟수
    };

    struct GATTService {
        std::string uuid;
        std::vector<std::string> characteristics;
        bool primary;
    };

    struct AdvertisementData {
        std::string name;
        std::vector<std::string> serviceUUIDs;
        std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
        std::map<std::string, std::vector<uint8_t>> serviceData;
        int16_t txPower;
    };

    BLEPeripheralManager();
    ~BLEPeripheralManager();

    bool initialize();
    bool setAdvertisementData(const AdvertisementData& data);
    bool startAdvertising();
    void stopAdvertising();
    bool addGATTService(const GATTService& service);

private:
    std::unique_ptr<HciAdapter> hciAdapter;
    std::unique_ptr<Mgmt> mgmt;
    bool isAdvertising;
};

} // namespace ggk