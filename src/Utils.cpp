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
// GVariant helper functions
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a GVariant containing a floating reference to a utf8 string
GVariant *Utils::gvariantFromString(const char *pStr)
{
	return g_variant_new_string(pStr);
}

// Returns a GVariant containing a floating reference to a utf8 string
GVariant *Utils::gvariantFromString(const std::string &str)
{
	return g_variant_new_string(str.c_str());
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
GVariant *Utils::gvariantFromStringArray(const std::vector<std::string> &arr)
{
	// 빌더 초기화 - 항상 유효한 빌더 사용
	g_auto(GVariantBuilder) builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

	// 배열이 비어있어도 문제없이 빈 배열 생성
    for (const std::string& str : arr)
    {
        g_variant_builder_add(&builder, "s", str.c_str());
    }

	return g_variant_builder_end(&builder);
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
	return g_variant_new_object_path(path.c_str());
}

// Returns an GVariant* containing a boolean
GVariant *Utils::gvariantFromBoolean(bool b)
{
	return g_variant_new_boolean(b);
}

// Returns an GVariant* containing a 16-bit integer
GVariant *Utils::gvariantFromInt(gint16 value)
{
	return g_variant_new_int16(value);
}

// Returns an GVariant* containing a 32-bit integer
GVariant *Utils::gvariantFromInt(gint32 value)
{
	return g_variant_new_int32(value);
}

// Returns an array of bytes ("ay") with the contents of the input C string
GVariant *Utils::gvariantFromByteArray(const char *pStr)
{
    if (pStr == nullptr || *pStr == 0)
    {
        // 빈 바이트 배열 생성
        g_auto(GVariantBuilder) builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        return g_variant_builder_end(&builder);
    }

    return gvariantFromByteArray(reinterpret_cast<const guint8 *>(pStr), strlen(pStr));
}

// Returns an array of bytes ("ay") with the contents of the input string
GVariant *Utils::gvariantFromByteArray(const std::string &str)
{
    if (str.empty())
    {
        // 빈 바이트 배열 생성
        g_auto(GVariantBuilder) builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        return g_variant_builder_end(&builder);
    }

    return gvariantFromByteArray(reinterpret_cast<const guint8 *>(str.c_str()), str.length());
}

// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
GVariant *Utils::gvariantFromByteArray(const guint8 *pBytes, int count)
{
	GBytes *pGbytes = g_bytes_new(pBytes, count);
	GVariant *pGVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pGbytes, count);
	g_bytes_unref(pGbytes);
	return pGVariant;
}

// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
GVariant *Utils::gvariantFromByteArray(const std::vector<guint8> bytes)
{
    if (bytes.empty())
    {
        // 빈 바이트 배열 생성
        g_auto(GVariantBuilder) builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        return g_variant_builder_end(&builder);
    }

    GBytes *pGbytes = g_bytes_new(bytes.data(), bytes.size());
    GVariant *pGVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pGbytes, bytes.size());
    g_bytes_unref(pGbytes);
    return pGVariant;
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

// Extracts a string from an array of bytes ("ay")
std::string Utils::stringFromGVariantByteArray(const GVariant *pVariant)
{
	gsize size;
	gconstpointer pPtr = g_variant_get_fixed_array(const_cast<GVariant *>(pVariant), &size, 1);
	std::vector<gchar> array(size + 1, 0);
	memcpy(array.data(), pPtr, size);
	return array.data();
}

GVariant *Utils::createEmptyDictionary() 
{
    return g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0);
}

}; // namespace ggk