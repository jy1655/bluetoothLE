#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include "HciSocket.h"
#include "Logger.h"

namespace ggk {

class HciAdapter {
public:
    struct HciHeader {
        uint16_t code;
        uint16_t controllerId;
        uint16_t dataSize;

        void toNetwork() {
            code = htobs(code);
            controllerId = htobs(controllerId);
            dataSize = htobs(dataSize);
        }

        void toHost() {
            code = btohs(code);
            controllerId = btohs(controllerId);
            dataSize = btohs(dataSize);
        }
    } __attribute__((packed));

    struct AdapterSettings {
        uint32_t currentSettings;
        uint32_t supportedSettings;
    };

    HciAdapter();
    ~HciAdapter();

    bool initialize();
    void stop();
    
    bool sendCommand(HciHeader& request);
    bool setAdapterName(const std::string& name);
    bool setAdvertisingEnabled(bool enabled);
    bool setPowered(bool powered);
    bool setLEEnabled(bool enabled);

    // 상수 정의
    static constexpr uint16_t CMD_SET_POWERED = 0x0005;
    static constexpr uint16_t CMD_SET_ADVERTISING = 0x0029;
    static constexpr uint16_t CMD_SET_LE = 0x000D;
    static constexpr uint16_t CMD_SET_LOCAL_NAME = 0x000F;
    
    static constexpr uint8_t EVT_CMD_COMPLETE = 0x0001;
    static constexpr uint8_t EVT_CMD_STATUS = 0x0002;
    
    static constexpr uint8_t STATUS_SUCCESS = 0x00;

private:
    HciSocket hciSocket;
    std::atomic<bool> isRunning;
    std::thread eventThread;
    AdapterSettings settings;
    
    void processEvents();
    void handleCommandComplete(const std::vector<uint8_t>& response);
    void handleCommandStatus(const std::vector<uint8_t>& response);

    static constexpr int MAX_EVENT_WAIT_MS = 1000;
    static constexpr uint16_t NON_CONTROLLER_ID = 0xffff;
};

} // namespace ggk