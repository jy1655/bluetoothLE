// test/DBusTestEnvironment.h
#pragma once
#include "DBusManager.h"
#include <gtest/gtest.h>


namespace ggk {


class DBusTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // 테스트 시작 전 환경 준비
        #ifdef TESTING
        // 테스트에서는 세션 버스 사용 (시스템 버스는 권한 필요)
        DBusManager::getInstance().reset();
        DBusManager::getInstance().setBusType(G_BUS_TYPE_SESSION);
        #endif
        
        // 테스트용 버스 네임으로 초기화
        if (!DBusManager::getInstance().initialize("org.example.ble.test")) {
            FAIL() << "Failed to initialize D-Bus for testing";
        }
    }
    
    void TearDown() override {
        // 테스트 종료 후 정리
        DBusManager::getInstance().shutdown();
    }
};

} // namespace ggk