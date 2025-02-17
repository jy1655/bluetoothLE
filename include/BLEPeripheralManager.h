#pragma once

#include <memory>
#include <map>
#include <string>
#include <vector>
#include "HciSocket.h"
#include "Logger.h"

namespace ggk {

class BLEPeripheralManager {
public:
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
    bool isAdvertising;
    int hciDevice;
};

} // namespace ggk