#pragma once

#include "DBusName.h"
#include <gtest/gtest.h>

namespace ggk {

class DBusTestEnvironment : public ::testing::Environment {
public:
    static DBusConnection& getConnection();

    void SetUp() override;
    void TearDown() override;

    // 외부에서 버스 이름 지정 가능하게 설정
    static void setTestBusName(const std::string& name);

private:
    static std::string testBusName;
};

} // namespace ggk
