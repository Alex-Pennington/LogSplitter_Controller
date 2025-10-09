#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "constants.h"

struct MonitorConfig {
    uint32_t magic;
    
    // Network configuration
    char syslogServer[64];
    int syslogPort;
    char mqttBrokerHost[64];
    int mqttBrokerPort;
    char hostname[32];
    
    // Debug configuration
    bool debugEnabled;
    
    // Reserved for future use
    uint8_t reserved[64];
    
    // CRC32 checksum (must be last field)
    uint32_t crc32;
};

class ConfigManager {
private:
    MonitorConfig config;
    bool configValid = false;
    
    // CRC32 calculation
    uint32_t calculateCRC32(const uint8_t* data, size_t length) const;
    void updateCRC32();
    bool validateCRC32() const;
    
public:
    ConfigManager();
    
    // Configuration management
    bool loadFromEEPROM();
    bool saveToEEPROM();
    void setDefaults();
    bool isConfigValid() const { return configValid; }
    
    // Syslog configuration
    const char* getSyslogServer() const { return config.syslogServer; }
    int getSyslogPort() const { return config.syslogPort; }
    void setSyslogServer(const char* server, int port = 514);
    
    // MQTT configuration
    const char* getMqttBrokerHost() const { return config.mqttBrokerHost; }
    int getMqttBrokerPort() const { return config.mqttBrokerPort; }
    void setMqttBroker(const char* host, int port = 1883);
    
    // Hostname configuration
    const char* getHostname() const { return config.hostname; }
    void setHostname(const char* hostname);
    
    // Debug configuration
    bool getDebugEnabled() const { return config.debugEnabled; }
    void setDebugEnabled(bool enabled);
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
};