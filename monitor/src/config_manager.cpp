#include "config_manager.h"
#include "logger.h"

// Forward declaration for debugPrintf
extern void debugPrintf(const char* fmt, ...);

ConfigManager::ConfigManager() {
    setDefaults();
}

void ConfigManager::setDefaults() {
    config.magic = CONFIG_MAGIC;
    
    // Set default network configuration
    strncpy(config.syslogServer, SYSLOG_SERVER, sizeof(config.syslogServer) - 1);
    config.syslogServer[sizeof(config.syslogServer) - 1] = '\0';
    config.syslogPort = SYSLOG_PORT;
    
    strncpy(config.mqttBrokerHost, BROKER_HOST, sizeof(config.mqttBrokerHost) - 1);
    config.mqttBrokerHost[sizeof(config.mqttBrokerHost) - 1] = '\0';
    config.mqttBrokerPort = BROKER_PORT;
    
    strncpy(config.hostname, SYSLOG_HOSTNAME, sizeof(config.hostname) - 1);
    config.hostname[sizeof(config.hostname) - 1] = '\0';
    
    // Set default debug configuration
    config.debugEnabled = false;
    
    // Clear reserved space
    memset(config.reserved, 0, sizeof(config.reserved));
    
    updateCRC32();
    configValid = true;
}

bool ConfigManager::loadFromEEPROM() {
    debugPrintf("ConfigManager: Loading configuration from EEPROM\n");
    
    // Read configuration from EEPROM
    for (size_t i = 0; i < sizeof(MonitorConfig); i++) {
        ((uint8_t*)&config)[i] = EEPROM.read(CONFIG_EEPROM_ADDR + i);
    }
    
    // Validate magic number
    if (config.magic != CONFIG_MAGIC) {
        debugPrintf("ConfigManager: Invalid magic number in EEPROM (0x%08X), using defaults\n", config.magic);
        setDefaults();
        return false;
    }
    
    // Validate CRC32
    if (!validateCRC32()) {
        debugPrintf("ConfigManager: CRC32 validation failed, using defaults\n");
        setDefaults();
        return false;
    }
    
    configValid = true;
    debugPrintf("ConfigManager: Configuration loaded successfully from EEPROM\n");
    debugPrintf("  Syslog: %s:%d\n", config.syslogServer, config.syslogPort);
    debugPrintf("  MQTT: %s:%d\n", config.mqttBrokerHost, config.mqttBrokerPort);
    debugPrintf("  Hostname: %s\n", config.hostname);
    debugPrintf("  Debug: %s\n", config.debugEnabled ? "ON" : "OFF");
    
    return true;
}

bool ConfigManager::saveToEEPROM() {
    if (!configValid) {
        debugPrintf("ConfigManager: Cannot save invalid configuration\n");
        return false;
    }
    
    // Update CRC32 before saving
    updateCRC32();
    
    debugPrintf("ConfigManager: Saving configuration to EEPROM\n");
    
    // Write configuration to EEPROM
    for (size_t i = 0; i < sizeof(MonitorConfig); i++) {
        EEPROM.write(CONFIG_EEPROM_ADDR + i, ((uint8_t*)&config)[i]);
    }
    
    debugPrintf("ConfigManager: Configuration saved successfully\n");
    return true;
}

void ConfigManager::setSyslogServer(const char* server, int port) {
    if (server && strlen(server) < sizeof(config.syslogServer)) {
        strncpy(config.syslogServer, server, sizeof(config.syslogServer) - 1);
        config.syslogServer[sizeof(config.syslogServer) - 1] = '\0';
        config.syslogPort = port;
        updateCRC32();
        debugPrintf("ConfigManager: Syslog server set to %s:%d\n", server, port);
    }
}

void ConfigManager::setMqttBroker(const char* host, int port) {
    if (host && strlen(host) < sizeof(config.mqttBrokerHost)) {
        strncpy(config.mqttBrokerHost, host, sizeof(config.mqttBrokerHost) - 1);
        config.mqttBrokerHost[sizeof(config.mqttBrokerHost) - 1] = '\0';
        config.mqttBrokerPort = port;
        updateCRC32();
        debugPrintf("ConfigManager: MQTT broker set to %s:%d\n", host, port);
    }
}

void ConfigManager::setHostname(const char* hostname) {
    if (hostname && strlen(hostname) < sizeof(config.hostname)) {
        strncpy(config.hostname, hostname, sizeof(config.hostname) - 1);
        config.hostname[sizeof(config.hostname) - 1] = '\0';
        updateCRC32();
        debugPrintf("ConfigManager: Hostname set to %s\n", hostname);
    }
}

void ConfigManager::setDebugEnabled(bool enabled) {
    config.debugEnabled = enabled;
    updateCRC32();
    debugPrintf("ConfigManager: Debug %s\n", enabled ? "enabled" : "disabled");
}

uint32_t ConfigManager::calculateCRC32(const uint8_t* data, size_t length) const {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

void ConfigManager::updateCRC32() {
    // Calculate CRC32 for everything except the CRC32 field itself
    config.crc32 = calculateCRC32((uint8_t*)&config, sizeof(MonitorConfig) - sizeof(uint32_t));
}

bool ConfigManager::validateCRC32() const {
    uint32_t calculatedCRC = calculateCRC32((uint8_t*)&config, sizeof(MonitorConfig) - sizeof(uint32_t));
    return calculatedCRC == config.crc32;
}

void ConfigManager::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize,
        "Config: Valid=%s, Syslog=%s:%d, MQTT=%s:%d, Host=%s, Debug=%s",
        configValid ? "YES" : "NO",
        config.syslogServer, config.syslogPort,
        config.mqttBrokerHost, config.mqttBrokerPort,
        config.hostname,
        config.debugEnabled ? "ON" : "OFF");
}