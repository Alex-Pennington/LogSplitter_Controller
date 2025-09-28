#pragma once

#include <Arduino.h>
#include <WiFiS3.h>
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
    char clientId[32];
    
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
    bool isConnected() const { return (wifiState == WiFiState::CONNECTED && mqttState == MQTTState::CONNECTED); }
    bool isWiFiConnected() const { return wifiState == WiFiState::CONNECTED; }
    bool isMQTTConnected() const { return mqttState == MQTTState::CONNECTED; }
    bool isConnectionStable() const { return connectionStable; }
    
    // MQTT operations (with timeout protection)
    bool publish(const char* topic, const char* payload);
    bool subscribe(const char* topic);
    void poll() { if (mqttState == MQTTState::CONNECTED) mqttClient.poll(); }
    void setMessageCallback(void (*callback)(int));
    
    // Access to underlying client for message reading
    MqttClient& getMqttClient() { return mqttClient; }
    
    // Status and health reporting
    void printStatus();
    void printHealthStats();
    void getHealthString(char* buffer, size_t bufferSize);
    unsigned long getConnectionUptime() const { return connectionUptime; }
    unsigned long getFailedPublishCount() const { return failedPublishCount; }
};