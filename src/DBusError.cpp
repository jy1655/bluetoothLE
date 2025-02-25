// DBusError.cpp
#include "DBusError.h"

namespace ggk {

DBusError::DBusError(DBusErrorCode code, const std::string& message)
    : std::runtime_error(message)
    , errorCode(code)
    , errorName(codeToName(code))
    , errorMessage(message)
{
    Logger::error(SSTR << "DBus error: " << errorName << " - " << message);
}

DBusError DBusError::fromGError(GError* error)
{
    if (!error) {
        return DBusError(DBusErrorCode::NONE, "No error");
    }

    // GQuark를 문자열로 변환
    const gchar* error_domain = g_quark_to_string(error->domain);
    DBusErrorCode code = nameToCode(std::string(error_domain));
    return DBusError(code, error->message);
}

GError* DBusError::toGError() const
{
    GError* error = nullptr;
    g_set_error(&error,
                g_quark_from_string(errorName.c_str()),
                static_cast<gint>(errorCode),
                "%s", errorMessage.c_str());
    return error;
}

std::string DBusError::codeToName(DBusErrorCode code)
{
    switch (code) {
        case DBusErrorCode::FAILED:
            return "org.freedesktop.DBus.Error.Failed";
        case DBusErrorCode::NO_MEMORY:
            return "org.freedesktop.DBus.Error.NoMemory";
        // ... 다른 에러 코드들에 대한 매핑
        case DBusErrorCode::BLUEZ_FAILED:
            return "org.bluez.Error.Failed";
        // ... BlueZ 관련 에러 코드들에 대한 매핑
        default:
            return "org.freedesktop.DBus.Error.Failed";
    }
}

DBusErrorCode DBusError::nameToCode(const std::string& name)
{
    if (name == "org.freedesktop.DBus.Error.Failed")
        return DBusErrorCode::FAILED;
    if (name == "org.freedesktop.DBus.Error.NoMemory")
        return DBusErrorCode::NO_MEMORY;
    // ... 다른 에러 이름들에 대한 매핑
    if (name == "org.bluez.Error.Failed")
        return DBusErrorCode::BLUEZ_FAILED;
    // ... BlueZ 관련 에러 이름들에 대한 매핑
    
    return DBusErrorCode::FAILED;
}

} // namespace ggk