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
        uint8_t type;     // 패킷 타입 (1 = command)
        uint16_t opcode;  // OGF(6비트) + OCF(10비트)
        uint8_t plen;     // 파라미터 길이
    } __attribute__((packed));

    struct AdapterSettings {
        uint32_t currentSettings;
        uint32_t supportedSettings;
    };

    // Commands
    static const uint16_t CMD_SET_POWERED = 0x0C03;           // OGF=0x03, OCF=0x03
    static const uint16_t CMD_SET_BREDR = 0x0C28;            // OGF=0x03, OCF=0x28
    static const uint16_t CMD_SET_LE = 0x0C20;               // OGF=0x03, OCF=0x20
    static const uint16_t CMD_SET_ADVERTISING = 0x0C08;       // OGF=0x03, OCF=0x08
    static const uint16_t CMD_SET_LOCAL_NAME = 0x0C13;        // OGF=0x03, OCF=0x13
    static const uint16_t CMD_SET_ADVERTISING_DATA = 0x2008;  // OGF=0x08, OCF=0x08
    static const uint16_t CMD_SET_DISCOVERABLE = 0x0C1E;      // OGF=0x03, OCF=0x1E
    static const uint16_t CMD_SET_SECURE_CONN = 0x0C41;       // OGF=0x03, OCF=0x41
    static const uint16_t CMD_SET_BONDABLE = 0x0C45;          // OGF=0x03, OCF=0x45
    static const uint16_t CMD_SET_CONNECTABLE = 0x0C26;       // OGF=0x03, OCF=0x26

    HciAdapter();
    ~HciAdapter();

    bool initialize();
    void stop();
    
    bool sendCommand(HciHeader& request);
    bool setAdapterName(const std::string& name);
    bool setAdvertisingEnabled(bool enabled);
    bool setPowered(bool powered);
    bool setLEEnabled(bool enabled);

    void handleCommandComplete(const uint8_t* data, uint8_t length);
    void handleCommandStatus(const uint8_t* data, uint8_t length);

    HciSocket& getSocket() { return hciSocket; }

private:
    HciSocket hciSocket;
    std::atomic<bool> isRunning;
    std::thread eventThread;
    AdapterSettings settings;
    
    void processEvents();

    static constexpr int MAX_EVENT_WAIT_MS = 1000;
    static constexpr uint16_t NON_CONTROLLER_ID = 0xffff;
};

} // namespace ggk