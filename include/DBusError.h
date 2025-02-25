// DBusError.h
#pragma once

#include <string>
#include <stdexcept>
#include <gio/gio.h>
#include "Logger.h"

namespace ggk {

// DBus 에러 코드 정의
enum class DBusErrorCode {
    NONE = 0,
    FAILED,                    // org.freedesktop.DBus.Error.Failed
    NO_MEMORY,                 // org.freedesktop.DBus.Error.NoMemory
    SERVICE_UNKNOWN,           // org.freedesktop.DBus.Error.ServiceUnknown
    NAME_HAS_NO_OWNER,        // org.freedesktop.DBus.Error.NameHasNoOwner
    NO_REPLY,                 // org.freedesktop.DBus.Error.NoReply
    IO_ERROR,                 // org.freedesktop.DBus.Error.IOError
    NOT_SUPPORTED,            // org.freedesktop.DBus.Error.NotSupported
    INVALID_ARGS,             // org.freedesktop.DBus.Error.InvalidArgs
    INVALID_SIGNATURE,        // org.freedesktop.DBus.Error.InvalidSignature
    FILE_NOT_FOUND,          // org.freedesktop.DBus.Error.FileNotFound
    PROPERTY_READ_ONLY,      // org.freedesktop.DBus.Error.PropertyReadOnly
    PROPERTY_WRITE_ONLY,     // org.freedesktop.DBus.Error.PropertyWriteOnly
    // BlueZ 관련 에러 코드
    BLUEZ_FAILED,            // org.bluez.Error.Failed
    BLUEZ_REJECTED,          // org.bluez.Error.Rejected
    BLUEZ_CANCELED,          // org.bluez.Error.Canceled
    BLUEZ_INVALID_ARGS,      // org.bluez.Error.InvalidArgs
    BLUEZ_NOT_READY,         // org.bluez.Error.NotReady
    BLUEZ_NOT_AVAILABLE,     // org.bluez.Error.NotAvailable
    BLUEZ_NOT_SUPPORTED,     // org.bluez.Error.NotSupported
    BLUEZ_NOT_AUTHORIZED,    // org.bluez.Error.NotAuthorized
};

class DBusError : public std::runtime_error {
public:
    // 에러 생성자
    DBusError(DBusErrorCode code, const std::string& message);
    // GError로부터 DBusError 생성
    static DBusError fromGError(GError* error);

    // 에러 정보 접근자
    DBusErrorCode code() const { return errorCode; }
    const std::string& name() const { return errorName; }
    const std::string& message() const { return errorMessage; }
    
    // GError 변환
    GError* toGError() const;

private:
    DBusErrorCode errorCode;
    std::string errorName;
    std::string errorMessage;

    // 에러 코드를 DBus 에러 이름으로 변환
    static std::string codeToName(DBusErrorCode code);
    // DBus 에러 이름을 에러 코드로 변환
    static DBusErrorCode nameToCode(const std::string& name);
};

} // namespace ggk