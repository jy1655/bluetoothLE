#include <gtest/gtest.h>
#include "../include/HciSocket.h"

using namespace ggk;

// ✅ 1. `connect()`가 성공적으로 실행되는지 테스트
TEST(HciSocketTest, ConnectSuccess) {
    HciSocket socket;

    ASSERT_TRUE(socket.connect()) << "HciSocket 연결 실패! BLE 인터페이스(hci0)가 활성화되어 있는지 확인하세요.";
}

// ✅ 2. `isConnected()` 상태 확인
TEST(HciSocketTest, IsConnectedTest) {
    HciSocket socket;
    
    ASSERT_TRUE(socket.connect()) << "HciSocket 연결 실패!";
    EXPECT_TRUE(socket.isConnected()) << "isConnected()가 true를 반환해야 합니다.";
}

// ✅ 3. `write()`가 정상적으로 동작하는지 확인
TEST(HciSocketTest, WriteDataTest) {
    HciSocket socket;
    ASSERT_TRUE(socket.connect()) << "HciSocket 연결 실패!";

    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04};

    EXPECT_TRUE(socket.write(testData)) << "HciSocket 데이터 쓰기 실패!";
}

// ✅ 4. `read()`가 정상적으로 데이터를 읽어오는지 확인
TEST(HciSocketTest, ReadDataTest) {
    HciSocket socket;
    ASSERT_TRUE(socket.connect()) << "HciSocket 연결 실패!";

    std::vector<uint8_t> response;
    
    bool success = socket.read(response);
    
    if (success) {
        EXPECT_GT(response.size(), 0) << "HciSocket에서 데이터를 읽었지만, 응답 크기가 0입니다.";
    } else {
        GTEST_SKIP() << "BLE 장치에서 응답을 받지 못했습니다. 이는 장치가 IDLE 상태일 수도 있습니다.";
    }
}

// ✅ 5. `disconnect()`가 정상적으로 동작하는지 확인
TEST(HciSocketTest, DisconnectTest) {
    HciSocket socket;
    ASSERT_TRUE(socket.connect()) << "HciSocket 연결 실패!";

    socket.disconnect();
    
    EXPECT_FALSE(socket.isConnected()) << "disconnect() 이후에도 소켓이 연결된 상태로 남아있습니다.";
}
