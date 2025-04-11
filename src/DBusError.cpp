#include "DBusError.h"
#include "Logger.h"

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
        // GQuark를 문자열로 안전하게 변환
        const gchar* domain = g_quark_to_string(error->domain);
        name = domain ? domain : ERROR_FAILED;
        message = error->message ? error->message : "Unknown error";
    } else {
        // 유효하지 않은 GError 처리
        name = ERROR_FAILED;
        message = "Null error pointer";
    }
}

GErrorPtr DBusError::toGError() const {
    try {
        // 새 GError 생성
        GError* error = g_error_new_literal(
            g_quark_from_string(name.c_str()),
            0,  // 코드는 일반적으로 중요하지 않음
            message.c_str()
        );
        
        // 스마트 포인터로 래핑하여 반환
        return makeGErrorPtr(error);
    } catch (const std::exception& e) {
        Logger::error("Exception in DBusError::toGError: " + std::string(e.what()));
        return makeGErrorPtr(nullptr);
    }
}

std::string DBusError::toString() const {
    return name + ": " + message;
}

bool DBusError::isErrorType(const std::string& errorName) const {
    return name == errorName;
}

} // namespace ggk