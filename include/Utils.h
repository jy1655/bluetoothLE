#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <vector>
#include <string>
#include <memory>
#include <endian.h>

#include "DBusObjectPath.h"
#include "DBusTypes.h"

namespace ggk {

struct Utils {
    // -----------------------------------------------------------------------------------------------------------------------------
    // String utility functions
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

    // Returns a properly formatted Bluetooth address from a set of six octets
    static std::string bluetoothAddressString(uint8_t *pAddress);

    // -----------------------------------------------------------------------------------------------------------------------------
    // Modern GVariant helpers with smart pointers - these should be preferred
    // -----------------------------------------------------------------------------------------------------------------------------

    // Returns a GVariantPtr containing a string
    static GVariantPtr gvariantPtrFromString(const char *pStr);
    static GVariantPtr gvariantPtrFromString(const std::string &str);

    // Returns a GVariantPtr containing an array of strings ("as") from a vector of strings
    static GVariantPtr gvariantPtrFromStringArray(const std::vector<std::string> &arr);
    static GVariantPtr gvariantPtrFromStringArray(const std::vector<const char *> &arr);

    // Returns a GVariantPtr containing an object path ("o") from a DBusObjectPath
    static GVariantPtr gvariantPtrFromObject(const DBusObjectPath &path);

    // Returns a GVariantPtr containing a boolean
    static GVariantPtr gvariantPtrFromBoolean(bool b);

    // Returns a GVariantPtr containing integers
    static GVariantPtr gvariantPtrFromInt(gint16 value);
    static GVariantPtr gvariantPtrFromInt(gint32 value);
    static GVariantPtr gvariantPtrFromInt(gint64 value);
    static GVariantPtr gvariantPtrFromUInt(guint16 value);
    static GVariantPtr gvariantPtrFromUInt(guint32 value);
    static GVariantPtr gvariantPtrFromUInt(guint64 value);

    // Returns a GVariantPtr containing an array of bytes ("ay") from various sources
    static GVariantPtr gvariantPtrFromByteArray(const char *pStr);
    static GVariantPtr gvariantPtrFromByteArray(const std::string &str);
    static GVariantPtr gvariantPtrFromByteArray(const guint8 *pBytes, int count);
    static GVariantPtr gvariantPtrFromByteArray(const std::vector<guint8> bytes);

    // Extracts a string from an array of bytes ("ay")
    static std::string stringFromGVariantByteArray(const GVariant *pVariant);

    // Creates empty dictionary variants
    static GVariantPtr createEmptyDictionaryPtr();
    static GVariantPtr createEmptyArrayPtr(const GVariantType* type);

    // Parse variant data
    static bool variantToBoolean(GVariant* variant, bool defaultValue = false);
    static std::string variantToString(GVariant* variant, const std::string& defaultValue = "");
    static std::vector<std::string> variantToStringArray(GVariant* variant);
    static std::vector<uint8_t> variantToByteArray(GVariant* variant);

    // -----------------------------------------------------------------------------------------------------------------------------
    // Bluetooth utilities
    // -----------------------------------------------------------------------------------------------------------------------------

    // Check if a BlueZ tool or command is available
    static bool isCommandAvailable(const std::string& command);

    // Check if BlueZ service is running
    static bool isBlueZServiceRunning();

    // Check if a Bluetooth adapter is available
    static bool isBluetoothAdapterAvailable(const std::string& adapter = "hci0");

    // Check if a package is installed
    static bool isPackageInstalled(const std::string& packageName);

    // Simple file operations
    static bool writeToFile(const std::string& filename, const std::string& content);
    static std::string readFromFile(const std::string& filename);
    static bool executeScript(const std::string& scriptContent);
};

}; // namespace ggk