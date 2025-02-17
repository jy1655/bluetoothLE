#pragma once

#include <vector>
#include <atomic>
#include <cstdint>

namespace ggk {

class HciSocket {
public:
    // Initializes an unconnected socket
    HciSocket();

    // Socket destructor
    // This will automatically disconnect the socket if it is currently connected
    ~HciSocket();

    // Connects to an HCI socket using the Bluetooth Management API protocol
    // Returns true on success, otherwise false
    bool connect();

    // Returns true if the socket is currently connected, otherwise false
    bool isConnected() const;

    // Disconnects from the HCI socket
    void disconnect();

    // 소켓 실행 중지
    void stop();

    // Reads data from the HCI socket
    // Raw data is read and returned in `response`.
    bool read(std::vector<uint8_t> &response) const;

    // Writes the array of bytes
    bool write(std::vector<uint8_t> buffer) const;
    bool write(const uint8_t *pBuffer, size_t count) const;

private:
    // Wait for data to arrive, or for a shutdown event
    bool waitForDataOrShutdown() const;

    // Utilitarian function for logging errors for the given operation
    void logErrno(const char *pOperation) const;

    int fdSocket;
    std::atomic<bool> isRunning;
    
    static constexpr size_t kResponseMaxSize = 64 * 1024;
    static constexpr int kDataWaitTimeMS = 10;
};

} // namespace ggk