#include "Mgmt.h"
#include "Logger.h"
#include <string.h>

namespace ggk {

Mgmt::Mgmt(HciAdapter& adapter, uint16_t controllerIndex)
    : hciAdapter(adapter)
    , controllerIndex(controllerIndex) {
}

bool Mgmt::setName(std::string name, std::string shortName) {
    name = truncateName(name);
    shortName = truncateShortName(shortName);

    struct {
        HciAdapter::HciHeader header;
        char name[249];
        char shortName[11];
    } __attribute__((packed)) request;

    request.header.code = HciAdapter::CMD_SET_LOCAL_NAME;
    request.header.controllerId = controllerIndex;
    request.header.dataSize = sizeof(request) - sizeof(HciAdapter::HciHeader);

    memset(request.name, 0, sizeof(request.name));
    strncpy(request.name, name.c_str(), sizeof(request.name) - 1);

    memset(request.shortName, 0, sizeof(request.shortName));
    strncpy(request.shortName, shortName.c_str(), sizeof(request.shortName) - 1);

    return hciAdapter.sendCommand(request.header);
}

bool Mgmt::setDiscoverable(uint8_t disc, uint16_t timeout) {
    struct {
        HciAdapter::HciHeader header;
        uint8_t disc;
        uint16_t timeout;
    } __attribute__((packed)) request;

    request.header.code = HciAdapter::CMD_SET_DISCOVERABLE;
    request.header.controllerId = controllerIndex;
    request.header.dataSize = sizeof(request) - sizeof(HciAdapter::HciHeader);
    request.disc = disc;
    request.timeout = timeout;

    return hciAdapter.sendCommand(request.header);
}

bool Mgmt::setState(uint16_t commandCode, uint16_t controllerId, uint8_t newState) {
    struct {
        HciAdapter::HciHeader header;
        uint8_t state;
    } __attribute__((packed)) request;

    request.header.code = commandCode;
    request.header.controllerId = controllerId;
    request.header.dataSize = sizeof(request) - sizeof(HciAdapter::HciHeader);
    request.state = newState;

    return hciAdapter.sendCommand(request.header);
}

bool Mgmt::setPowered(bool newState) {
    return setState(HciAdapter::CMD_SET_POWERED, controllerIndex, newState ? 1 : 0);
}

bool Mgmt::setBredr(bool newState) {
    return setState(HciAdapter::CMD_SET_BREDR, controllerIndex, newState ? 1 : 0);
}

bool Mgmt::setSecureConnections(uint8_t newState) {
    return setState(HciAdapter::CMD_SET_SECURE_CONN, controllerIndex, newState);
}

bool Mgmt::setBondable(bool newState) {
    return setState(HciAdapter::CMD_SET_BONDABLE, controllerIndex, newState ? 1 : 0);
}

bool Mgmt::setConnectable(bool newState) {
    return setState(HciAdapter::CMD_SET_CONNECTABLE, controllerIndex, newState ? 1 : 0);
}

bool Mgmt::setLE(bool newState) {
    return setState(HciAdapter::CMD_SET_LE, controllerIndex, newState ? 1 : 0);
}

bool Mgmt::setAdvertising(uint8_t newState) {
    return setState(HciAdapter::CMD_SET_ADVERTISING, controllerIndex, newState);
}

std::string Mgmt::truncateName(const std::string& name) {
    if (name.length() <= kMaxAdvertisingNameLength) {
        return name;
    }
    return name.substr(0, kMaxAdvertisingNameLength);
}

std::string Mgmt::truncateShortName(const std::string& name) {
    if (name.length() <= kMaxAdvertisingShortNameLength) {
        return name;
    }
    return name.substr(0, kMaxAdvertisingShortNameLength);
}

} // namespace ggk