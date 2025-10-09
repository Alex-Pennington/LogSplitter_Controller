#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "logger.h"

// EEPROM address for monitor configuration (after NAU7802 calibration data)
// NAU7802 uses addresses 0-31, so we start at 32
#define MONITOR_CONFIG_EEPROM_ADDR 32

// Configuration magic number for validation
#define MONITOR_CONFIG_MAGIC 0x4D4F4E43  // "MONC" in hex

struct MonitorConfig {
    uint32_t magic;                    // Magic number for validation
    
    // Syslog Configuration
    char syslogServer[64];             // Syslog server IP/hostname
    uint16_t syslogPort;               // Syslog UDP port (514)
    char syslogHostname[32];           // Hostname for syslog messages
    
    // MQTT Configuration  
    char mqttBroker[64];               // MQTT broker IP/hostname
    uint16_t mqttPort;                 // MQTT broker port (1883)
    char mqttUsername[32];             // MQTT username (optional)
    char mqttPassword[32];             // MQTT password (optional)
    
    // Logging Configuration
    uint8_t logLevel;                  // Current log level (0-7)
    bool logToSerial;                  // Enable/disable serial logging
    bool logToSyslog;                  // Enable/disable syslog transmission
    
    // Network Configuration
    uint32_t wifiConnectTimeoutMs;     // WiFi connection timeout
    uint32_t mqttReconnectIntervalMs;  // MQTT reconnection interval
    uint32_t networkStabilityTimeMs;   // Network stability check time
    uint8_t maxConnectRetries;         // Maximum connection retry attempts
    
    // Sensor Configuration
    float temperatureOffset;           // Temperature sensor offset
    bool enableSensorFiltering;       // Enable sensor data filtering
    uint32_t sensorReadIntervalMs;     // Sensor reading interval
    
    // System Configuration
    bool enableHeartbeat;              // Enable MQTT heartbeat
    uint32_t heartbeatIntervalMs;      // Heartbeat transmission interval
    bool enableWatchdog;               // Enable watchdog timer
    
    // Reserved for future expansion
    uint8_t reserved[64];              // Reserved bytes for future features
    
    // CRC32 checksum (must be last field)
    uint32_t crc32;                    // Configuration integrity checksum
};

class MonitorConfigManager {
private:
    MonitorConfig config;
    bool configValid = false;
    bool configChanged = false;
    
    // Validation helpers
    bool isValidConfig(const MonitorConfig& data);
    void updateCRC(MonitorConfig& data);
    
    // CRC32 calculation helpers
    uint32_t calculateCRC32(const MonitorConfig& data);
    bool validateCRC(const MonitorConfig& data);
    
    // EEPROM helpers
    bool writeToEEPROM(const MonitorConfig& data);
    bool readFromEEPROM(MonitorConfig& data);
    
public:
    MonitorConfigManager() = default;
    
    // Initialization
    bool begin();
    
    // Load/Save
    bool load();
    bool save();
    bool saveIfChanged();
    
    // Reset to defaults (public method)
    void resetToDefaults();
    
    // Configuration validation
    bool isConfigValid() const { return configValid; }
    bool hasChanged() const { return configChanged; }
    void markAsChanged() { configChanged = true; }
    
    // Syslog Configuration
    const char* getSyslogServer() const { return config.syslogServer; }
    uint16_t getSyslogPort() const { return config.syslogPort; }
    const char* getSyslogHostname() const { return config.syslogHostname; }
    
    bool setSyslogServer(const char* server);
    bool setSyslogPort(uint16_t port);
    bool setSyslogHostname(const char* hostname);
    
    // MQTT Configuration
    const char* getMqttBroker() const { return config.mqttBroker; }
    uint16_t getMqttPort() const { return config.mqttPort; }
    const char* getMqttUsername() const { return config.mqttUsername; }
    const char* getMqttPassword() const { return config.mqttPassword; }
    
    bool setMqttBroker(const char* broker);
    bool setMqttPort(uint16_t port);
    bool setMqttCredentials(const char* username, const char* password);
    
    // Logging Configuration
    uint8_t getLogLevel() const { return config.logLevel; }
    bool getLogToSerial() const { return config.logToSerial; }
    bool getLogToSyslog() const { return config.logToSyslog; }
    
    bool setLogLevel(uint8_t level);
    void setLogToSerial(bool enable) { config.logToSerial = enable; markAsChanged(); }
    void setLogToSyslog(bool enable) { config.logToSyslog = enable; markAsChanged(); }
    
    // Network Configuration
    uint32_t getWifiConnectTimeoutMs() const { return config.wifiConnectTimeoutMs; }
    uint32_t getMqttReconnectIntervalMs() const { return config.mqttReconnectIntervalMs; }
    uint32_t getNetworkStabilityTimeMs() const { return config.networkStabilityTimeMs; }
    uint8_t getMaxConnectRetries() const { return config.maxConnectRetries; }
    
    void setWifiConnectTimeoutMs(uint32_t timeout) { config.wifiConnectTimeoutMs = timeout; markAsChanged(); }
    void setMqttReconnectIntervalMs(uint32_t interval) { config.mqttReconnectIntervalMs = interval; markAsChanged(); }
    void setNetworkStabilityTimeMs(uint32_t time) { config.networkStabilityTimeMs = time; markAsChanged(); }
    void setMaxConnectRetries(uint8_t retries) { config.maxConnectRetries = retries; markAsChanged(); }
    
    // Sensor Configuration
    float getTemperatureOffset() const { return config.temperatureOffset; }
    bool getEnableSensorFiltering() const { return config.enableSensorFiltering; }
    uint32_t getSensorReadIntervalMs() const { return config.sensorReadIntervalMs; }
    
    void setTemperatureOffset(float offset) { config.temperatureOffset = offset; markAsChanged(); }
    void setEnableSensorFiltering(bool enable) { config.enableSensorFiltering = enable; markAsChanged(); }
    void setSensorReadIntervalMs(uint32_t interval) { config.sensorReadIntervalMs = interval; markAsChanged(); }
    
    // System Configuration
    bool getEnableHeartbeat() const { return config.enableHeartbeat; }
    uint32_t getHeartbeatIntervalMs() const { return config.heartbeatIntervalMs; }
    bool getEnableWatchdog() const { return config.enableWatchdog; }
    
    void setEnableHeartbeat(bool enable) { config.enableHeartbeat = enable; markAsChanged(); }
    void setHeartbeatIntervalMs(uint32_t interval) { config.heartbeatIntervalMs = interval; markAsChanged(); }
    void setEnableWatchdog(bool enable) { config.enableWatchdog = enable; markAsChanged(); }

private:
    void setDefaults();
    
    // Factory reset
    bool factoryReset();
    
    // Status and debugging
    void getStatusString(char* buffer, size_t bufferSize);
    void printConfiguration();
    
    // Apply configuration to other components
    void applyToNetworkManager(class NetworkManager* networkManager);
    void applyToLogger();
};