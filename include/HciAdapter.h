#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h> 
#include <bluetooth/hci_lib.h>

#include "HciSocket.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

class HciAdapter {
public:
    struct HciHeader {
        uint16_t code;
        uint16_t controllerId;
        uint16_t dataSize;

        void toNetwork() {
            code = Utils::endianToHci(code);
            controllerId = Utils::endianToHci(controllerId);
            dataSize = Utils::endianToHci(dataSize);
        }

        void toHost() {
            code = Utils::endianToHost(code);
            controllerId = Utils::endianToHost(controllerId);
            dataSize = Utils::endianToHost(dataSize);
        }
    } __attribute__((packed));

    struct AdapterSettings {
        uint32_t currentSettings;
        uint32_t supportedSettings;
    };

    // Commands
    static const uint16_t CMD_SET_POWERED = 0x0005;
    static const uint16_t CMD_SET_BREDR = 0x002A;
    static const uint16_t CMD_SET_SECURE_CONN = 0x002D;
    static const uint16_t CMD_SET_BONDABLE = 0x0009;
    static const uint16_t CMD_SET_CONNECTABLE = 0x0007;
    static const uint16_t CMD_SET_LE = 0x000D;
    static const uint16_t CMD_SET_ADVERTISING = 0x0029;
    static const uint16_t CMD_SET_LOCAL_NAME = 0x000F;
    static const uint16_t CMD_SET_ADVERTISING_DATA = 0x0008;
    static const uint16_t CMD_SET_DISCOVERABLE = 0x0006;

    // Events
    // static const uint16_t EVT_CMD_COMPLETE = 0x0001;
    // static const uint16_t EVT_CMD_STATUS = 0x0002;

    HciAdapter();
    ~HciAdapter();

    bool initialize();
    void stop();
    
    bool sendCommand(HciHeader& request);
    bool setAdapterName(const std::string& name);
    bool setAdvertisingEnabled(bool enabled);
    bool setPowered(bool powered);
    bool setLEEnabled(bool enabled);

    HciSocket& getSocket() { return hciSocket; }

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