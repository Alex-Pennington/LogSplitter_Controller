#pragma once

#include <Arduino.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include "constants.h"

class NetworkManager {
private:
    WiFiClient wifiClient;
    MqttClient mqttClient;
    char clientId[32];
    
    // Connection state
    bool wifiConnected = false;
    bool mqttConnected = false;
    unsigned long lastConnectAttempt = 0;
    uint8_t connectRetries = 0;
    
    // Connection helpers
    bool connectWiFi();
    bool connectMQTT();
    void generateClientId();
    void printWiFiInfo();
    void printWiFiStatus();
    void scanNetworksDebug();

public:
    NetworkManager();
    
    // Initialization
    bool begin();
    
    // Connection management (non-blocking)
    void update();
    bool isConnected() const { return wifiConnected && mqttConnected; }
    bool isWiFiConnected() const { return wifiConnected; }
    bool isMQTTConnected() const { return mqttConnected; }
    
    // MQTT operations
    bool publish(const char* topic, const char* payload);
    bool subscribe(const char* topic);
    void poll() { mqttClient.poll(); }
    void setMessageCallback(void (*callback)(int));
    
    // Access to underlying client for message reading
    MqttClient& getMqttClient() { return mqttClient; }
    
    // Status reporting
    void printStatus();
};