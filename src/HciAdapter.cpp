#include "HciAdapter.h"
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
    uint8_t* pRequest = reinterpret_cast<uint8_t*>(&request);
    request.opcode = htole16(request.opcode);  // endian 변환

    return hciSocket.write(pRequest, sizeof(request) + request.plen);
}

bool HciAdapter::setAdapterName(const std::string& name) {
    struct {
        HciHeader header;
        char name[248];
    } __attribute__((packed)) request;

    request.header.type = 0x01;
    request.header.opcode = CMD_SET_LOCAL_NAME;
    request.header.plen = sizeof(request) - sizeof(HciHeader);

    memset(request.name, 0, sizeof(request.name));
    strncpy(request.name, name.c_str(), sizeof(request.name) - 1);

    return sendCommand(request.header);
}

bool HciAdapter::setAdvertisingEnabled(bool enabled) {
    struct {
        HciHeader header;
        uint8_t enable;
    } __attribute__((packed)) request;

    request.header.type = 0x01;
    request.header.opcode = CMD_SET_ADVERTISING;
    request.header.plen = 1;  // 파라미터 크기
    request.enable = enabled ? 0x01 : 0x00;

    return sendCommand(request.header);
}

bool HciAdapter::setPowered(bool powered) {
    struct {
        HciHeader header;
        uint8_t powered;
    } __attribute__((packed)) request;

    request.header.type = 0x01;
    request.header.opcode = CMD_SET_POWERED;
    request.header.plen = sizeof(request) - sizeof(HciHeader);
    request.powered = powered ? 0x01 : 0x00;

    return sendCommand(request.header);
}

bool HciAdapter::setLEEnabled(bool enabled) {
    struct {
        HciHeader header;
        uint8_t enable;
    } __attribute__((packed)) request;

    request.header.type = 0x01;
    request.header.opcode = CMD_SET_LE;
    request.header.plen = sizeof(request) - sizeof(HciHeader);
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

        if (response.size() < 3) {  // 최소 이벤트 헤더 크기
            Logger::error("Received invalid HCI event (too short)");
            continue;
        }

        uint8_t eventCode = response[0];
        uint8_t parameterLength = response[1];
        const uint8_t* parameters = response.data() + 2;

        switch (eventCode) {
            case 0x0E:  // Command Complete Event
                handleCommandComplete(parameters, parameterLength);
                break;
            case 0x0F:  // Command Status Event
                handleCommandStatus(parameters, parameterLength);
                break;
            default:
                Logger::debug("Received unhandled HCI event: " + Utils::hex(eventCode));
                break;
        }
    }

    Logger::debug("Stopped HCI event processing thread");
}

void HciAdapter::handleCommandComplete(const uint8_t* data, uint8_t length) {
    if (length < 3) return;
    
    uint8_t numCommands = data[0];
    uint16_t opcode = data[1] | (data[2] << 8);
    uint8_t status = (length > 3) ? data[3] : 0xFF;

    std::string result;
    switch(opcode) {
        case CMD_SET_POWERED:
            result = "Set Powered";
            break;
        case CMD_SET_LE:
            result = "Set LE Mode";
            break;
        case CMD_SET_BREDR:
            result = "Set BR/EDR Mode";
            break;
        case CMD_SET_ADVERTISING:
            result = "Set Advertising";
            break;
        case CMD_SET_LOCAL_NAME:
            result = "Set Local Name";
            break;
        default:
            result = "Unknown Command";
    }

    Logger::debug("Command Complete: " + result + 
                 " (opcode=0x" + Utils::hex(opcode) + 
                 ", status=0x" + Utils::hex(status) + ")");
}

void HciAdapter::handleCommandStatus(const uint8_t* data, uint8_t length) {
    if (length < 3) return;
    
    uint8_t status = data[0];
    uint16_t opcode = data[1] | (data[2] << 8);

    Logger::debug("Command Status: opcode=" + Utils::hex(opcode) + 
                 " status=" + Utils::hex(status));
}
} // namespace ggk