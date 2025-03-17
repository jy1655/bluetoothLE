#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <vector>
#include <string>
#include <endian.h>

#include "DBusObjectPath.h"
#include "DBusTypes.h"

namespace ggk {

struct Utils
{
	// -----------------------------------------------------------------------------------------------------------------------------
	// Handy string functions
	// -----------------------------------------------------------------------------------------------------------------------------

	// Trim from start (in place)
	static void trimBeginInPlace(std::string &str);

	// Trim from end (in place)
	static void trimEndInPlace(std::string &str);

	// Trim from both ends (in place)
	static void trimInPlace(std::string &str);

	// Trim from start (copying)
	static std::string trimBegin(const std::string &str);

	// Trim from end (copying)
	static std::string trimEnd(const std::string &str);

	// Trim from both ends (copying)
	static std::string trim(const std::string &str);

	// -----------------------------------------------------------------------------------------------------------------------------
	// Hex output functions
	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns a zero-padded 8-bit hex value in the format: 0xA
	static std::string hex(uint8_t value);

	// Returns a zero-padded 8-bit hex value in the format: 0xAB
	static std::string hex(uint16_t value);

	// Returns a zero-padded 8-bit hex value in the format: 0xABCD
	static std::string hex(uint32_t value);

	// A full hex-dump of binary data (with accompanying ASCII output)
	static std::string hex(const uint8_t *pData, int count);

	// Returns a peoperly formatted Bluetooth address from a set of six octets stored at `pAddress`
	//
	// USE WITH CAUTION: It is expected that pAddress point to an array of 6 bytes. The length of the array cannot be validated and
	// incorrect lengths will produce undefined, likely unwanted and potentially fatal results. Or it will return the address of the
	// train at platform 9 3/4. You decide.
	//
	// This method returns a set of six zero-padded 8-bit hex values 8-bit in the format: 12:34:56:78:9A:BC
	static std::string bluetoothAddressString(uint8_t *pAddress);

	// -----------------------------------------------------------------------------------------------------------------------------
	// A small collection of helper functions for generating various types of GVariants, which are needed when responding to BlueZ
	// method/property messages. Real services will likley need more of these to support various types of data passed to/from BlueZ,
	// or feel free to do away with them and use GLib directly.
	// -----------------------------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------------------------
	// 이전 버전과 호환되는 GVariant 생성 함수들 (raw 포인터 반환)
	// 참고: 이 함수들은 새로운 스마트 포인터 기반 함수를 내부적으로 사용하지만,
	// 기존 코드와의 호환성을 위해 raw 포인터를 반환합니다. 
	// 소유권 관리를 위해 새로운 함수를 사용하는 것이 권장됩니다.
	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns a GVariant containing a floating reference to a utf8 string
	static GVariant *gvariantFromString(const char *pStr);

	// Returns a GVariant containing a floating reference to a utf8 string
	static GVariant *gvariantFromString(const std::string &str);

	// Returns an array of strings ("as") with one string per variable argument.
	//
	// The array must be terminated with a nullptr.
	//
	// This is an extension method to the vararg version, which accepts pass-through variable arguments from other mthods.
	static GVariant *gvariantFromStringArray(const char *pStr, va_list args);

	// Returns an array of strings ("as") with one string per variable argument.
	//
	// The array must be terminated with a nullptr.
	static GVariant *gvariantFromStringArray(const char *pStr, ...);

	// Returns an array of strings ("as") from an array of strings
	static GVariant *gvariantFromStringArray(const std::vector<std::string> &arr);

	// Returns an array of strings ("as") from an array of C strings
	static GVariant *gvariantFromStringArray(const std::vector<const char *> &arr);

	// Returns an GVariant* containing an object path ("o") from an DBusObjectPath
	static GVariant *gvariantFromObject(const DBusObjectPath &path);

	// Returns an GVariant* containing a boolean
	static GVariant *gvariantFromBoolean(bool b);

	// Returns an GVariant* containing a 16-bit integer
	static GVariant *gvariantFromInt(gint16 value);

	// Returns an GVariant* containing a 32-bit integer
	static GVariant *gvariantFromInt(gint32 value);

	// Returns an array of bytes ("ay") with the contents of the input C string
	static GVariant *gvariantFromByteArray(const char *pStr);

	// Returns an array of bytes ("ay") with the contents of the input string
	static GVariant *gvariantFromByteArray(const std::string &str);

	// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
	static GVariant *gvariantFromByteArray(const guint8 *pBytes, int count);

	// Returns an array of bytes ("ay") with the contents of the input array of unsigned 8-bit values
	static GVariant *gvariantFromByteArray(const std::vector<guint8> bytes);

	// Returns an array of bytes ("ay") containing a single unsigned 8-bit value
	static GVariant *gvariantFromByteArray(const guint8 data);

	// Returns an array of bytes ("ay") containing a single signed 8-bit value
	static GVariant *gvariantFromByteArray(const gint8 data);

	// Returns an array of bytes ("ay") containing a single unsigned 16-bit value
	static GVariant *gvariantFromByteArray(const guint16 data);

	// Returns an array of bytes ("ay") containing a single signed 16-bit value
	static GVariant *gvariantFromByteArray(const gint16 data);

	// Returns an array of bytes ("ay") containing a single unsigned 32-bit value
	static GVariant *gvariantFromByteArray(const guint32 data);

	// Returns an array of bytes ("ay") containing a single signed 32-bit value
	static GVariant *gvariantFromByteArray(const gint32 data);

	// Returns an array of bytes ("ay") containing a single unsigned 64-bit value
	static GVariant *gvariantFromByteArray(const guint64 data);

	// Returns an array of bytes ("ay") containing a single signed 64-bit value
	static GVariant *gvariantFromByteArray(const gint64 data);

	// -----------------------------------------------------------------------------------------------------------------------------
	// 새로운 스마트 포인터 기반 GVariant 생성 함수들
	// 소유권 관리가 명확하고 안전하게 스마트 포인터를 반환합니다.
	// 새로운 코드에서는 이 함수들을 사용하는 것이 권장됩니다.
	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns a GVariantPtr containing a string
	static GVariantPtr gvariantPtrFromString(const char *pStr);

	// Returns a GVariantPtr containing a string
	static GVariantPtr gvariantPtrFromString(const std::string &str);

	// Returns a GVariantPtr containing an array of strings ("as") from a vector of strings
	static GVariantPtr gvariantPtrFromStringArray(const std::vector<std::string> &arr);

	// Returns a GVariantPtr containing an array of strings ("as") from a vector of C strings
	static GVariantPtr gvariantPtrFromStringArray(const std::vector<const char *> &arr);

	// Returns a GVariantPtr containing an object path ("o") from a DBusObjectPath
	static GVariantPtr gvariantPtrFromObject(const DBusObjectPath &path);

	// Returns a GVariantPtr containing a boolean
	static GVariantPtr gvariantPtrFromBoolean(bool b);

	// Returns a GVariantPtr containing a 16-bit integer
	static GVariantPtr gvariantPtrFromInt(gint16 value);

	// Returns a GVariantPtr containing a 32-bit integer
	static GVariantPtr gvariantPtrFromInt(gint32 value);

	// Returns a GVariantPtr containing an array of bytes ("ay") from a C string
	static GVariantPtr gvariantPtrFromByteArray(const char *pStr);

	// Returns a GVariantPtr containing an array of bytes ("ay") from a string
	static GVariantPtr gvariantPtrFromByteArray(const std::string &str);

	// Returns a GVariantPtr containing an array of bytes ("ay") from raw byte data
	static GVariantPtr gvariantPtrFromByteArray(const guint8 *pBytes, int count);

	// Returns a GVariantPtr containing an array of bytes ("ay") from a vector of bytes
	static GVariantPtr gvariantPtrFromByteArray(const std::vector<guint8> bytes);

	// Returns a GVariantPtr containing an array of bytes ("ay") with a single numeric value
	template<typename T>
	static GVariantPtr gvariantPtrFromByteArray(T value) {
		return gvariantPtrFromByteArray(reinterpret_cast<const guint8 *>(&value), sizeof(value));
	}

	// Extracts a string from an array of bytes ("ay")
	static std::string stringFromGVariantByteArray(const GVariant *pVariant);

	// Make a Emtpy GvarientBuilder
	static GVariant *createEmptyDictionary();

	// Make an Emtpy GVariantBuilder as a smart pointer
	static GVariantPtr createEmptyDictionaryPtr();

	// -----------------------------------------------------------------------------------------------------------------------------
	// Endian conversion
	//
	// The Bluetooth Management API defines itself has using little-endian byte order. In the methods below, 'Hci' refers to this
	// format, while 'Host' refers to the endianness of the hardware we are running on.
	//
	// The `Utils::endianToHost()` method overloads perform endian byte-ordering conversions from the HCI to our endian format
	// The `Utils::endianToHci()` method overloads perform endian byte-ordering conversions from our endian format to that of the HCI
	// -----------------------------------------------------------------------------------------------------------------------------

	// Convert a byte from HCI format to host format
	//
	// Since bytes are endian agnostic, this function simply returns the input value
	static uint8_t endianToHost(uint8_t value) {return value;}

	// Convert a byte from host format to HCI format
	//
	// Since bytes are endian agnostic, this function simply returns the input value
	static uint8_t endianToHci(uint8_t value) {return value;}

	// Convert a 16-bit value from HCI format to host format
	static uint16_t endianToHost(uint16_t value) {return le16toh(value);}

	// Convert a 16-bit value from host format to HCI format
	static uint16_t endianToHci(uint16_t value) {return htole16(value);}

	// Convert a 32-bit value from HCI format to host format
	static uint32_t endianToHost(uint32_t value) {return le32toh(value);}

	// Convert a 32-bit value from host format to HCI format
	static uint32_t endianToHci(uint32_t value) {return htole32(value);}
};

}; // namespace ggk