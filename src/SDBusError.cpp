#include "SDBusError.h"

namespace ggk {

SDBusError::SDBusError(const std::string& name, const std::string& message)
    : name(name), message(message) {
}

SDBusError::SDBusError(const sdbus::Error& error)
    : name(error.getName()), message(error.getMessage()) {
}

sdbus::Error SDBusError::toSdbusError() const {
    // sdbus-c++ 2.1.0에서는 Error 생성자가 변경됨
    return sdbus::Error(sdbus::Error::Name(name), message);
}

std::string SDBusError::toString() const {
    return name + ": " + message;
}

bool SDBusError::isErrorType(const std::string& errorName) const {
    return name == errorName;
}

} // namespace ggk