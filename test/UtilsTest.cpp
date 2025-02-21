#include <gtest/gtest.h>
#include "../include/Utils.h"

using namespace ggk;

// ✅ 문자열 Trim 테스트
TEST(UtilsTest, TrimFunctions) {
    std::string str = "  Hello World  ";
    
    // trimInPlace()
    Utils::trimInPlace(str);
    EXPECT_EQ(str, "Hello World");

    // trimBegin()
    EXPECT_EQ(Utils::trimBegin("  Test"), "Test");

    // trimEnd()
    EXPECT_EQ(Utils::trimEnd("Test  "), "Test");
}

// ✅ Hex 변환 함수 테스트
TEST(UtilsTest, HexFunctions) {
    EXPECT_EQ(Utils::hex((uint8_t)0xA), "0x0A");
    EXPECT_EQ(Utils::hex((uint16_t)0xAB), "0x00AB");
    EXPECT_EQ(Utils::hex((uint32_t)0xABCD), "0x0000ABCD");
}

// ✅ Bluetooth MAC 주소 변환 테스트
TEST(UtilsTest, BluetoothAddressString) {
    uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    EXPECT_EQ(Utils::bluetoothAddressString(mac), "12:34:56:78:9A:BC");
}

// ✅ GVariant 변환 테스트
TEST(UtilsTest, GVariantFromString) {
    GVariant *variant = Utils::gvariantFromString("Hello");
    const gchar *str = g_variant_get_string(variant, nullptr);
    EXPECT_STREQ(str, "Hello");
    g_variant_unref(variant);
}

// ✅ GVariant Boolean 변환 테스트
TEST(UtilsTest, GVariantFromBoolean) {
    GVariant *variant = Utils::gvariantFromBoolean(true);
    EXPECT_TRUE(g_variant_get_boolean(variant));
    g_variant_unref(variant);
}

// ✅ GVariant ByteArray 변환 테스트
TEST(UtilsTest, GVariantFromByteArray) {
    std::vector<guint8> bytes = {0x01, 0x02, 0x03};
    GVariant *variant = Utils::gvariantFromByteArray(bytes);
    
    gsize size;
    const guint8 *data = (const guint8 *)g_variant_get_fixed_array(variant, &size, sizeof(guint8));
    
    EXPECT_EQ(size, bytes.size());
    EXPECT_EQ(data[0], 0x01);
    EXPECT_EQ(data[1], 0x02);
    EXPECT_EQ(data[2], 0x03);

    g_variant_unref(variant);
}
