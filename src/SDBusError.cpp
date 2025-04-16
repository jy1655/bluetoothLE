#include "SDBusError.h"

namespace ggk {

SDBusError::SDBusError(const std::string& name, const std::string& message)
    : name(name), message(message) {
}

SDBusError::SDBusError(const sdbus::Error& error)
    : name(error.getName()), message(error.getMessage()) {
}

sdbus::Error SDBusError::toSdbusError() const {
    return sdbus::Error(name, message);
}

std::string SDBusError::toString() const {
    return name + ": " + message;
}

bool SDBusError::isErrorType(const std::string& errorName) const {
    return name == errorName;
}

} // namespace ggk