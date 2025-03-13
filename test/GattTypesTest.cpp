#include <gtest/gtest.h>
#include "../include/GattTypes.h"

using namespace ggk;

// UUID 유효성 테스트
TEST(GattUuidTest, ValidUUID) {
    EXPECT_NO_THROW(GattUuid("12345678-1234-5678-1234-567812345678"));
    EXPECT_NO_THROW(GattUuid("12345678123456781234567812345678"));
}

TEST(GattUuidTest, InvalidUUID) {
    EXPECT_THROW(GattUuid("invalid-uuid-format"), std::invalid_argument);
    EXPECT_THROW(GattUuid("12345"), std::invalid_argument);
}

// Short UUID 변환 테스트
TEST(GattUuidTest, FromShortUuid) {
    GattUuid uuid = GattUuid::fromShortUuid(0x180D);
    EXPECT_EQ(uuid.toString(), "0000180d-0000-1000-8000-00805f9b34fb");
}

// BlueZ 포맷 변환 테스트
TEST(GattUuidTest, ToBlueZFormat) {
    GattUuid uuid("0000180D-0000-1000-8000-00805F9B34FB");
    EXPECT_EQ(uuid.toBlueZFormat(), "0000180d00001000800000805f9b34fb");
}
