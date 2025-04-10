#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include <sys/ioctl.h>       // ioctl 함수 정의
#include <linux/ioctl.h>     // _IOR 매크로 정의
#include <bluetooth/hci_lib.h>  // hci_xxx 함수들

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
    disconnect();  // Ensure we're disconnected first

    Logger::info("Connecting to HCI socket");

    // Check if Bluetooth service is running
    if (system("systemctl is-active --quiet bluetooth.service") != 0) {
        Logger::warn("Bluetooth service not running, device may not be available");
    }

    // Check if HCI device exists
    if (system("ls /sys/class/bluetooth/hci0 > /dev/null 2>&1") != 0) {
        Logger::error("HCI device not found. Check if Bluetooth adapter is present");
        return false;
    }

    // Try to create the socket
    fdSocket = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (fdSocket < 0) {
        logErrno("Connect(socket)");
        return false;
    }

    // Try to get device info
    struct hci_dev_info di;
    int dev_id = 0;  // hci0
    
    if (ioctl(fdSocket, HCIGETDEVINFO, (void *)&di) < 0) {
        // Device info retrieval failed, try to reset device
        Logger::warn("Could not get device info, attempting reset");
        system("sudo hciconfig hci0 down");
        system("sudo hciconfig hci0 up");
        usleep(200000);  // 200ms wait
    }

    // Set up the socket address
    struct sockaddr_hci addr;
    memset(&addr, 0, sizeof(addr));
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = dev_id;
    addr.hci_channel = HCI_CHANNEL_RAW;

    // Try to bind to the socket
    if (bind(fdSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        // If binding fails, try to reset HCI device and retry
        Logger::warn("Bind failed, attempting to reset HCI device");
        
        logErrno("Connect(bind) - first attempt");
        
        // Reset the device and retry
        system("sudo hciconfig hci0 down");
        usleep(100000);  // 100ms wait
        system("sudo hciconfig hci0 up");
        usleep(300000);  // 300ms wait
        
        // Retry the bind
        if (bind(fdSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            logErrno("Connect(bind) - second attempt");
            disconnect();
            return false;
        }
    }

    // Set up HCI filter
    struct hci_filter flt;
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_all_events(&flt);
    
    if (setsockopt(fdSocket, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        logErrno("Failed to set HCI filter");
        disconnect();
        return false;
    }

    // Enable non-blocking mode
    int flags = fcntl(fdSocket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fdSocket, F_SETFL, flags | O_NONBLOCK);
    }

    Logger::info("Connected to HCI device " + std::to_string(dev_id) + 
                " (fd = " + std::to_string(fdSocket) + ")");
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


bool HciSocket::write(const uint8_t *pBuffer, size_t count) const {
    std::string dump = "";
    dump += "  > Writing " + std::to_string(count) + " bytes\n";
    dump += Utils::hex(pBuffer, count);
    Logger::debug(dump);

    // 형 변환 경고 수정
    ssize_t written = ::write(fdSocket, pBuffer, count);
    if (written < 0 || static_cast<size_t>(written) != count) {
        logErrno("write");
        return false;
    }

    return true;
}

bool HciSocket::write(std::vector<uint8_t> buffer) const {
    return write(buffer.data(), buffer.size());
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