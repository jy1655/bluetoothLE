#include <algorithm>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>
#include "Utils.h"
#include "Logger.h"

namespace ggk {

// ---------------------------------------------------------------------------------------------------------------------------------
// String utility functions
// ---------------------------------------------------------------------------------------------------------------------------------

// Trim from start (in place)
void Utils::trimBeginInPlace(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(),
    [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Trim from end (in place)
void Utils::trimEndInPlace(std::string &str) {
    str.erase(std::find_if(str.rbegin(), str.rend(),
    [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

// Trim from both ends (in place)
void Utils::trimInPlace(std::string &str) {
    trimBeginInPlace(str);
    trimEndInPlace(str);
}

// Trim from start (copying)
std::string Utils::trimBegin(const std::string &str) {
    std::string out = str;
    trimBeginInPlace(out);
    return out;
}

// Trim from end (copying)
std::string Utils::trimEnd(const std::string &str) {
    std::string out = str;
    trimEndInPlace(out);
    return out;
}

// Trim from both ends (copying)
std::string Utils::trim(const std::string &str) {
    std::string out = str;
    trimInPlace(out);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Hex output functions
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a zero-padded 8-bit hex value in the format: 0xA
std::string Utils::hex(uint8_t value) {
    char hex[16];
    sprintf(hex, "0x%02X", value);
    return hex;
}

// Returns a zero-padded 8-bit hex value in the format: 0xAB
std::string Utils::hex(uint16_t value) {
    char hex[16];
    sprintf(hex, "0x%04X", value);
    return hex;
}

// Returns a zero-padded 8-bit hex value in the format: 0xABCD
std::string Utils::hex(uint32_t value) {
    char hex[16];
    sprintf(hex, "0x%08X", value);
    return hex;
}

// A full hex-dump of binary data (with accompanying ASCII output)
std::string Utils::hex(const uint8_t *pData, int count) {
    if (!pData || count <= 0) {
        return "[empty data]";
    }

    char hex[16];
    std::string line;
    std::vector<std::string> hexData;
    std::vector<std::string> asciiData;

    // Hex data output
    for (int i = 0; i < count; ++i) {
        sprintf(hex, "%02X ", pData[i]);
        line += hex;

        if (line.length() >= 16 * 3) {
            hexData.push_back(line);
            line = "";
        }
    }

    if (!line.empty()) {
        hexData.push_back(line);
        line = "";
    }

    // ASCII data output
    for (int i = 0; i < count; ++i) {
        // Unprintable?
        if (pData[i] < 0x20 || pData[i] > 0x7e) {
            line += ".";
        } else {
            line += pData[i];
        }

        if (line.length() >= 16) {
            asciiData.push_back(line);
            line = "";
        }
    }

    if (!line.empty()) {
        asciiData.push_back(line);
    }

    std::string result = "";
    size_t dataSize = hexData.size();
    for (size_t i = 0; i < dataSize; ++i) {
        std::string hexPart = hexData[i];
        hexPart.insert(hexPart.length(), 48-hexPart.length(), ' ');

        std::string asciiPart = (i < asciiData.size()) ? asciiData[i] : "";
        asciiPart.insert(asciiPart.length(), 16-asciiPart.length(), ' ');

        result += std::string("    > ") + hexPart + "   [" + asciiPart + "]";

        if (i < dataSize - 1) { result += "\n"; }
    }

    return result;
}

// Returns a properly formatted Bluetooth address from a set of six octets
std::string Utils::bluetoothAddressString(uint8_t *pAddress) {
    if (!pAddress) {
        return "[invalid address]";
    }
    
    char hex[32];
    snprintf(hex, sizeof(hex), "%02X:%02X:%02X:%02X:%02X:%02X", 
        pAddress[0], pAddress[1], pAddress[2], pAddress[3], pAddress[4], pAddress[5]);
    return hex;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Modern GVariant smart pointer helpers
// ---------------------------------------------------------------------------------------------------------------------------------

// Returns a GVariantPtr containing a string
GVariantPtr Utils::gvariantPtrFromString(const char* pStr) {
    if (!pStr) {
        return makeNullGVariantPtr();
    }
    
    GVariant* variant = g_variant_new_string(pStr);
    return makeGVariantPtr(variant);
}

GVariantPtr Utils::gvariantPtrFromString(const std::string &str) {
    return gvariantPtrFromString(str.c_str());
}

// Returns a GVariantPtr containing an array of strings ("as") from a vector of strings
GVariantPtr Utils::gvariantPtrFromStringArray(const std::vector<std::string> &arr) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    for (const auto& str : arr) {
        g_variant_builder_add(&builder, "s", str.c_str());
    }

    GVariant* result = g_variant_builder_end(&builder);
    return makeGVariantPtr(result);
}

// Returns a GVariantPtr containing an array of strings ("as") from a vector of C strings
GVariantPtr Utils::gvariantPtrFromStringArray(const std::vector<const char *> &arr) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

    for (const char* pStr : arr) {
        if (pStr) {  // NULL check
            g_variant_builder_add(&builder, "s", pStr);
        }
    }

    GVariant* result = g_variant_builder_end(&builder);
    return makeGVariantPtr(result);
}

// Returns a GVariantPtr containing an object path ("o") from a DBusObjectPath
GVariantPtr Utils::gvariantPtrFromObject(const DBusObjectPath& path) {
    if (path.toString().empty()) {
        return makeNullGVariantPtr();
    }
    
    GVariant* variant = g_variant_new_object_path(path.c_str());
    if (!variant) {
        return makeNullGVariantPtr();
    }
    
    return makeGVariantPtr(variant);
}

// Returns a GVariantPtr containing a boolean
GVariantPtr Utils::gvariantPtrFromBoolean(bool b) {
    return makeGVariantPtr(g_variant_new_boolean(b));
}

// Returns a GVariantPtr containing integers
GVariantPtr Utils::gvariantPtrFromInt(gint16 value) {
    return makeGVariantPtr(g_variant_new_int16(value));
}

GVariantPtr Utils::gvariantPtrFromInt(gint32 value) {
    return makeGVariantPtr(g_variant_new_int32(value));
}

GVariantPtr Utils::gvariantPtrFromInt(gint64 value) {
    return makeGVariantPtr(g_variant_new_int64(value));
}

GVariantPtr Utils::gvariantPtrFromUInt(guint16 value) {
    return makeGVariantPtr(g_variant_new_uint16(value));
}

GVariantPtr Utils::gvariantPtrFromUInt(guint32 value) {
    return makeGVariantPtr(g_variant_new_uint32(value));
}

GVariantPtr Utils::gvariantPtrFromUInt(guint64 value) {
    return makeGVariantPtr(g_variant_new_uint64(value));
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a C string
GVariantPtr Utils::gvariantPtrFromByteArray(const char* pStr) {
    if (!pStr) {
        // Empty byte array
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(reinterpret_cast<const guint8*>(pStr), strlen(pStr));
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a string
GVariantPtr Utils::gvariantPtrFromByteArray(const std::string& str) {
    if (str.empty()) {
        // Empty byte array
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(reinterpret_cast<const guint8*>(str.c_str()), str.length());
}

// Returns a GVariantPtr containing an array of bytes ("ay") from raw byte data
GVariantPtr Utils::gvariantPtrFromByteArray(const guint8* pBytes, int count) {
    if (!pBytes || count <= 0) {
        // Empty byte array
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }
    
    // More efficient implementation using GBytes
    GBytes* bytes = g_bytes_new(pBytes, count);
    GVariant* variant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
    g_bytes_unref(bytes);
    
    return makeGVariantPtr(variant);
}

// Returns a GVariantPtr containing an array of bytes ("ay") from a vector of bytes
GVariantPtr Utils::gvariantPtrFromByteArray(const std::vector<guint8> bytes) {
    if (bytes.empty()) {
        // Empty byte array
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        GVariant* result = g_variant_builder_end(&builder);
        return makeGVariantPtr(result);
    }

    return gvariantPtrFromByteArray(bytes.data(), bytes.size());
}

// Extracts a string from an array of bytes ("ay")
std::string Utils::stringFromGVariantByteArray(const GVariant *pVariant) {
    if (!pVariant) {
        return "";
    }
    
    gsize size;
    gconstpointer pPtr = g_variant_get_fixed_array(const_cast<GVariant*>(pVariant), &size, 1);
    if (!pPtr || size == 0) {
        return "";
    }
    
    return std::string(static_cast<const char*>(pPtr), size);
}

// Creates an empty dictionary variant
GVariantPtr Utils::createEmptyDictionaryPtr() {
    return makeGVariantPtr(g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
}

// Creates an empty array variant of the specified type
GVariantPtr Utils::createEmptyArrayPtr(const GVariantType* type) {
    return makeGVariantPtr(g_variant_new_array(type, NULL, 0));
}

// Parse variant data to boolean
bool Utils::variantToBoolean(GVariant* variant, bool defaultValue) {
    if (!variant) {
        return defaultValue;
    }
    
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        return g_variant_get_boolean(variant);
    }
    
    return defaultValue;
}

// Parse variant data to string
std::string Utils::variantToString(GVariant* variant, const std::string& defaultValue) {
    if (!variant) {
        return defaultValue;
    }
    
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING)) {
        const gchar* value = g_variant_get_string(variant, NULL);
        return value ? value : defaultValue;
    }
    
    return defaultValue;
}

// Parse variant data to string array
std::vector<std::string> Utils::variantToStringArray(GVariant* variant) {
    std::vector<std::string> result;
    
    if (!variant || !g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING_ARRAY)) {
        return result;
    }
    
    gsize size = g_variant_n_children(variant);
    for (gsize i = 0; i < size; i++) {
        GVariant* child = g_variant_get_child_value(variant, i);
        if (child) {
            const gchar* value = g_variant_get_string(child, NULL);
            if (value) {
                result.push_back(value);
            }
            g_variant_unref(child);
        }
    }
    
    return result;
}

// Parse variant data to byte array
std::vector<uint8_t> Utils::variantToByteArray(GVariant* variant) {
    std::vector<uint8_t> result;
    
    if (!variant) {
        return result;
    }
    
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BYTESTRING)) {
        gsize size;
        const guint8* data = static_cast<const guint8*>(g_variant_get_fixed_array(variant, &size, 1));
        if (data && size > 0) {
            result.assign(data, data + size);
        }
    }
    else if (g_variant_is_of_type(variant, G_VARIANT_TYPE("ay"))) {
        gsize size = g_variant_n_children(variant);
        result.reserve(size);
        
        for (gsize i = 0; i < size; i++) {
            GVariant* child = g_variant_get_child_value(variant, i);
            if (child) {
                guint8 byte = g_variant_get_byte(child);
                result.push_back(byte);
                g_variant_unref(child);
            }
        }
    }
    
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Bluetooth utilities
// ---------------------------------------------------------------------------------------------------------------------------------

// Check if a command is available
bool Utils::isCommandAvailable(const std::string& command) {
    return access((std::string("/usr/bin/") + command).c_str(), X_OK) == 0 ||
           access((std::string("/bin/") + command).c_str(), X_OK) == 0 ||
           access((std::string("/usr/sbin/") + command).c_str(), X_OK) == 0 ||
           access((std::string("/sbin/") + command).c_str(), X_OK) == 0;
}

// Check if BlueZ service is running
bool Utils::isBlueZServiceRunning() {
    int result = system("systemctl is-active --quiet bluetooth.service");
    return result == 0;
}

// Check if a Bluetooth adapter is available
bool Utils::isBluetoothAdapterAvailable(const std::string& adapter) {
    return std::filesystem::exists("/sys/class/bluetooth/" + adapter);
}

// Check if a package is installed
bool Utils::isPackageInstalled(const std::string& packageName) {
    std::string command = "dpkg -s " + packageName + " 2>/dev/null | grep 'Status: install ok installed' >/dev/null";
    int result = system(command.c_str());
    return result == 0;
}

// Write content to a file
bool Utils::writeToFile(const std::string& filename, const std::string& content) {
    try {
        std::ofstream file(filename);
        if (!file) {
            return false;
        }
        
        file << content;
        return file.good();
    }
    catch (const std::exception& e) {
        Logger::error("Error writing to file: " + std::string(e.what()));
        return false;
    }
}

// Read content from a file
std::string Utils::readFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    catch (const std::exception& e) {
        Logger::error("Error reading from file: " + std::string(e.what()));
        return "";
    }
}

// Execute a shell script from string content
bool Utils::executeScript(const std::string& scriptContent) {
    try {
        // Create a temporary file for the script
        std::string tempFileName = "/tmp/ble_script_" + std::to_string(getpid()) + ".sh";
        
        // Write the script content to the file
        if (!writeToFile(tempFileName, scriptContent)) {
            return false;
        }
        
        // Make the script executable
        chmod(tempFileName.c_str(), 0755);
        
        // Execute the script
        int result = system(tempFileName.c_str());
        
        // Clean up
        std::filesystem::remove(tempFileName);
        
        return result == 0;
    }
    catch (const std::exception& e) {
        Logger::error("Error executing script: " + std::string(e.what()));
        return false;
    }
}

} // namespace ggk