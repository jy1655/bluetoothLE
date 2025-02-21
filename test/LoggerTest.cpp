#include <gtest/gtest.h>
#include "../include/Logger.h"
#include <string>

using namespace ggk;

// ✅ 로그 메시지를 저장할 변수
std::string lastLogMessage;

// ✅ 로그 메시지를 받는 콜백 함수
void TestLogReceiver(const char* message) {
    lastLogMessage = message;
}

// ✅ 1. `debug()` 로그 테스트
TEST(LoggerTest, DebugLoggingTest) {
    Logger::registerDebugReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::debug("Test Debug Message");
    EXPECT_EQ(lastLogMessage, "Test Debug Message");
}

// ✅ 2. `info()` 로그 테스트
TEST(LoggerTest, InfoLoggingTest) {
    Logger::registerInfoReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::info("Test Info Message");
    EXPECT_EQ(lastLogMessage, "Test Info Message");
}

// ✅ 3. `status()` 로그 테스트
TEST(LoggerTest, StatusLoggingTest) {
    Logger::registerStatusReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::status("Test Status Message");
    EXPECT_EQ(lastLogMessage, "Test Status Message");
}

// ✅ 4. `warn()` 로그 테스트
TEST(LoggerTest, WarnLoggingTest) {
    Logger::registerWarnReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::warn("Test Warn Message");
    EXPECT_EQ(lastLogMessage, "Test Warn Message");
}

// ✅ 5. `error()` 로그 테스트
TEST(LoggerTest, ErrorLoggingTest) {
    Logger::registerErrorReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::error("Test Error Message");
    EXPECT_EQ(lastLogMessage, "Test Error Message");
}

// ✅ 6. `fatal()` 로그 테스트
TEST(LoggerTest, FatalLoggingTest) {
    Logger::registerFatalReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::fatal("Test Fatal Message");
    EXPECT_EQ(lastLogMessage, "Test Fatal Message");
}

// ✅ 7. `always()` 로그 테스트
TEST(LoggerTest, AlwaysLoggingTest) {
    Logger::registerAlwaysReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::always("Test Always Message");
    EXPECT_EQ(lastLogMessage, "Test Always Message");
}

// ✅ 8. `trace()` 로그 테스트
TEST(LoggerTest, TraceLoggingTest) {
    Logger::registerTraceReceiver(TestLogReceiver);
    lastLogMessage.clear();

    Logger::trace("Test Trace Message");
    EXPECT_EQ(lastLogMessage, "Test Trace Message");
}
