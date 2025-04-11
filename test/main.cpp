#include <gtest/gtest.h>
#include "Logger.h"

#include "DBusTestEnvironment.h" // DBus 이후 테스트부터 필요

void initLogger() {
    // 표준 출력으로 로그 출력
    ggk::Logger::registerDebugReceiver([](const char* msg) { std::cout << "DEBUG: " << msg << std::endl; });
    ggk::Logger::registerInfoReceiver([](const char* msg) { std::cout << "INFO: " << msg << std::endl; });
    ggk::Logger::registerErrorReceiver([](const char* msg) { std::cerr << "ERROR: " << msg << std::endl; });
    ggk::Logger::registerWarnReceiver([](const char* msg) { std::cout << "WARN: " << msg << std::endl; });
    ggk::Logger::registerStatusReceiver([](const char* msg) { std::cout << "STATUS: " << msg << std::endl; });
    ggk::Logger::registerFatalReceiver([](const char* msg) { std::cerr << "FATAL: " << msg << std::endl; });
    ggk::Logger::registerAlwaysReceiver([](const char* msg) { std::cout << "ALWAYS: " << msg << std::endl; });
    ggk::Logger::registerTraceReceiver([](const char* msg) { std::cout << "TRACE: " << msg << std::endl; });
}

// Google Test 실행을 위한 main 함수
int main(int argc, char **argv) {
    initLogger();

    ggk::Logger::info("Logger initialized for tests");

    ::testing::InitGoogleTest(&argc, argv);

    // DBus 네임 관련 테스트 환경 등록- DBus 이전 테스트 진행시 주석처리 해도 무방
    //::testing::AddGlobalTestEnvironment(new ggk::DBusTestEnvironment());

    return RUN_ALL_TESTS();
}
