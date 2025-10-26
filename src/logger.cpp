#include "logger.h"
// NetworkManager include removed - non-networking version
#include "constants.h"
#include <stdarg.h>

// Static member definitions - NetworkManager removed
LogLevel Logger::currentLogLevel = LOG_INFO;  // Default to INFO level
char Logger::logBuffer[512];
bool Logger::telemetryEnabled = true;  // Default to enabled
Stream* Logger::telemetryStream = &Serial;  // Default to Serial, can be changed

void Logger::begin() {
    // NetworkManager removed - non-networking version
    // Default log level can be overridden by configuration
    currentLogLevel = LOG_INFO;
}

void Logger::setLogLevel(LogLevel minLevel) {
    currentLogLevel = minLevel;
}

LogLevel Logger::getLogLevel() {
    return currentLogLevel;
}

bool Logger::shouldLog(LogLevel level) {
    // Lower numerical values = higher priority
    // Always show critical and error messages
    if (level <= LOG_ERROR) {
        return (level <= currentLogLevel);
    }
    
    // For non-critical messages, respect telemetry setting
    if (!telemetryEnabled) {
        return false;
    }
    
    // Only log if message level is at or above current threshold
    return (level <= currentLogLevel);
}

const char* Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LOG_EMERGENCY: return "EMERG";
        case LOG_ALERT:     return "ALERT";
        case LOG_CRITICAL:  return "CRIT";
        case LOG_ERROR:     return "ERROR";
        case LOG_WARNING:   return "WARN";
        case LOG_NOTICE:    return "NOTICE";
        case LOG_INFO:      return "INFO";
        case LOG_DEBUG:     return "DEBUG";
        default:            return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    if (!shouldLog(level)) {
        return;  // Don't log if below threshold
    }
    
    // Format the message
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    
    // Network logging removed - non-networking version
    // All logging goes to telemetry stream - machine readable format
    if (telemetryStream) {
        unsigned long ts = millis();
        telemetryStream->print(ts);
        telemetryStream->print("|");
        telemetryStream->print(getLevelString(level));
        telemetryStream->print("|");
        telemetryStream->println(logBuffer);
    }
}

void Logger::logCritical(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    log(LOG_CRITICAL, "%s", logBuffer);
}

void Logger::logError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    log(LOG_ERROR, "%s", logBuffer);
}

void Logger::logWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    log(LOG_WARNING, "%s", logBuffer);
}

void Logger::logInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    log(LOG_INFO, "%s", logBuffer);
}

void Logger::logDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);
    log(LOG_DEBUG, "%s", logBuffer);
}

void Logger::setTelemetryEnabled(bool enabled) {
    telemetryEnabled = enabled;
}

bool Logger::isTelemetryEnabled() {
    return telemetryEnabled;
}

void Logger::setTelemetryStream(Stream* stream) {
    telemetryStream = stream;
}