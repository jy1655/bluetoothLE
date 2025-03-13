#pragma once

#include <string>
#include <gio/gio.h>
#include "DBusTypes.h"

namespace ggk {

class DBusError {
public:
    // 표준 D-Bus 오류
    static const char* ERROR_FAILED;
    static const char* ERROR_NO_REPLY;
    static const char* ERROR_NOT_SUPPORTED;
    static const char* ERROR_INVALID_ARGS;
    static const char* ERROR_INVALID_SIGNATURE;
    static const char* ERROR_UNKNOWN_METHOD;
    static const char* ERROR_UNKNOWN_OBJECT;
    static const char* ERROR_UNKNOWN_INTERFACE;
    static const char* ERROR_UNKNOWN_PROPERTY;
    static const char* ERROR_PROPERTY_READ_ONLY;
    
    // 생성자
    DBusError(const std::string& name, const std::string& message);
    
    // GError에서 생성
    explicit DBusError(const GError* error);
    
    // 접근자
    const std::string& getName() const { return name; }
    const std::string& getMessage() const { return message; }
    
    // GError로 변환
    GErrorPtr toGError() const;
    
    // 예외 메시지로 변환
    std::string toString() const;

private:
    std::string name;
    std::string message;
};

} // namespace ggk