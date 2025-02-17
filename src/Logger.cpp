#include "Logger.h"

namespace ggk {

//
// Log receiver delegates
//

// Initialize static members
Logger::LogReceiver Logger::logReceiverDebug;
Logger::LogReceiver Logger::logReceiverInfo;
Logger::LogReceiver Logger::logReceiverStatus;
Logger::LogReceiver Logger::logReceiverWarn;
Logger::LogReceiver Logger::logReceiverError;
Logger::LogReceiver Logger::logReceiverFatal;
Logger::LogReceiver Logger::logReceiverAlways;
Logger::LogReceiver Logger::logReceiverTrace;

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

//
// Logging actions
//

// Log a DEBUG entry with a C string
void Logger::debug(const char *pText) { if (logReceiverDebug) { logReceiverDebug(pText); } }

// Log a DEBUG entry with a string
void Logger::debug(const std::string &text) { if (logReceiverDebug) { debug(text.c_str()); } }

// Log a DEBUG entry using a stream
void Logger::debug(const std::ostream &text) { if (logReceiverDebug) { debug(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a INFO entry with a C string
void Logger::info(const char *pText) { if (logReceiverInfo) { logReceiverInfo(pText); } }

// Log a INFO entry with a string
void Logger::info(const std::string &text) { if (logReceiverInfo) { info(text.c_str()); } }

// Log a INFO entry using a stream
void Logger::info(const std::ostream &text) { if (logReceiverInfo) { info(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a STATUS entry with a C string
void Logger::status(const char *pText) { if (logReceiverStatus) { Logger::logReceiverStatus(pText); } }

// Log a STATUS entry with a string
void Logger::status(const std::string &text) { if (logReceiverStatus) { status(text.c_str()); } }

// Log a STATUS entry using a stream
void Logger::status(const std::ostream &text) { if (logReceiverStatus) { status(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a WARN entry with a C string
void Logger::warn(const char *pText) { if (logReceiverWarn) { logReceiverWarn(pText); } }

// Log a WARN entry with a string
void Logger::warn(const std::string &text) { if (logReceiverWarn) { warn(text.c_str()); } }

// Log a WARN entry using a stream
void Logger::warn(const std::ostream &text) { if (logReceiverWarn) { warn(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a ERROR entry with a C string
void Logger::error(const char *pText) { if (logReceiverError) { logReceiverError(pText); } }

// Log a ERROR entry with a string
void Logger::error(const std::string &text) { if (logReceiverError) { error(text.c_str()); } }

// Log a ERROR entry using a stream
void Logger::error(const std::ostream &text) { if (logReceiverError) { error(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a FATAL entry with a C string
void Logger::fatal(const char *pText) { if (logReceiverFatal) { logReceiverFatal(pText); } }

// Log a FATAL entry with a string
void Logger::fatal(const std::string &text) { if (logReceiverFatal) { fatal(text.c_str()); } }

// Log a FATAL entry using a stream
void Logger::fatal(const std::ostream &text) { if (logReceiverFatal) { fatal(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a ALWAYS entry with a C string
void Logger::always(const char *pText) { if (logReceiverAlways) { logReceiverAlways(pText); } }

// Log a ALWAYS entry with a string
void Logger::always(const std::string &text) { if (logReceiverAlways) { always(text.c_str()); } }

// Log a ALWAYS entry using a stream
void Logger::always(const std::ostream &text) { if (logReceiverAlways) { always(static_cast<const std::ostringstream &>(text).str().c_str()); } }

// Log a TRACE entry with a C string
void Logger::trace(const char *pText) { if (logReceiverTrace) { logReceiverTrace(pText); } }

// Log a TRACE entry with a string
void Logger::trace(const std::string &text) { if (logReceiverTrace) { trace(text.c_str()); } }

// Log a TRACE entry using a stream
void Logger::trace(const std::ostream &text) { if (logReceiverTrace) { trace(static_cast<const std::ostringstream &>(text).str().c_str()); } }

}; // namespace ggk