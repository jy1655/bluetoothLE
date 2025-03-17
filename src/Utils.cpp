#include <algorithm>
#include <string.h>

#include "Utils.h"

namespace ggk {

// ---------------------------------------------------------------------------------------------------------------------------------
// Handy string functions
// ---------------------------------------------------------------------------------------------------------------------------------

// Trim from start (in place)
void Utils::trimBeginInPlace(std::string &str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(),
    [](int ch)
    {
        return !std::isspace(ch);
    }));
}

// Trim from end (in place)
void Utils::trimEndInPlace(std::string &str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(),
    [](int ch)
    {
        return !std::isspace(ch);
    }).base(), str.end());
}

// Trim from both ends (in place)
void Utils::trimInPlace(std::string &str)
{
    trimBeginInPlace(str);
    trimEndInPlace(str);
}

// Trim from start (copying)
std::string Utils::trimBegin(const std::string &str)
{
	std::string out = str;
    trimBeginInPlace(out);
    return out;
}

// Trim from end (copying)
std::string Utils::trimEnd(const std::string &str)
{
	std::string out = str;
    trimEndInPlace(out);
    return out;
}

// Trim from both ends (copying)
std::string Utils::trim(const std::string &str)
{
	std::string out = str;
    trimInPlace(out);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Hex output functions
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a zero-padded 8-bit hex value in the format: 0xA
std::string Utils::hex(uint8_t value)
{
	char hex[16];
	sprintf(hex, "0x%02X", value);
	return hex;
}

// Returns a zero-padded 8-bit hex value in the format: 0xAB
std::string Utils::hex(uint16_t value)
{
	char hex[16];
	sprintf(hex, "0x%04X", value);
	return hex;
}

// Returns a zero-padded 8-bit hex value in the format: 0xABCD
std::string Utils::hex(uint32_t value)
{
	char hex[16];
	sprintf(hex, "0x%08X", value);
	return hex;
}

// A full hex-dump of binary data (with accompanying ASCII output)
std::string Utils::hex(const uint8_t *pData, int count)
{
	char hex[16];

	// Hex data output
	std::string line;
	std::vector<std::string> hexData;
	for (int i = 0; i < count; ++i)
	{
		sprintf(hex, "%02X ", pData[i]);
		line += hex;

		if (line.length() >= 16 * 3)
		{
			hexData.push_back(line);
			line = "";
		}
	}

	if (!line.empty())
	{
		hexData.push_back(line);
		line = "";
	}

	// ASCII data output
	std::vector<std::string> asciiData;
	for (int i = 0; i < count; ++i)
	{
		// Unprintable?
		if (pData[i] < 0x20 || pData[i] > 0x7e)
		{
			line += ".";
		}
		else
		{
			line += pData[i];
		}

		if (line.length() >= 16)
		{
			asciiData.push_back(line);
			line = "";
		}
	}

	if (!line.empty())
	{
		asciiData.push_back(line);
	}

	std::string result = "";
	size_t dataSize = hexData.size();
	for (size_t i = 0; i < dataSize; ++i)
	{
		std::string hexPart = hexData[i];
		hexPart.insert(hexPart.length(), 48-hexPart.length(), ' ');

		std::string asciiPart = asciiData[i];
		asciiPart.insert(asciiPart.length(), 16-asciiPart.length(), ' ');

		result += std::string("    > ") + hexPart + "   [" + asciiPart + "]";

		if (i < dataSize - 1) { result += "\n"; }
	}

	return result;
}

// Returns a peoperly formatted Bluetooth address from a set of six octets stored at `pAddress`
//
// USE WITH CAUTION: It is expected that pAddress point to an array of 6 bytes. The length of the array cannot be validated and
// incorrect lengths will produce undefined, likely unwanted and potentially fatal results. Or it will return the address of the
// train at platform 9 3/4. You decide.
//
// This method returns a set of six zero-padded 8-bit hex values 8-bit in the format: 12:34:56:78:9A:BC
std::string Utils::bluetoothAddressString(uint8_t *pAddress)
{
	char hex[32];
	snprintf(hex, sizeof(hex), "%02X:%02X:%02X:%02X:%02X:%02X", 
		pAddress[0], pAddress[1], pAddress[2], pAddress[3], pAddress[4], pAddress[5]);
	return hex;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// GVariant helper functions - 원래 버전 (raw 포인터 반환)
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a GVariant containing a floating reference to a utf8 string
GVariant *Utils::gvariantFromString(const char *pStr)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromString(pStr);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns a GVariant containing a floating reference to a utf8 string
GVariant *Utils::gvariantFromString(const std::string &str)
{
    return gvariantFromString(str.c_str());
}

// Returns an array of strings ("as") with one string per variable argument.
//
// The array must be terminated with a nullptr.
//
// This is an extension method to the vararg version, which accepts pass-through variable arguments from other mthods.
GVariant *Utils::gvariantFromStringArray(const char *pStr, va_list args)
{
    // 항상 유효한 빌더 사용
    g_auto(GVariantBuilder) builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    // pStr이 NULL이 아니면 문자열들 추가
    if (pStr != nullptr)
    {
        const char *currentStr = pStr;
        while (currentStr != nullptr)
        {
            g_variant_builder_add(&builder, "s", currentStr);
            currentStr = va_arg(args, const char *);
        }
    }

    // g_variant_builder_end는 floating reference를 반환하므로 sink 필요 없음
    return g_variant_builder_end(&builder);
}

// Returns an array of strings ("as") with one string per variable argument.
//
// The array must be terminated with a nullptr.
GVariant *Utils::gvariantFromStringArray(const char *pStr, ...)
{
    // 항상 유효한 빌더 사용
    g_auto(GVariantBuilder) builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    // pStr이 NULL이 아니면 문자열들 추가
    if (pStr != nullptr)
    {
        va_list args;
        va_start(args, pStr);

        const char *currentStr = pStr;
        while (currentStr != nullptr)
        {
            g_variant_builder_add(&builder, "s", currentStr);
            currentStr = va_arg(args, const char *);
        }

        va_end(args);
    }

    return g_variant_builder_end(&builder);
}

// Returns an array of strings ("as") from an array of strings
GVariant* Utils::gvariantFromStringArray(const std::vector<std::string>& arr) {
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromStringArray(arr);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an array of strings ("as") from an array of C strings
GVariant *Utils::gvariantFromStringArray(const std::vector<const char *> &arr)
{
    g_auto(GVariantBuilder) builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    for (const char *pStr : arr)
    {
        if (pStr != nullptr) // NULL 체크
        {
            g_variant_builder_add(&builder, "s", pStr);
        }
    }

    return g_variant_builder_end(&builder);
}

// Returns an GVariant* containing an object path ("o") from an DBusObjectPath
GVariant *Utils::gvariantFromObject(const DBusObjectPath &path)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromObject(path);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an GVariant* containing a boolean
GVariant *Utils::gvariantFromBoolean(bool b)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromBoolean(b);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an GVariant* containing a 16-bit integer
GVariant *Utils::gvariantFromInt(gint16 value)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromInt(value);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an GVariant* containing a 32-bit integer
GVariant *Utils::gvariantFromInt(gint32 value)
{
    return g_variant_new_int32(value);
}

// Returns an array of bytes ("ay") with the contents of the input C string
GVariant *Utils::gvariantFromByteArray(const char *pStr)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromByteArray(pStr);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an array of bytes ("ay") with the contents of the input string
GVariant *Utils::gvariantFromByteArray(const std::string &str)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromByteArray(str);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
GVariant *Utils::gvariantFromByteArray(const guint8 *pBytes, int count)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromByteArray(pBytes, count);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
GVariant *Utils::gvariantFromByteArray(const std::vector<guint8> bytes)
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = gvariantPtrFromByteArray(bytes);
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Returns an array of bytes ("ay") containing a single unsigned 8-bit value
GVariant *Utils::gvariantFromByteArray(const guint8 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single signed 8-bit value
GVariant *Utils::gvariantFromByteArray(const gint8 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single unsigned 16-bit value
GVariant *Utils::gvariantFromByteArray(const guint16 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single signed 16-bit value
GVariant *Utils::gvariantFromByteArray(const gint16 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single unsigned 32-bit value
GVariant *Utils::gvariantFromByteArray(const guint32 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single signed 32-bit value
GVariant *Utils::gvariantFromByteArray(const gint32 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single unsigned 64-bit value
GVariant *Utils::gvariantFromByteArray(const guint64 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// Returns an array of bytes ("ay") containing a single signed 64-bit value
GVariant *Utils::gvariantFromByteArray(const gint64 data)
{
	return gvariantFromByteArray((const guint8 *) &data, sizeof(data));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 새로운 스마트 포인터 기반 GVariant 생성 함수들
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a GVariantPtr containing a string
GVariantPtr Utils::gvariantPtrFromString(const char* pStr)
{
    if (!pStr) {
        return makeNullGVariantPtr();
    }
    return makeGVariantPtr(g_variant_new_string(pStr));
}

// Returns a GVariantPtr containing a string
GVariantPtr Utils::gvariantPtrFromString(const std::string& str)
{
    return gvariantPtrFromString(str.c_str());
}

// Returns a GVariantPtr containing an array of strings ("as") from a vector of strings
GVariantPtr Utils::gvariantPtrFromStringArray(const std::vector<std::string>& arr)
{
    // 명시적으로 힙에 빌더 생성
    GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    
    for (const auto& str : arr) {
        g_variant_builder_add(builder, "s", str.c_str());
    }
    
    // 빌더로부터 GVariant 생성
    GVariant* result = g_variant_builder_end(builder);
    
    // 빌더 메모리 해제
    g_variant_builder_unref(builder);
    
    return makeGVariantPtr(result);
}

// Returns a GVariantPtr containing an array of strings ("as") from a vector of C strings
GVariantPtr Utils::gvariantPtrFromStringArray(const std::vector<const char*>& arr)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    for (const auto& str : arr) {
        if (str) {  // NULL 체크
            g_variant_builder_add(&builder, "s", str);
        }
    }

    GVariant* result = g_variant_builder_end(&builder);
    return makeGVariantPtr(result);
}

// Returns a GVariantPtr containing an object path ("o") from a DBusObjectPath
GVariantPtr Utils::gvariantPtrFromObject(const DBusObjectPath& path)
{
    return makeGVariantPtr(g_variant_new_object_path(path.c_str()));
}

// Returns a GVariantPtr containing a boolean
GVariantPtr Utils::gvariantPtrFromBoolean(bool b)
{
    return makeGVariantPtr(g_variant_new_boolean(b));
}

// Returns a GVariantPtr containing a 16-bit integer
GVariantPtr Utils::gvariantPtrFromInt(gint16 value)
{
    return makeGVariantPtr(g_variant_new_int16(value));
}

// Returns a GVariantPtr containing a 32-bit integer
GVariantPtr Utils::gvariantPtrFromInt(gint32 value)
{
    return makeGVariantPtr(g_variant_new_int32(value));
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a C string
GVariantPtr Utils::gvariantPtrFromByteArray(const char* pStr)
{
    if (!pStr || *pStr == 0) {
        // 빈 바이트 배열 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(reinterpret_cast<const guint8*>(pStr), strlen(pStr));
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a string
GVariantPtr Utils::gvariantPtrFromByteArray(const std::string& str)
{
    if (str.empty()) {
        // 빈 바이트 배열 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(reinterpret_cast<const guint8*>(str.c_str()), str.length());
}

// Returns a GVariantPtr containing an array of bytes ("ay") from raw byte data
GVariantPtr Utils::gvariantPtrFromByteArray(const guint8* pBytes, int count)
{
    if (!pBytes || count <= 0) {
        // 빈 바이트 배열 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }
    
    GBytes* bytes = g_bytes_new(pBytes, count);
    GVariant* variant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
    g_bytes_unref(bytes);
    
    return makeGVariantPtr(variant);
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a vector of bytes
GVariantPtr Utils::gvariantPtrFromByteArray(const std::vector<guint8> bytes)
{
    if (bytes.empty()) {
        // 빈 바이트 배열 생성
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(bytes.data(), bytes.size());
}

// Extracts a string from an array of bytes ("ay")
std::string Utils::stringFromGVariantByteArray(const GVariant *pVariant)
{
    if (!pVariant) {
        return "";
    }
    
    gsize size;
    gconstpointer pPtr = g_variant_get_fixed_array(const_cast<GVariant*>(pVariant), &size, 1);
    if (!pPtr || size == 0) {
        return "";
    }
    
    std::vector<gchar> array(size + 1, 0);
    memcpy(array.data(), pPtr, size);
    return array.data();
}

// Make a Empty GvarientBuilder
GVariant *Utils::createEmptyDictionary() 
{
    // 내부적으로 스마트 포인터 버전 사용
    GVariantPtr ptr = createEmptyDictionaryPtr();
    // 소유권 이전을 위해 참조 카운트 증가 후 raw 포인터 반환
    return g_variant_ref_sink(g_variant_ref(ptr.get()));
}

// Make an Empty GVariantBuilder as a smart pointer
GVariantPtr Utils::createEmptyDictionaryPtr()
{
    return makeGVariantPtr(g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
}

}