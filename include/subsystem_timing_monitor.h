#pragma once

#include <Arduino.h>
#include "logger.h"

// Subsystem identifiers for timing monitoring
enum class SubsystemID {
    NETWORK_MANAGER,
    TELNET_SERVER,
    PRESSURE_MANAGER,
    SEQUENCE_CONTROLLER,
    RELAY_CONTROLLER,
    INPUT_MANAGER,
    SYSTEM_ERROR_MANAGER,
    SAFETY_SYSTEM,
    COMMAND_PROCESSING_SERIAL,
    COMMAND_PROCESSING_TELNET,
    TELEMETRY_PUBLISHING,
    MAIN_LOOP_TOTAL,
    COUNT  // Keep this last - used for array sizing
};

// Timing statistics for each subsystem
struct SubsystemTiming {
    const char* name;
    unsigned long totalTime;
    unsigned long maxTime;
    unsigned long callCount;
    unsigned long warningCount;
    unsigned long lastCallTime;
    bool isActive;  // Currently being timed
    unsigned long startTime;
};

// Configurable timing thresholds
struct TimingThresholds {
    unsigned long warningThreshold;  // Warn if subsystem takes longer than this
    unsigned long criticalThreshold; // Critical if subsystem takes longer than this
    bool enableLogging;              // Enable detailed timing logs
};

class SubsystemTimingMonitor {
public:
    SubsystemTimingMonitor();
    
    // Configuration
    void begin();
    void setThreshold(SubsystemID subsystem, unsigned long warningMs, unsigned long criticalMs);
    void setGlobalThresholds(unsigned long warningMs, unsigned long criticalMs);
    void enableDetailedLogging(bool enabled);
    
    // Timing operations
    void startTiming(SubsystemID subsystem);
    void endTiming(SubsystemID subsystem);
    
    // Automatic timing wrapper class
    class TimingScope {
    public:
        TimingScope(SubsystemTimingMonitor* monitor, SubsystemID subsystem);
        ~TimingScope();
    private:
        SubsystemTimingMonitor* monitor;
        SubsystemID subsystem;
    };
    
    // Statistics and reporting
    void getTimingReport(char* buffer, size_t bufferSize);
    void getSubsystemStatus(SubsystemID subsystem, char* buffer, size_t bufferSize);
    void logTimingReport();
    void resetStatistics();
    
    // Timeout analysis
    void analyzeTimeout(char* analysis, size_t bufferSize);
    SubsystemID getSlowestSubsystem();
    unsigned long getTotalExecutionTime();
    
    // Health monitoring
    bool hasAnyWarnings();
    bool hasAnyCriticalIssues();
    void checkHealthStatus();

private:
    SubsystemTiming timings[static_cast<int>(SubsystemID::COUNT)];
    TimingThresholds globalThresholds;
    TimingThresholds subsystemThresholds[static_cast<int>(SubsystemID::COUNT)];
    
    bool detailedLoggingEnabled;
    unsigned long lastReportTime;
    unsigned long reportInterval;
    
    // Helper methods
    const char* getSubsystemName(SubsystemID subsystem);
    void logWarning(SubsystemID subsystem, unsigned long duration);
    void logCritical(SubsystemID subsystem, unsigned long duration);
    void initializeSubsystemData();
};

// Convenience macro for automatic timing
#define TIME_SUBSYSTEM(monitor, subsystem) \
    SubsystemTimingMonitor::TimingScope _timing_scope(monitor, subsystem)