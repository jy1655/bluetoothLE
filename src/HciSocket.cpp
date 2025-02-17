#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include "HciSocket.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

// Initializes an unconnected socket
HciSocket::HciSocket()
    : fdSocket(-1)
    , isRunning(true) {
}

HciSocket::~HciSocket() {
    stop();
    disconnect();
}

void HciSocket::stop() {
    isRunning = false;
}

// Connects to an HCI socket using the Bluetooth Management API protocol
//
// Returns true on success, otherwise false
bool HciSocket::connect() {
    disconnect();

    // USER 채널로 변경
    fdSocket = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (fdSocket < 0) {
        logErrno("Connect(socket)");
        return false;
    }

    // Get HCI device info
    struct hci_dev_list_req *dl;
    struct hci_dev_req *dr;
    
    dl = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
    if (!dl) {
        Logger::error("Failed to allocate memory");
        return false;
    }
    
    dl->dev_num = HCI_MAX_DEV;
    dr = dl->dev_req;

    if (ioctl(fdSocket, HCIGETDEVLIST, (void *)dl) < 0) {
        free(dl);
        logErrno("Failed to get HCI device list");
        return false;
    }

    int dev_id = -1;
    for (int i = 0; i < dl->dev_num; i++) {
        dev_id = dr[i].dev_id;
        break;  // Use first available device
    }
    free(dl);

    if (dev_id < 0) {
        Logger::error("No HCI devices found");
        return false;
    }

    struct sockaddr_hci addr;
    memset(&addr, 0, sizeof(addr));
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = dev_id;
    addr.hci_channel = HCI_CHANNEL_USER;

    if (bind(fdSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logErrno("Connect(bind)");
        disconnect();
        return false;
    }

    // Set filter to receive HCI events
    struct hci_filter flt;
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_all_events(&flt);
    
    if (setsockopt(fdSocket, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        logErrno("Failed to set HCI filter");
        disconnect();
        return false;
    }

    Logger::debug(SSTR << "Connected to HCI device " << dev_id << " (fd = " << fdSocket << ")");

    return true;
}

// Returns true if the socket is currently connected, otherwise false
bool HciSocket::isConnected() const
{
	return fdSocket >= 0;
}

// Disconnects from the HCI socket
void HciSocket::disconnect()
{
	if (isConnected())
	{
		Logger::debug("HciSocket disconnecting");

		if (close(fdSocket) != 0)
		{
			logErrno("close(fdSocket)");
		}

		fdSocket = -1;
		Logger::trace("HciSocket closed");
	}
}

// Reads data from the HCI socket
//
// Raw data is read and returned in `response`.
//
// Returns true if data was read successfully, otherwise false is returned. A false return code does not necessarily depict
// an error, as this can arise from expected conditions (such as an interrupt.)
bool HciSocket::read(std::vector<uint8_t> &response) const
{
	// Fill our response with empty data
	response.resize(kResponseMaxSize, 0);

	// Wait for data or a cancellation
	if (!waitForDataOrShutdown())
	{
		return false;
	}

	// Block until we receive data, a disconnect, or a signal
	ssize_t bytesRead = ::recv(fdSocket, &response[0], kResponseMaxSize, MSG_WAITALL);

	// If there was an error, wipe the data and return an error condition
	if (bytesRead < 0)
	{
		if (errno == EINTR)
		{
			Logger::debug("HciSocket receive interrupted");
		}
		else
		{
			logErrno("recv");
		}
		response.resize(0);
		return false;
	}
	else if (bytesRead == 0)
	{
		Logger::error("Peer closed the socket");
		response.resize(0);
		return false;
	}

	// We have data
	response.resize(bytesRead);

	std::string dump = "";
	dump += "  > Read " + std::to_string(response.size()) + " bytes\n";
	dump += Utils::hex(response.data(), response.size());
	Logger::debug(dump);

	return true;
}

// Writes the array of bytes of a given count
//
// This method returns true if the bytes were written successfully, otherwise false
bool HciSocket::write(const uint8_t *pBuffer, size_t count) const {
    std::string dump = "";
    dump += "  > Writing " + std::to_string(count) + " bytes\n";
    dump += Utils::hex(pBuffer, count);
    Logger::debug(dump);

    // HCI 명령어 패킷 구조체 생성
    struct hci_command_hdr hdr;
    hdr.opcode = pBuffer[1] | (pBuffer[2] << 8);  // LSB | (MSB << 8)
    hdr.plen = count - 3;  // 헤더(3바이트) 제외한 실제 파라미터 길이

    // 헤더 먼저 전송
    if (::write(fdSocket, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        logErrno("write(header)");
        return false;
    }

    // 파라미터가 있다면 전송
    if (hdr.plen > 0) {
        if (::write(fdSocket, pBuffer + 3, hdr.plen) != hdr.plen) {
            logErrno("write(parameters)");
            return false;
        }
    }

    return true;
}

// Writes the array of bytes of a given count
//
// This method returns true if the bytes were written successfully, otherwise false
bool HciSocket::write(const uint8_t *pBuffer, size_t count) const
{
	std::string dump = "";
	dump += "  > Writing " + std::to_string(count) + " bytes\n";
	dump += Utils::hex(pBuffer, count);
	Logger::debug(dump);

	size_t len = ::write(fdSocket, pBuffer, count);

	if (len != count)
	{
		logErrno("write");
		return false;
	}

	return true;
}

// Wait for data to arrive, or for a shutdown event
//
// Returns true if data is available, false if we are shutting down
bool HciSocket::waitForDataOrShutdown() const {
    while(isRunning) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fdSocket, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = kDataWaitTimeMS * 1000;

        int retval = select(fdSocket+1, &rfds, NULL, NULL, &tv);

        if (retval > 0) { return true; }
        if (retval < 0) { return false; }
        continue;
    }
    return false;
}

// Utilitarian function for logging errors for the given operation
void HciSocket::logErrno(const char *pOperation) const
{
	std::string errorDetail(strerror(errno));
	if (errno == EAGAIN)
	{
		errorDetail += " or not enough permission for this operation";
	}

	Logger::error(SSTR << "Error on Bluetooth management socket during " << pOperation << " operation. Error code " << errno << ": " << errorDetail);
}

}; // namespace ggk