#include <gtest/gtest.h>
#include "Utils.h"

using namespace ggk;

// ✅ 문자열 Trim 테스트
TEST(UtilsTest, TrimFunctions) {
    std::string str = "  Hello World  ";
    Utils::trimInPlace(str);
    EXPECT_EQ(str, "Hello World");

    EXPECT_EQ(Utils::trimBegin("  Test"), "Test");
    EXPECT_EQ(Utils::trimEnd("Test  "), "Test");
    EXPECT_EQ(Utils::trim("  Hello  "), "Hello");
}

// ✅ Hex 변환 함수 테스트
TEST(UtilsTest, HexFunctions) {
    EXPECT_EQ(Utils::hex((uint8_t)0x0A), "0x0A");
    EXPECT_EQ(Utils::hex((uint16_t)0x00AB), "0x00AB");
    EXPECT_EQ(Utils::hex((uint32_t)0x0000ABCD), "0x0000ABCD");
}

// ✅ Bluetooth MAC 주소 변환 테스트
TEST(UtilsTest, BluetoothAddressString) {
    uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    EXPECT_EQ(Utils::bluetoothAddressString(mac), "12:34:56:78:9A:BC");
    EXPECT_EQ(Utils::bluetoothAddressString(nullptr), "[invalid address]");
}

// ✅ GVariant 문자열 테스트
TEST(UtilsTest, GvariantFromString) {
    GVariantPtr result = Utils::gvariantPtrFromString("test");
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "s");
    EXPECT_STREQ(g_variant_get_string(result.get(), nullptr), "test");

    result = Utils::gvariantPtrFromString("");
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "s");
    EXPECT_STREQ(g_variant_get_string(result.get(), nullptr), "");
}

// ✅ GVariant 문자열 배열 테스트
TEST(UtilsTest, GvariantFromStringArray) {
    std::vector<std::string> arr = {"one", "two", "three"};
    GVariantPtr result = Utils::gvariantPtrFromStringArray(arr);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "as");
    EXPECT_EQ(g_variant_n_children(result.get()), 3);

    std::vector<std::string> emptyArr;
    result = Utils::gvariantPtrFromStringArray(emptyArr);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "as");
    EXPECT_EQ(g_variant_n_children(result.get()), 0);
}

// ✅ GVariant Boolean 테스트
TEST(UtilsTest, GvariantFromBoolean) {
    GVariantPtr result = Utils::gvariantPtrFromBoolean(true);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "b");
    EXPECT_TRUE(g_variant_get_boolean(result.get()));

    result = Utils::gvariantPtrFromBoolean(false);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "b");
    EXPECT_FALSE(g_variant_get_boolean(result.get()));
}

// ✅ GVariant Byte Array 테스트
TEST(UtilsTest, GvariantFromByteArray) {
    std::vector<guint8> bytes = {0x01, 0x02, 0x03};
    GVariantPtr result = Utils::gvariantPtrFromByteArray(bytes);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "ay");

    gsize size;
    const guint8* data = static_cast<const guint8*>(g_variant_get_fixed_array(result.get(), &size, 1));
    ASSERT_EQ(size, 3);
    EXPECT_EQ(data[0], 0x01);
    EXPECT_EQ(data[1], 0x02);
    EXPECT_EQ(data[2], 0x03);

    std::vector<guint8> empty;
    result = Utils::gvariantPtrFromByteArray(empty);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "ay");
    data = static_cast<const guint8*>(g_variant_get_fixed_array(result.get(), &size, 1));
    EXPECT_EQ(size, 0);
}

// ✅ GVariant Object Path 테스트
TEST(UtilsTest, GvariantFromObject) {
    DBusObjectPath path("/org/test/path");
    GVariantPtr result = Utils::gvariantPtrFromObject(path);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "o");
    EXPECT_STREQ(g_variant_get_string(result.get(), nullptr), "/org/test/path");

    DBusObjectPath emptyPath("");
    result = Utils::gvariantPtrFromObject(emptyPath);
    ASSERT_TRUE(result);
    EXPECT_STREQ(g_variant_get_type_string(result.get()), "o");
    EXPECT_STREQ(g_variant_get_string(result.get(), nullptr), "/");
}
