#pragma once

#include <sstream>
#include <functional>

namespace ggk {

// Our handy stringstream macro
#define SSTR std::ostringstream().flush()

class Logger {
public:
    // Define our own log receiver type using std::function
    using LogReceiver = std::function<void(const char*)>;

    // Registration
    static void registerDebugReceiver(LogReceiver receiver);
    static void registerInfoReceiver(LogReceiver receiver);
    static void registerStatusReceiver(LogReceiver receiver);
    static void registerWarnReceiver(LogReceiver receiver);
    static void registerErrorReceiver(LogReceiver receiver);
    static void registerFatalReceiver(LogReceiver receiver);
    static void registerAlwaysReceiver(LogReceiver receiver);
    static void registerTraceReceiver(LogReceiver receiver);

    // Logging actions
    static void debug(const char* pText);
    static void debug(const std::string& text);
    static void debug(const std::ostream& text);

    static void info(const char* pText);
    static void info(const std::string& text);
    static void info(const std::ostream& text);

    static void status(const char* pText);
    static void status(const std::string& text);
    static void status(const std::ostream& text);

    static void warn(const char* pText);
    static void warn(const std::string& text);
    static void warn(const std::ostream& text);

    static void error(const char* pText);
    static void error(const std::string& text);
    static void error(const std::ostream& text);

    static void fatal(const char* pText);
    static void fatal(const std::string& text);
    static void fatal(const std::ostream& text);

    static void always(const char* pText);
    static void always(const std::string& text);
    static void always(const std::ostream& text);

    static void trace(const char* pText);
    static void trace(const std::string& text);
    static void trace(const std::ostream& text);

private:
    static LogReceiver logReceiverDebug;
    static LogReceiver logReceiverInfo;
    static LogReceiver logReceiverStatus;
    static LogReceiver logReceiverWarn;
    static LogReceiver logReceiverError;
    static LogReceiver logReceiverFatal;
    static LogReceiver logReceiverAlways;
    static LogReceiver logReceiverTrace;
};

} // namespace ggk