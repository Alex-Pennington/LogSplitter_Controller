#pragma once

#include <Arduino.h>

// Syslog severity levels (RFC 3164)
enum LogLevel {
    LOG_EMERGENCY = 0,  // System unusable
    LOG_ALERT = 1,      // Immediate action required  
    LOG_CRITICAL = 2,   // Critical conditions
    LOG_ERROR = 3,      // Error conditions
    LOG_WARNING = 4,    // Warning conditions
    LOG_NOTICE = 5,     // Normal but significant
    LOG_INFO = 6,       // Informational messages
    LOG_DEBUG = 7       // Debug-level messages
};

// Logger configuration
class Logger {
public:
    static void begin(); // NetworkManager removed - non-networking version
    static void setLogLevel(LogLevel minLevel);
    static LogLevel getLogLevel();
    
    // Convenience logging functions
    static void logCritical(const char* fmt, ...);
    static void logError(const char* fmt, ...);
    static void logWarn(const char* fmt, ...);
    static void logInfo(const char* fmt, ...);
    static void logDebug(const char* fmt, ...);
    
    // Generic logging function
    static void log(LogLevel level, const char* fmt, ...);
    
private:
    // NetworkManager removed - non-networking version
    static LogLevel currentLogLevel;
    static char logBuffer[512];
    
    static bool shouldLog(LogLevel level);
    static const char* getLevelString(LogLevel level);
};

// Convenience macros for easier migration
#define LOG_CRITICAL(...) Logger::logCritical(__VA_ARGS__)
#define LOG_ERROR(...) Logger::logError(__VA_ARGS__)
#define LOG_WARN(...) Logger::logWarn(__VA_ARGS__)
#define LOG_INFO(...) Logger::logInfo(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::logDebug(__VA_ARGS__)