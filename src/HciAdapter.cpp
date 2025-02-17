#include "HciAdapter.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <string.h>

namespace ggk {

HciAdapter::HciAdapter() 
    : isRunning(false) {
}

HciAdapter::~HciAdapter() {
    stop();
}

bool HciAdapter::initialize() {
    if (!hciSocket.connect()) {
        Logger::error("Failed to connect HCI socket");
        return false;
    }

    isRunning = true;
    eventThread = std::thread(&HciAdapter::processEvents, this);
    
    // Power on and enable BLE
    if (!setPowered(true) || !setLEEnabled(true)) {
        stop();
        return false;
    }

    Logger::info("HCI Adapter initialized");
    return true;
}

void HciAdapter::stop() {
    isRunning = false;
    
    if (eventThread.joinable()) {
        eventThread.join();
    }
    
    hciSocket.disconnect();
}

bool HciAdapter::sendCommand(HciHeader& request) {
    if (!hciSocket.isConnected()) {
        Logger::error("HCI socket not connected");
        return false;
    }

    request.toNetwork();
    uint8_t* pRequest = reinterpret_cast<uint8_t*>(&request);
    
    std::vector<uint8_t> requestPacket(pRequest, pRequest + sizeof(request) + request.dataSize);
    if (!hciSocket.write(requestPacket)) {
        Logger::error("Failed to write HCI command");
        return false;
    }

    return true;
}

bool HciAdapter::setAdapterName(const std::string& name) {
    struct {
        HciHeader header;
        char name[249];
        char shortName[11];
    } __attribute__((packed)) request;

    request.header.code = CMD_SET_LOCAL_NAME;
    request.header.controllerId = 0;  // First controller
    request.header.dataSize = sizeof(request) - sizeof(HciHeader);

    strncpy(request.name, name.c_str(), sizeof(request.name) - 1);
    strncpy(request.shortName, name.c_str(), sizeof(request.shortName) - 1);

    return sendCommand(request.header);
}

bool HciAdapter::setAdvertisingEnabled(bool enabled) {
    struct {
        HciHeader header;
        uint8_t enable;
    } __attribute__((packed)) request;

    request.header.code = CMD_SET_ADVERTISING;
    request.header.controllerId = 0;
    request.header.dataSize = sizeof(request) - sizeof(HciHeader);
    request.enable = enabled ? 0x01 : 0x00;

    return sendCommand(request.header);
}

bool HciAdapter::setPowered(bool powered) {
    struct {
        HciHeader header;
        uint8_t powered;
    } __attribute__((packed)) request;

    request.header.code = CMD_SET_POWERED;
    request.header.controllerId = 0;
    request.header.dataSize = sizeof(request) - sizeof(HciHeader);
    request.powered = powered ? 0x01 : 0x00;

    return sendCommand(request.header);
}

bool HciAdapter::setLEEnabled(bool enabled) {
    struct {
        HciHeader header;
        uint8_t enable;
    } __attribute__((packed)) request;

    request.header.code = CMD_SET_LE;
    request.header.controllerId = 0;
    request.header.dataSize = sizeof(request) - sizeof(HciHeader);
    request.enable = enabled ? 0x01 : 0x00;

    return sendCommand(request.header);
}

void HciAdapter::processEvents() {
    Logger::debug("Started HCI event processing thread");

    while (isRunning && hciSocket.isConnected()) {
        std::vector<uint8_t> response;
        if (!hciSocket.read(response)) {
            continue;
        }

        if (response.size() < sizeof(HciHeader)) {
            Logger::error("Received invalid HCI event (too short)");
            continue;
        }

        HciHeader* header = reinterpret_cast<HciHeader*>(response.data());
        header->toHost();

        switch (header->code) {
            case EVT_CMD_COMPLETE:
                handleCommandComplete(response);
                break;
            case EVT_CMD_STATUS:
                handleCommandStatus(response);
                break;
            default:
                Logger::debug("Received unhandled HCI event: " + std::to_string(header->code));
                break;
        }
    }

    Logger::debug("Stopped HCI event processing thread");
}

void HciAdapter::handleCommandComplete(const std::vector<uint8_t>& response) {
    struct CommandComplete {
        HciHeader header;
        uint16_t command;
        uint8_t status;
    } __attribute__((packed));

    if (response.size() < sizeof(CommandComplete)) {
        Logger::error("Invalid command complete event");
        return;
    }

    const CommandComplete* evt = reinterpret_cast<const CommandComplete*>(response.data());
    Logger::debug("Command " + std::to_string(evt->command) + " completed with status: " + 
                 std::to_string(evt->status));
}

void HciAdapter::handleCommandStatus(const std::vector<uint8_t>& response) {
    struct CommandStatus {
        HciHeader header;
        uint16_t command;
        uint8_t status;
    } __attribute__((packed));

    if (response.size() < sizeof(CommandStatus)) {
        Logger::error("Invalid command status event");
        return;
    }

    const CommandStatus* evt = reinterpret_cast<const CommandStatus*>(response.data());
    Logger::debug("Command " + std::to_string(evt->command) + " status: " + 
                 std::to_string(evt->status));
}

} // namespace ggk