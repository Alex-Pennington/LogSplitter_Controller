#include "subsystem_timing_monitor.h"
#include "constants.h"
#include <string.h>

SubsystemTimingMonitor::SubsystemTimingMonitor() :
    detailedLoggingEnabled(false),
    lastReportTime(0),
    reportInterval(60000) // Report every 60 seconds
{
    initializeSubsystemData();
}

void SubsystemTimingMonitor::begin() {
    // Set default global thresholds
    globalThresholds.warningThreshold = 100;  // 100ms warning
    globalThresholds.criticalThreshold = 500; // 500ms critical  
    globalThresholds.enableLogging = true;
    
    // Initialize per-subsystem thresholds to global defaults
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        subsystemThresholds[i] = globalThresholds;
    }
    
    // Set specific thresholds for known slow operations
    setThreshold(SubsystemID::NETWORK_MANAGER, 200, 1000);  // Network can be slower
    setThreshold(SubsystemID::TELNET_SERVER, 50, 200);      // Telnet should be fast
    setThreshold(SubsystemID::COMMAND_PROCESSING_SERIAL, 150, 500);
    setThreshold(SubsystemID::COMMAND_PROCESSING_TELNET, 150, 500);
    setThreshold(SubsystemID::MAIN_LOOP_TOTAL, 5000, 8000); // Main loop total time
    
    resetStatistics();
    
    LOG_INFO("Subsystem timing monitor initialized with %dms warning, %dms critical thresholds", 
             globalThresholds.warningThreshold, globalThresholds.criticalThreshold);
}

void SubsystemTimingMonitor::initializeSubsystemData() {
    const char* names[] = {
        "NetworkManager",
        "TelnetServer", 
        "PressureManager",
        "SequenceController",
        "RelayController",
        "InputManager",
        "SystemErrorManager",
        "SafetySystem",
        "CommandProcessing_Serial",
        "CommandProcessing_Telnet",
        "TelemetryPublishing",
        "MainLoop_Total"
    };
    
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        timings[i].name = names[i];
        timings[i].totalTime = 0;
        timings[i].maxTime = 0;
        timings[i].callCount = 0;
        timings[i].warningCount = 0;
        timings[i].lastCallTime = 0;
        timings[i].isActive = false;
        timings[i].startTime = 0;
    }
}

void SubsystemTimingMonitor::setThreshold(SubsystemID subsystem, unsigned long warningMs, unsigned long criticalMs) {
    int idx = static_cast<int>(subsystem);
    if (idx < static_cast<int>(SubsystemID::COUNT)) {
        subsystemThresholds[idx].warningThreshold = warningMs;
        subsystemThresholds[idx].criticalThreshold = criticalMs;
        subsystemThresholds[idx].enableLogging = true;
    }
}

void SubsystemTimingMonitor::setGlobalThresholds(unsigned long warningMs, unsigned long criticalMs) {
    globalThresholds.warningThreshold = warningMs;
    globalThresholds.criticalThreshold = criticalMs;
}

void SubsystemTimingMonitor::enableDetailedLogging(bool enabled) {
    detailedLoggingEnabled = enabled;
    LOG_INFO("Subsystem timing detailed logging %s", enabled ? "ENABLED" : "DISABLED");
}

void SubsystemTimingMonitor::startTiming(SubsystemID subsystem) {
    int idx = static_cast<int>(subsystem);
    if (idx < static_cast<int>(SubsystemID::COUNT)) {
        timings[idx].isActive = true;
        timings[idx].startTime = millis();
    }
}

void SubsystemTimingMonitor::endTiming(SubsystemID subsystem) {
    unsigned long endTime = millis();
    int idx = static_cast<int>(subsystem);
    
    if (idx >= static_cast<int>(SubsystemID::COUNT) || !timings[idx].isActive) {
        return;
    }
    
    timings[idx].isActive = false;
    unsigned long duration = endTime - timings[idx].startTime;
    
    // Update statistics
    timings[idx].totalTime += duration;
    timings[idx].callCount++;
    timings[idx].lastCallTime = duration;
    
    if (duration > timings[idx].maxTime) {
        timings[idx].maxTime = duration;
    }
    
    // Check thresholds
    TimingThresholds& thresholds = subsystemThresholds[idx];
    
    if (duration >= thresholds.criticalThreshold) {
        logCritical(subsystem, duration);
    } else if (duration >= thresholds.warningThreshold) {
        timings[idx].warningCount++;
        logWarning(subsystem, duration);
    } else if (detailedLoggingEnabled) {
        LOG_DEBUG("%s completed in %lums", timings[idx].name, duration);
    }
}

// Automatic timing scope implementation
SubsystemTimingMonitor::TimingScope::TimingScope(SubsystemTimingMonitor* mon, SubsystemID sub) :
    monitor(mon), subsystem(sub) {
    if (monitor) {
        monitor->startTiming(subsystem);
    }
}

SubsystemTimingMonitor::TimingScope::~TimingScope() {
    if (monitor) {
        monitor->endTiming(subsystem);
    }
}

void SubsystemTimingMonitor::getTimingReport(char* buffer, size_t bufferSize) {
    int offset = 0;
    offset += snprintf(buffer + offset, bufferSize - offset, 
        "=== Subsystem Timing Report ===\\n");
    
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        if (timings[i].callCount > 0) {
            unsigned long avgTime = timings[i].totalTime / timings[i].callCount;
            offset += snprintf(buffer + offset, bufferSize - offset,
                "%s: calls=%lu avg=%lums max=%lums last=%lums warns=%lu\\n",
                timings[i].name,
                timings[i].callCount,
                avgTime,
                timings[i].maxTime,
                timings[i].lastCallTime,
                timings[i].warningCount);
            
            if (offset >= bufferSize - 100) break; // Prevent buffer overflow
        }
    }
}

void SubsystemTimingMonitor::getSubsystemStatus(SubsystemID subsystem, char* buffer, size_t bufferSize) {
    int idx = static_cast<int>(subsystem);
    if (idx >= static_cast<int>(SubsystemID::COUNT)) {
        snprintf(buffer, bufferSize, "Invalid subsystem ID");
        return;
    }
    
    SubsystemTiming& timing = timings[idx];
    if (timing.callCount > 0) {
        unsigned long avgTime = timing.totalTime / timing.callCount;
        snprintf(buffer, bufferSize,
            "%s: %lu calls, avg %lums, max %lums, last %lums, %lu warnings",
            timing.name, timing.callCount, avgTime, timing.maxTime, 
            timing.lastCallTime, timing.warningCount);
    } else {
        snprintf(buffer, bufferSize, "%s: No calls recorded", timing.name);
    }
}

void SubsystemTimingMonitor::logTimingReport() {
    LOG_INFO("=== Subsystem Timing Report ===");
    
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        if (timings[i].callCount > 0) {
            unsigned long avgTime = timings[i].totalTime / timings[i].callCount;
            LOG_INFO("%s: calls=%lu avg=%lums max=%lums warns=%lu",
                timings[i].name, timings[i].callCount, avgTime, 
                timings[i].maxTime, timings[i].warningCount);
        }
    }
}

void SubsystemTimingMonitor::resetStatistics() {
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        timings[i].totalTime = 0;
        timings[i].maxTime = 0;
        timings[i].callCount = 0;
        timings[i].warningCount = 0;
        timings[i].lastCallTime = 0;
        timings[i].isActive = false;
        timings[i].startTime = 0;
    }
    LOG_INFO("Subsystem timing statistics reset");
}

void SubsystemTimingMonitor::analyzeTimeout(char* analysis, size_t bufferSize) {
    SubsystemID slowest = getSlowestSubsystem();
    int slowestIdx = static_cast<int>(slowest);
    
    unsigned long totalTime = getTotalExecutionTime();
    
    snprintf(analysis, bufferSize,
        "TIMEOUT ANALYSIS: Total execution %lums. Slowest: %s (max %lums, last %lums, %lu warnings). "
        "Active subsystems: ",
        totalTime, timings[slowestIdx].name, timings[slowestIdx].maxTime,
        timings[slowestIdx].lastCallTime, timings[slowestIdx].warningCount);
    
    // Add list of currently active subsystems
    size_t len = strlen(analysis);
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT) && len < bufferSize - 50; i++) {
        if (timings[i].isActive) {
            len += snprintf(analysis + len, bufferSize - len, "%s ", timings[i].name);
        }
    }
}

SubsystemID SubsystemTimingMonitor::getSlowestSubsystem() {
    SubsystemID slowest = SubsystemID::NETWORK_MANAGER;
    unsigned long maxTime = 0;
    
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        if (timings[i].maxTime > maxTime) {
            maxTime = timings[i].maxTime;
            slowest = static_cast<SubsystemID>(i);
        }
    }
    
    return slowest;
}

unsigned long SubsystemTimingMonitor::getTotalExecutionTime() {
    unsigned long total = 0;
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        total += timings[i].totalTime;
    }
    return total;
}

bool SubsystemTimingMonitor::hasAnyWarnings() {
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        if (timings[i].warningCount > 0) {
            return true;
        }
    }
    return false;
}

bool SubsystemTimingMonitor::hasAnyCriticalIssues() {
    for (int i = 0; i < static_cast<int>(SubsystemID::COUNT); i++) {
        if (timings[i].maxTime >= subsystemThresholds[i].criticalThreshold) {
            return true;
        }
    }
    return false;
}

void SubsystemTimingMonitor::checkHealthStatus() {
    unsigned long now = millis();
    
    // Periodic reporting
    if (now - lastReportTime >= reportInterval) {
        lastReportTime = now;
        
        if (hasAnyWarnings()) {
            LOG_WARN("Subsystem timing issues detected - generating report");
            logTimingReport();
        } else {
            LOG_INFO("Subsystem timing health check: All systems nominal");
        }
        
        // Check for critical issues only during periodic reporting
        if (hasAnyCriticalIssues()) {
            LOG_ERROR("Critical timing issues detected in one or more subsystems");
        }
    }
}

const char* SubsystemTimingMonitor::getSubsystemName(SubsystemID subsystem) {
    int idx = static_cast<int>(subsystem);
    if (idx < static_cast<int>(SubsystemID::COUNT)) {
        return timings[idx].name;
    }
    return "Unknown";
}

void SubsystemTimingMonitor::logWarning(SubsystemID subsystem, unsigned long duration) {
    TimingThresholds& thresholds = subsystemThresholds[static_cast<int>(subsystem)];
    if (thresholds.enableLogging) {
        LOG_WARN("%s took %lums (warning threshold: %lums)", 
                 getSubsystemName(subsystem), duration, thresholds.warningThreshold);
    }
}

void SubsystemTimingMonitor::logCritical(SubsystemID subsystem, unsigned long duration) {
    TimingThresholds& thresholds = subsystemThresholds[static_cast<int>(subsystem)];
    LOG_CRITICAL("%s took %lums (CRITICAL threshold: %lums) - potential system bottleneck", 
                 getSubsystemName(subsystem), duration, thresholds.criticalThreshold);
}