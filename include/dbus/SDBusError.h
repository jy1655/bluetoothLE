#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>

namespace ggk {

/**
 * @brief D-Bus error handling class
 */
class SDBusError {
public:
    // Standard D-Bus error names
    static constexpr const char* ERROR_FAILED = "org.freedesktop.DBus.Error.Failed";
    static constexpr const char* ERROR_NO_REPLY = "org.freedesktop.DBus.Error.NoReply";
    static constexpr const char* ERROR_NOT_SUPPORTED = "org.freedesktop.DBus.Error.NotSupported";
    static constexpr const char* ERROR_INVALID_ARGS = "org.freedesktop.DBus.Error.InvalidArgs";
    static constexpr const char* ERROR_INVALID_SIGNATURE = "org.freedesktop.DBus.Error.InvalidSignature";
    static constexpr const char* ERROR_UNKNOWN_METHOD = "org.freedesktop.DBus.Error.UnknownMethod";
    static constexpr const char* ERROR_UNKNOWN_OBJECT = "org.freedesktop.DBus.Error.UnknownObject";
    static constexpr const char* ERROR_UNKNOWN_INTERFACE = "org.freedesktop.DBus.Error.UnknownInterface";
    static constexpr const char* ERROR_UNKNOWN_PROPERTY = "org.freedesktop.DBus.Error.UnknownProperty";
    static constexpr const char* ERROR_PROPERTY_READ_ONLY = "org.freedesktop.DBus.Error.PropertyReadOnly";
    
    /**
     * @brief Constructor
     * 
     * @param name Error name
     * @param message Error message
     */
    SDBusError(const std::string& name, const std::string& message);
    
    /**
     * @brief Constructor from sdbus::Error
     * 
     * @param error sdbus::Error
     */
    explicit SDBusError(const sdbus::Error& error);
    
    /**
     * @brief Get error name
     * 
     * @return Error name
     */
    const std::string& getName() const { return name; }
    
    /**
     * @brief Get error message
     * 
     * @return Error message
     */
    const std::string& getMessage() const { return message; }
    
    /**
     * @brief Convert to sdbus::Error
     * 
     * @return sdbus::Error
     */
    sdbus::Error toSdbusError() const;
    
    /**
     * @brief Convert to string representation
     * 
     * @return String representation
     */
    std::string toString() const;
    
    /**
     * @brief Check error type
     * 
     * @param errorName Error name to check
     * @return true if error matches the given name
     */
    bool isErrorType(const std::string& errorName) const;

private:
    std::string name;
    std::string message;
};

} // namespace ggk