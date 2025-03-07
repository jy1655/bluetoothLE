#include "DBusError.h"

namespace ggk {

// 표준 D-Bus 오류 상수 정의
const char* DBusError::ERROR_FAILED = "org.freedesktop.DBus.Error.Failed";
const char* DBusError::ERROR_NO_REPLY = "org.freedesktop.DBus.Error.NoReply";
const char* DBusError::ERROR_NOT_SUPPORTED = "org.freedesktop.DBus.Error.NotSupported";
const char* DBusError::ERROR_INVALID_ARGS = "org.freedesktop.DBus.Error.InvalidArgs";
const char* DBusError::ERROR_INVALID_SIGNATURE = "org.freedesktop.DBus.Error.InvalidSignature";
const char* DBusError::ERROR_UNKNOWN_METHOD = "org.freedesktop.DBus.Error.UnknownMethod";
const char* DBusError::ERROR_UNKNOWN_OBJECT = "org.freedesktop.DBus.Error.UnknownObject";
const char* DBusError::ERROR_UNKNOWN_INTERFACE = "org.freedesktop.DBus.Error.UnknownInterface";
const char* DBusError::ERROR_UNKNOWN_PROPERTY = "org.freedesktop.DBus.Error.UnknownProperty";
const char* DBusError::ERROR_PROPERTY_READ_ONLY = "org.freedesktop.DBus.Error.PropertyReadOnly";

DBusError::DBusError(const std::string& name, const std::string& message) 
    : name(name), message(message) {
}

DBusError::DBusError(const GError* error) {
    if (error) {
        name = g_quark_to_string(error->domain);  // GQuark를 문자열로 변환
        message = error->message ? error->message : "";
    } else {
        name = ERROR_FAILED;
        message = "Unknown error";
    }
}

GErrorPtr DBusError::toGError() const {
    GError* error = g_error_new_literal(
        g_quark_from_string(name.c_str()),
        0,  // 코드는 일반적으로 중요하지 않음
        message.c_str()
    );
    return GErrorPtr(error, &g_error_free);
}

std::string DBusError::toString() const {
    return name + ": " + message;
}

} // namespace ggk