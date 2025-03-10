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

// ✅ gvariantFromString 변환 테스트
TEST(UtilsTest, GvariantFromString) {
    // 일반 문자열 테스트
    GVariant* result = Utils::gvariantFromString("test");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "s");
    EXPECT_STREQ(g_variant_get_string(result, nullptr), "test");
    g_variant_unref(result);
    
    // 빈 문자열 테스트
    result = Utils::gvariantFromString("");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "s");
    EXPECT_STREQ(g_variant_get_string(result, nullptr), "");
    g_variant_unref(result);
}

// gvariantFromStringArray (std::vector<std::string>) 테스트
TEST(UtilsTest, GvariantFromStringArray) {
    // 정상 배열 테스트
    std::vector<std::string> strArray = {"one", "two", "three"};
    GVariant* result = Utils::gvariantFromStringArray(strArray);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "as");
    
    // 배열 요소 검증
    gsize size;
    EXPECT_EQ(g_variant_n_children(result), 3);
    g_variant_unref(result);
    
    // 빈 배열 테스트 - 가장 중요한 테스트!
    std::vector<std::string> emptyArray;
    result = Utils::gvariantFromStringArray(emptyArray);
    ASSERT_NE(result, nullptr) << "gvariantFromStringArray with empty array returned nullptr";
    EXPECT_STREQ(g_variant_get_type_string(result), "as") << "Expected type 'as', got: " 
                                                          << g_variant_get_type_string(result);
    EXPECT_EQ(g_variant_n_children(result), 0) << "Empty array should have 0 children";
    g_variant_unref(result);
}

// ✅ GVariant Boolean 변환 테스트
TEST(UtilsTest, GvariantFromBoolean) {
    // true 테스트
    GVariant* result = Utils::gvariantFromBoolean(true);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "b");
    EXPECT_TRUE(g_variant_get_boolean(result));
    g_variant_unref(result);
    
    // false 테스트
    result = Utils::gvariantFromBoolean(false);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "b");
    EXPECT_FALSE(g_variant_get_boolean(result));
    g_variant_unref(result);
}

// ✅ GVariant ByteArray 변환 테스트
TEST(UtilsTest, GvariantFromByteArray) {
    // 정상 배열 테스트
    std::vector<guint8> byteArray = {0x01, 0x02, 0x03};
    GVariant* result = Utils::gvariantFromByteArray(byteArray);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "ay");
    
    // 배열 크기 확인
    gsize size;
    const guint8* data = (const guint8*)g_variant_get_fixed_array(result, &size, 1);
    EXPECT_EQ(size, 3);
    EXPECT_EQ(data[0], 0x01);
    EXPECT_EQ(data[1], 0x02);
    EXPECT_EQ(data[2], 0x03);
    g_variant_unref(result);
    
    // 빈 바이트 배열 테스트
    std::vector<guint8> emptyByteArray;
    result = Utils::gvariantFromByteArray(emptyByteArray);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "ay");
    data = (const guint8*)g_variant_get_fixed_array(result, &size, 1);
    EXPECT_EQ(size, 0);
    g_variant_unref(result);
}

// gvariantFromObject 테스트
TEST(UtilsTest, GvariantFromObject) {
    // 객체 경로 테스트
    DBusObjectPath path("/org/test/path");
    GVariant* result = Utils::gvariantFromObject(path);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "o");
    EXPECT_STREQ(g_variant_get_string(result, nullptr), "/org/test/path");
    g_variant_unref(result);
}