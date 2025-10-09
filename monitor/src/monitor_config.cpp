#include "monitor_config.h"
#include "constants.h"
#include "network_manager.h"
#include <string.h>

extern void debugPrintf(const char* fmt, ...);

// CRC32 lookup table for efficient calculation
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t MonitorConfigManager::calculateCRC32(const MonitorConfig& data) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)&data;
    size_t length = sizeof(MonitorConfig) - sizeof(uint32_t); // Exclude CRC field itself
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool MonitorConfigManager::validateCRC(const MonitorConfig& data) {
    uint32_t calculatedCRC = calculateCRC32(data);
    return calculatedCRC == data.crc32;
}

void MonitorConfigManager::updateCRC(MonitorConfig& data) {
    data.crc32 = calculateCRC32(data);
}

void MonitorConfigManager::setDefaults() {
    debugPrintf("MonitorConfig: Setting default configuration\n");
    
    // Clear the structure
    memset(&config, 0, sizeof(MonitorConfig));
    
    // Set magic number
    config.magic = MONITOR_CONFIG_MAGIC;
    
    // Syslog Configuration (from constants.h)
    strncpy(config.syslogServer, SYSLOG_SERVER, sizeof(config.syslogServer) - 1);
    config.syslogPort = SYSLOG_PORT;
    strncpy(config.syslogHostname, SYSLOG_HOSTNAME, sizeof(config.syslogHostname) - 1);
    
    // MQTT Configuration (from constants.h)
    strncpy(config.mqttBroker, BROKER_HOST, sizeof(config.mqttBroker) - 1);
    config.mqttPort = BROKER_PORT;
    strcpy(config.mqttUsername, "");  // Empty by default
    strcpy(config.mqttPassword, "");  // Empty by default
    
    // Logging Configuration
    config.logLevel = LOG_INFO;  // Default to INFO level
    config.logToSerial = true;
    config.logToSyslog = true;
    
    // Network Configuration (from constants.h)
    config.wifiConnectTimeoutMs = WIFI_CONNECT_TIMEOUT_MS;
    config.mqttReconnectIntervalMs = MQTT_RECONNECT_INTERVAL_MS;
    config.networkStabilityTimeMs = NETWORK_STABILITY_TIME_MS;
    config.maxConnectRetries = MAX_CONNECT_RETRIES;
    
    // Sensor Configuration
    config.temperatureOffset = 0.0;
    config.enableSensorFiltering = true;
    config.sensorReadIntervalMs = 1000;  // 1 second default
    
    // System Configuration
    config.enableHeartbeat = true;
    config.heartbeatIntervalMs = 30000;  // 30 seconds
    config.enableWatchdog = true;
    
    // Calculate and set CRC
    updateCRC(config);
    
    configValid = true;
    configChanged = true;
    
    debugPrintf("MonitorConfig: Default configuration set\n");
}

bool MonitorConfigManager::isValidConfig(const MonitorConfig& data) {
    // Check magic number
    if (data.magic != MONITOR_CONFIG_MAGIC) {
        debugPrintf("MonitorConfig: Invalid magic number: 0x%08X\n", data.magic);
        return false;
    }
    
    // Validate CRC
    if (!validateCRC(data)) {
        debugPrintf("MonitorConfig: CRC validation failed\n");
        return false;
    }
    
    // Check string fields are null-terminated
    if (strnlen(data.syslogServer, sizeof(data.syslogServer)) == sizeof(data.syslogServer)) {
        debugPrintf("MonitorConfig: Syslog server string not null-terminated\n");
        return false;
    }
    
    if (strnlen(data.mqttBroker, sizeof(data.mqttBroker)) == sizeof(data.mqttBroker)) {
        debugPrintf("MonitorConfig: MQTT broker string not null-terminated\n");
        return false;
    }
    
    // Validate port ranges
    if (data.syslogPort == 0 || data.mqttPort == 0) {
        debugPrintf("MonitorConfig: Invalid port numbers\n");
        return false;
    }
    
    // Validate log level
    if (data.logLevel > LOG_DEBUG) {
        debugPrintf("MonitorConfig: Invalid log level: %d\n", data.logLevel);
        return false;
    }
    
    // Validate timeout values
    if (data.wifiConnectTimeoutMs == 0 || data.mqttReconnectIntervalMs == 0) {
        debugPrintf("MonitorConfig: Invalid timeout values\n");
        return false;
    }
    
    debugPrintf("MonitorConfig: Configuration validation passed\n");
    return true;
}

bool MonitorConfigManager::writeToEEPROM(const MonitorConfig& data) {
    debugPrintf("MonitorConfig: Writing configuration to EEPROM at address %d\n", MONITOR_CONFIG_EEPROM_ADDR);
    
    const uint8_t* bytes = (const uint8_t*)&data;
    for (size_t i = 0; i < sizeof(MonitorConfig); i++) {
        EEPROM.write(MONITOR_CONFIG_EEPROM_ADDR + i, bytes[i]);
    }
    
    // Note: Arduino Uno R4 EEPROM automatically commits changes
    // EEPROM.commit(); // Not needed on Arduino Uno R4
    
    debugPrintf("MonitorConfig: Configuration written to EEPROM successfully\n");
    return true;
}

bool MonitorConfigManager::readFromEEPROM(MonitorConfig& data) {
    debugPrintf("MonitorConfig: Reading configuration from EEPROM at address %d\n", MONITOR_CONFIG_EEPROM_ADDR);
    
    uint8_t* bytes = (uint8_t*)&data;
    for (size_t i = 0; i < sizeof(MonitorConfig); i++) {
        bytes[i] = EEPROM.read(MONITOR_CONFIG_EEPROM_ADDR + i);
    }
    
    debugPrintf("MonitorConfig: Configuration read from EEPROM\n");
    return true;
}

bool MonitorConfigManager::begin() {
    debugPrintf("MonitorConfig: Initializing configuration manager\n");
    
    // Note: Arduino Uno R4 EEPROM doesn't need begin() with parameter
    // EEPROM.begin(512);  // Not needed on Arduino Uno R4
    
    // Try to load configuration from EEPROM
    if (load()) {
        LOG_INFO("MonitorConfig: Configuration loaded from EEPROM");
        return true;
    } else {
        LOG_WARN("MonitorConfig: Failed to load configuration, using defaults");
        setDefaults();
        return save();  // Save default configuration
    }
}

bool MonitorConfigManager::load() {
    MonitorConfig tempConfig;
    
    if (!readFromEEPROM(tempConfig)) {
        debugPrintf("MonitorConfig: Failed to read from EEPROM\n");
        return false;
    }
    
    if (!isValidConfig(tempConfig)) {
        debugPrintf("MonitorConfig: Invalid configuration in EEPROM\n");
        return false;
    }
    
    // Configuration is valid, copy it
    memcpy(&config, &tempConfig, sizeof(MonitorConfig));
    configValid = true;
    configChanged = false;
    
    debugPrintf("MonitorConfig: Configuration loaded successfully\n");
    return true;
}

bool MonitorConfigManager::save() {
    if (!configValid) {
        debugPrintf("MonitorConfig: Cannot save invalid configuration\n");
        return false;
    }
    
    // Update CRC before saving
    updateCRC(config);
    
    if (!writeToEEPROM(config)) {
        LOG_ERROR("MonitorConfig: Failed to write configuration to EEPROM");
        return false;
    }
    
    configChanged = false;
    LOG_INFO("MonitorConfig: Configuration saved to EEPROM");
    return true;
}

bool MonitorConfigManager::saveIfChanged() {
    if (configChanged) {
        return save();
    }
    return true;  // No need to save
}

bool MonitorConfigManager::setSyslogServer(const char* server) {
    if (!server || strlen(server) >= sizeof(config.syslogServer)) {
        return false;
    }
    
    strncpy(config.syslogServer, server, sizeof(config.syslogServer) - 1);
    config.syslogServer[sizeof(config.syslogServer) - 1] = '\0';
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setSyslogPort(uint16_t port) {
    if (port == 0) return false;
    
    config.syslogPort = port;
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setSyslogHostname(const char* hostname) {
    if (!hostname || strlen(hostname) >= sizeof(config.syslogHostname)) {
        return false;
    }
    
    strncpy(config.syslogHostname, hostname, sizeof(config.syslogHostname) - 1);
    config.syslogHostname[sizeof(config.syslogHostname) - 1] = '\0';
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setMqttBroker(const char* broker) {
    if (!broker || strlen(broker) >= sizeof(config.mqttBroker)) {
        return false;
    }
    
    strncpy(config.mqttBroker, broker, sizeof(config.mqttBroker) - 1);
    config.mqttBroker[sizeof(config.mqttBroker) - 1] = '\0';
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setMqttPort(uint16_t port) {
    if (port == 0) return false;
    
    config.mqttPort = port;
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setMqttCredentials(const char* username, const char* password) {
    if (username && strlen(username) >= sizeof(config.mqttUsername)) return false;
    if (password && strlen(password) >= sizeof(config.mqttPassword)) return false;
    
    if (username) {
        strncpy(config.mqttUsername, username, sizeof(config.mqttUsername) - 1);
        config.mqttUsername[sizeof(config.mqttUsername) - 1] = '\0';
    } else {
        config.mqttUsername[0] = '\0';
    }
    
    if (password) {
        strncpy(config.mqttPassword, password, sizeof(config.mqttPassword) - 1);
        config.mqttPassword[sizeof(config.mqttPassword) - 1] = '\0';
    } else {
        config.mqttPassword[0] = '\0';
    }
    
    markAsChanged();
    return true;
}

bool MonitorConfigManager::setLogLevel(uint8_t level) {
    if (level > LOG_DEBUG) return false;
    
    config.logLevel = level;
    markAsChanged();
    return true;
}

bool MonitorConfigManager::factoryReset() {
    debugPrintf("MonitorConfig: Performing factory reset\n");
    
    setDefaults();
    bool result = save();
    
    if (result) {
        LOG_INFO("MonitorConfig: Factory reset completed successfully");
    } else {
        LOG_ERROR("MonitorConfig: Factory reset failed to save configuration");
    }
    
    return result;
}

void MonitorConfigManager::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize,
        "Config: valid=%s, changed=%s, syslog=%s:%d, mqtt=%s:%d, log=%d",
        configValid ? "YES" : "NO",
        configChanged ? "YES" : "NO",
        config.syslogServer,
        config.syslogPort,
        config.mqttBroker,
        config.mqttPort,
        config.logLevel
    );
}

void MonitorConfigManager::printConfiguration() {
    debugPrintf("=== Monitor Configuration ===\n");
    debugPrintf("Valid: %s\n", configValid ? "YES" : "NO");
    debugPrintf("Changed: %s\n", configChanged ? "YES" : "NO");
    debugPrintf("Syslog: %s:%d (%s)\n", config.syslogServer, config.syslogPort, config.syslogHostname);
    debugPrintf("MQTT: %s:%d\n", config.mqttBroker, config.mqttPort);
    debugPrintf("Log Level: %d\n", config.logLevel);
    debugPrintf("Temperature Offset: %.2f\n", config.temperatureOffset);
    debugPrintf("WiFi Timeout: %lums\n", config.wifiConnectTimeoutMs);
    debugPrintf("MQTT Interval: %lums\n", config.mqttReconnectIntervalMs);
    debugPrintf("=============================\n");
}

void MonitorConfigManager::applyToNetworkManager(NetworkManager* networkManager) {
    if (!networkManager || !configValid) {
        return;
    }
    
    debugPrintf("MonitorConfig: Applying configuration to NetworkManager\n");
    
    // Apply syslog configuration
    networkManager->setSyslogServer(config.syslogServer, config.syslogPort);
    
    // Apply MQTT configuration (if available in NetworkManager)
    // networkManager->setMqttBroker(config.mqttBroker, config.mqttPort);
    
    debugPrintf("MonitorConfig: NetworkManager configuration applied\n");
}

void MonitorConfigManager::applyToLogger() {
    if (!configValid) {
        return;
    }
    
    debugPrintf("MonitorConfig: Applying configuration to Logger\n");
    
    // Apply log level
    Logger::setLogLevel((LogLevel)config.logLevel);
    
    debugPrintf("MonitorConfig: Logger configuration applied (level=%d)\n", config.logLevel);
}

void MonitorConfigManager::resetToDefaults() {
    setDefaults();
    configValid = true;
    configChanged = true;
}