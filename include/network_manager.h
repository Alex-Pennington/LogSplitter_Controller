#pragma once

#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <ArduinoMqttClient.h>
#include "constants.h"

// Connection states for non-blocking state machine
enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

enum class MQTTState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class NetworkManager {
private:
    WiFiClient wifiClient;
    MqttClient mqttClient;
    WiFiUDP udpClient;  // For syslog messages
    char clientId[32];
    char hostname[32];  // Store hostname locally since WiFi.getHostname() may not be available
    
    // Non-blocking connection state machine
    WiFiState wifiState = WiFiState::DISCONNECTED;
    MQTTState mqttState = MQTTState::DISCONNECTED;
    unsigned long connectStartTime = 0;
    unsigned long lastConnectAttempt = 0;
    uint8_t wifiRetries = 0;
    uint8_t mqttRetries = 0;
    
    // Connection stability and health monitoring
    unsigned long lastWiFiCheck = 0;
    unsigned long connectionUptime = 0;
    unsigned long totalDisconnections = 0;
    unsigned long failedPublishCount = 0;
    bool connectionStable = false;
    
    // Syslog configuration
    char syslogServer[64];
    int syslogPort;
    
    // MQTT broker configuration
    char mqttBrokerHost[64];
    int mqttBrokerPort;
    
    // Network bypass and timeout protection
    bool networkBypassMode = false;
    unsigned long lastUpdateTime = 0;
    unsigned long maxUpdateTime = 0;
    static const unsigned long MAX_UPDATE_TIMEOUT_MS = 100; // 100ms max for update()
    static const unsigned long NETWORK_BYPASS_THRESHOLD_MS = 500; // Bypass if update() takes >500ms
    
    // Connection helpers (now non-blocking)
    void updateWiFiConnection();
    void updateMQTTConnection();
    bool startWiFiConnection();
    bool startMQTTConnection();
    void generateClientId();
    void printWiFiInfo();
    void printWiFiStatus();
    void scanNetworksDebug();
    void updateConnectionHealth();

public:
    NetworkManager();
    
    // Initialization
    bool begin();
    
    // Connection management (completely non-blocking)
    void update();
    bool isConnected() const { return !networkBypassMode && (wifiState == WiFiState::CONNECTED && mqttState == MQTTState::CONNECTED); }
    bool isWiFiConnected() const { return !networkBypassMode && wifiState == WiFiState::CONNECTED; }
    bool isMQTTConnected() const { return !networkBypassMode && mqttState == MQTTState::CONNECTED; }
    bool isConnectionStable() const { return !networkBypassMode && connectionStable; }
    
    // Network bypass control
    void enableNetworkBypass(bool enable = true);
    bool isNetworkBypassed() const { return networkBypassMode; }
    unsigned long getMaxUpdateTime() const { return maxUpdateTime; }
    
    // MQTT operations (with timeout protection)
    bool publish(const char* topic, const char* payload);
    bool publishWithRetain(const char* topic, const char* payload);
    bool subscribe(const char* topic);
    void poll() { if (mqttState == MQTTState::CONNECTED) mqttClient.poll(); }
    void setMessageCallback(void (*callback)(int));
    
    // Syslog operations
    bool sendSyslog(const char* message, int severity = SYSLOG_SEVERITY);
    void setSyslogServer(const char* server, int port = SYSLOG_PORT);
    
    // MQTT broker configuration
    void setMqttBroker(const char* host, int port = BROKER_PORT);
    const char* getMqttBrokerHost() const { return mqttBrokerHost; }
    int getMqttBrokerPort() const { return mqttBrokerPort; }
    
    // Access to underlying client for message reading
    MqttClient& getMqttClient() { return mqttClient; }
    
    // Status and health reporting
    void printStatus();
    void printHealthStats();
    void getHealthString(char* buffer, size_t bufferSize);
    unsigned long getConnectionUptime() const { return connectionUptime; }
    unsigned long getFailedPublishCount() const { return failedPublishCount; }
    const char* getHostname() const { return hostname; }
};