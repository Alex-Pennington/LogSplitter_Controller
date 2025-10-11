#include "logger.h"
#include "network_manager.h"
#include "constants.h"
#include <stdarg.h>

// Static member definitions
NetworkManager* Logger::networkManager = nullptr;
LogLevel Logger::currentLogLevel = LOG_INFO;  // Default to INFO level
char Logger::logBuffer[512];

void Logger::begin(NetworkManager* netMgr) {
    networkManager = netMgr;
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
    
    // Send to syslog server if network is available
    bool syslogSent = false;
    if (networkManager && networkManager->isWiFiConnected()) {
        syslogSent = networkManager->sendSyslog(logBuffer, level);
    }
    
    // Only fallback to Serial if syslog is not available or fails
    // Once syslog is connected, all messages go through syslog only
    if (!syslogSent) {
        unsigned long ts = millis();
        Serial.print("[");
        Serial.print(ts);
        Serial.print("] [");
        Serial.print(getLevelString(level));
        Serial.print("] ");
        Serial.print("[SYSLOG_FAIL] ");
        Serial.println(logBuffer);
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