// test/DBusTestEnvironment.h
#pragma once
#include "DBusName.h"
#include <gtest/gtest.h>
#include "GattApplication.h"
#include <memory>

namespace ggk {

// 전역 테스트 환경 설정을 위한 클래스
class DBusTestEnvironment : public ::testing::Environment {
public:
    // 정적 메서드로 공유 연결에 접근할 수 있게 함
    static DBusConnection& getConnection();
    
    // GattApplication 공유 인스턴스 접근
    static GattApplication* getGattApplication();

    void SetUp() override;
    void TearDown() override;

private:
    // 정적 멤버 변수 선언 (여기서는 정의하지 않음)
    static std::unique_ptr<GattApplication> application;
};

} // namespace ggk