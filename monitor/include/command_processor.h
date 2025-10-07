#pragma once

#include "constants.h"
#include "network_manager.h"
#include "monitor_system.h"

class CommandProcessor {
public:
    CommandProcessor();
    
    void begin(NetworkManager* network, MonitorSystem* monitor);
    void setHeartbeatAnimation(class HeartbeatAnimation* heartbeat) { heartbeatAnimation = heartbeat; }
    bool processCommand(char* commandBuffer, bool fromMqtt, char* response, size_t responseSize);
    
private:
    NetworkManager* networkManager;
    MonitorSystem* monitorSystem;
    class HeartbeatAnimation* heartbeatAnimation = nullptr;
    
    // Command handlers
    void handleHelp(char* response, size_t responseSize, bool fromMqtt);
    void handleShow(char* response, size_t responseSize, bool fromMqtt);
    void handleStatus(char* response, size_t responseSize);
    void handleSet(char* param, char* value, char* response, size_t responseSize);
    void handleDebug(char* param, char* response, size_t responseSize);
    void handleNetwork(char* subcommand, char* response, size_t responseSize);
    void handleReset(char* param, char* response, size_t responseSize);
    void handleTest(char* param, char* response, size_t responseSize);
    void handleSyslog(char* param, char* response, size_t responseSize);
    void handleMonitor(char* param, char* value, char* response, size_t responseSize);
    void handleWeight(char* param, char* value, char* response, size_t responseSize);
    void handleTemperature(char* param, char* value, char* response, size_t responseSize);
    void handleLCD(char* param, char* value, char* response, size_t responseSize);
    void handleLogLevel(const char* param, char* response, size_t responseSize);
    void handleHeartbeat(char* param, char* response, size_t responseSize);
};

class CommandValidator {
public:
    static bool isValidCommand(const char* cmd);
    static bool isValidSetParam(const char* param);
    static bool validateCommand(const char* command);
    static bool validateSetCommand(const char* param, const char* value);
    static void sanitizeInput(char* input, size_t maxLength);
    static bool checkRateLimit();
};