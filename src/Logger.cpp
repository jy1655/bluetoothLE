#include "Logger.h"

namespace ggk {

// Initialize static members
Logger::LogReceiver Logger::logReceiverDebug;
Logger::LogReceiver Logger::logReceiverInfo;
Logger::LogReceiver Logger::logReceiverStatus;
Logger::LogReceiver Logger::logReceiverWarn;
Logger::LogReceiver Logger::logReceiverError;
Logger::LogReceiver Logger::logReceiverFatal;
Logger::LogReceiver Logger::logReceiverAlways;
Logger::LogReceiver Logger::logReceiverTrace;
Logger::Level Logger::currentLogLevel = Logger::Level::INFO;

//
// Registration
//

void Logger::registerDebugReceiver(LogReceiver receiver) { logReceiverDebug = receiver; }
void Logger::registerInfoReceiver(LogReceiver receiver) { logReceiverInfo = receiver; }
void Logger::registerStatusReceiver(LogReceiver receiver) { logReceiverStatus = receiver; }
void Logger::registerWarnReceiver(LogReceiver receiver) { logReceiverWarn = receiver; }
void Logger::registerErrorReceiver(LogReceiver receiver) { logReceiverError = receiver; }
void Logger::registerFatalReceiver(LogReceiver receiver) { logReceiverFatal = receiver; }
void Logger::registerAlwaysReceiver(LogReceiver receiver) { logReceiverAlways = receiver; }
void Logger::registerTraceReceiver(LogReceiver receiver) { logReceiverTrace = receiver; }

void Logger::setLogLevel(Level level) {
    currentLogLevel = level;
}

Logger::Level Logger::getLogLevel() {
    return currentLogLevel;
}

bool Logger::shouldLog(Level messageLevel) {
    return static_cast<int>(messageLevel) >= static_cast<int>(currentLogLevel);
}

const char* Logger::levelToString(Level level) {
    switch (level) {
        case Level::TRACE: return "TRACE";
        case Level::DEBUG: return "DEBUG";
        case Level::INFO: return "INFO";
        case Level::STATUS: return "STATUS";
        case Level::WARN: return "WARN";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
        case Level::ALWAYS: return "ALWAYS";
        default: return "UNKNOWN";
    }
}

//
// Logging actions
//

// Universal log method
void Logger::log(Level level, const std::string& message) {
    if (!shouldLog(level)) {
        return;
    }
    
    switch (level) {
        case Level::DEBUG:
            if (logReceiverDebug) logReceiverDebug(message.c_str());
            break;
        case Level::INFO:
            if (logReceiverInfo) logReceiverInfo(message.c_str());
            break;
        case Level::STATUS:
            if (logReceiverStatus) logReceiverStatus(message.c_str());
            break;
        case Level::WARN:
            if (logReceiverWarn) logReceiverWarn(message.c_str());
            break;
        case Level::ERROR:
            if (logReceiverError) logReceiverError(message.c_str());
            break;
        case Level::FATAL:
            if (logReceiverFatal) logReceiverFatal(message.c_str());
            break;
        case Level::ALWAYS:
            if (logReceiverAlways) logReceiverAlways(message.c_str());
            break;
        case Level::TRACE:
            if (logReceiverTrace) logReceiverTrace(message.c_str());
            break;
    }
}

// Log a DEBUG entry with a C string
void Logger::debug(const char *pText) { 
    if (shouldLog(Level::DEBUG) && logReceiverDebug) { 
        logReceiverDebug(pText); 
    } 
}

// Log a DEBUG entry with a string
void Logger::debug(const std::string &text) { 
    if (shouldLog(Level::DEBUG) && logReceiverDebug) { 
        debug(text.c_str()); 
    } 
}

// Log a DEBUG entry using a stream
void Logger::debug(const std::ostream &text) { 
    if (shouldLog(Level::DEBUG) && logReceiverDebug) { 
        debug(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a INFO entry with a C string
void Logger::info(const char *pText) { 
    if (shouldLog(Level::INFO) && logReceiverInfo) { 
        logReceiverInfo(pText); 
    } 
}

// Log a INFO entry with a string
void Logger::info(const std::string &text) { 
    if (shouldLog(Level::INFO) && logReceiverInfo) { 
        info(text.c_str()); 
    } 
}

// Log a INFO entry using a stream
void Logger::info(const std::ostream &text) { 
    if (shouldLog(Level::INFO) && logReceiverInfo) { 
        info(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a STATUS entry with a C string
void Logger::status(const char *pText) { 
    if (shouldLog(Level::STATUS) && logReceiverStatus) { 
        Logger::logReceiverStatus(pText); 
    } 
}

// Log a STATUS entry with a string
void Logger::status(const std::string &text) { 
    if (shouldLog(Level::STATUS) && logReceiverStatus) { 
        status(text.c_str()); 
    } 
}

// Log a STATUS entry using a stream
void Logger::status(const std::ostream &text) { 
    if (shouldLog(Level::STATUS) && logReceiverStatus) { 
        status(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a WARN entry with a C string
void Logger::warn(const char *pText) { 
    if (shouldLog(Level::WARN) && logReceiverWarn) { 
        logReceiverWarn(pText); 
    } 
}

// Log a WARN entry with a string
void Logger::warn(const std::string &text) { 
    if (shouldLog(Level::WARN) && logReceiverWarn) { 
        warn(text.c_str()); 
    } 
}

// Log a WARN entry using a stream
void Logger::warn(const std::ostream &text) { 
    if (shouldLog(Level::WARN) && logReceiverWarn) { 
        warn(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a ERROR entry with a C string
void Logger::error(const char *pText) { 
    if (shouldLog(Level::ERROR) && logReceiverError) { 
        logReceiverError(pText); 
    } 
}

// Log a ERROR entry with a string
void Logger::error(const std::string &text) { 
    if (shouldLog(Level::ERROR) && logReceiverError) { 
        error(text.c_str()); 
    } 
}

// Log a ERROR entry using a stream
void Logger::error(const std::ostream &text) { 
    if (shouldLog(Level::ERROR) && logReceiverError) { 
        error(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a FATAL entry with a C string
void Logger::fatal(const char *pText) { 
    if (shouldLog(Level::FATAL) && logReceiverFatal) { 
        logReceiverFatal(pText); 
    } 
}

// Log a FATAL entry with a string
void Logger::fatal(const std::string &text) { 
    if (shouldLog(Level::FATAL) && logReceiverFatal) { 
        fatal(text.c_str()); 
    } 
}

// Log a FATAL entry using a stream
void Logger::fatal(const std::ostream &text) { 
    if (shouldLog(Level::FATAL) && logReceiverFatal) { 
        fatal(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a ALWAYS entry with a C string
void Logger::always(const char *pText) { 
    if (logReceiverAlways) { 
        logReceiverAlways(pText); 
    } 
}

// Log a ALWAYS entry with a string
void Logger::always(const std::string &text) { 
    if (logReceiverAlways) { 
        always(text.c_str()); 
    } 
}

// Log a ALWAYS entry using a stream
void Logger::always(const std::ostream &text) { 
    if (logReceiverAlways) { 
        always(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

// Log a TRACE entry with a C string
void Logger::trace(const char *pText) { 
    if (shouldLog(Level::TRACE) && logReceiverTrace) { 
        logReceiverTrace(pText); 
    } 
}

// Log a TRACE entry with a string
void Logger::trace(const std::string &text) { 
    if (shouldLog(Level::TRACE) && logReceiverTrace) { 
        trace(text.c_str()); 
    } 
}

// Log a TRACE entry using a stream
void Logger::trace(const std::ostream &text) { 
    if (shouldLog(Level::TRACE) && logReceiverTrace) { 
        trace(static_cast<const std::ostringstream &>(text).str().c_str()); 
    } 
}

}; // namespace ggk