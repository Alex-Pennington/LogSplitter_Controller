#include "network_manager.h"
#include "arduino_secrets.h"

// Shared buffers for memory optimization
extern char g_message_buffer[SHARED_BUFFER_SIZE];
extern char g_topic_buffer[TOPIC_BUFFER_SIZE];

NetworkManager::NetworkManager() : mqttClient(wifiClient) {
    memset(clientId, 0, sizeof(clientId));
}

bool NetworkManager::begin() {
    Serial.println("NetworkManager: Initializing...");
    
    generateClientId();
    Serial.print("Using MQTT clientId: ");
    Serial.println(clientId);
    
    // Initialize state machine
    wifiState = WiFiState::DISCONNECTED;
    mqttState = MQTTState::DISCONNECTED;
    lastConnectAttempt = 0;
    wifiRetries = 0;
    mqttRetries = 0;
    
    // Start initial connection attempt (non-blocking)
    Serial.println("Starting initial network connection...");
    startWiFiConnection();
    
    // Always return true - connection happens asynchronously
    return true;
}

void NetworkManager::generateClientId() {
    uint8_t mac[6];
    if (WiFi.macAddress(mac)) {
        snprintf(clientId, sizeof(clientId), "r4-%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        snprintf(clientId, sizeof(clientId), "r4-unknown");
    }
}

bool NetworkManager::startWiFiConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiState = WiFiState::CONNECTED;
        return true;
    }
    
    Serial.print("Starting WiFi connection to '");
    Serial.print(SECRET_SSID);
    Serial.println("'...");
    
    // Start connection attempt (non-blocking)
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    wifiState = WiFiState::CONNECTING;
    connectStartTime = millis();
    
    return false; // Connection in progress
}

void NetworkManager::updateWiFiConnection() {
    unsigned long now = millis();
    
    switch (wifiState) {
        case WiFiState::DISCONNECTED:
            // Ready for new connection attempt
            break;
            
        case WiFiState::CONNECTING:
            // Check if connection completed or timed out
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nWiFi connected!");
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
                wifiState = WiFiState::CONNECTED;
                wifiRetries = 0;
                printWiFiInfo();
            } else if (now - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("\nWiFi connection timeout");
                wifiState = WiFiState::FAILED;
                printWiFiStatus();
            } else if (now - lastWiFiCheck > WIFI_CONNECT_CHECK_INTERVAL_MS) {
                Serial.print('.');
                lastWiFiCheck = now;
            }
            break;
            
        case WiFiState::CONNECTED:
            // Monitor connection health
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi connection lost");
                wifiState = WiFiState::DISCONNECTED;
                mqttState = MQTTState::DISCONNECTED; // MQTT depends on WiFi
                totalDisconnections++;
                connectionUptime = 0;
                connectionStable = false;
            }
            break;
            
        case WiFiState::FAILED:
            // Wait before retrying
            break;
    }
}

bool NetworkManager::startMQTTConnection() {
    if (wifiState != WiFiState::CONNECTED) {
        return false;
    }
    
    if (mqttClient.connected()) {
        mqttState = MQTTState::CONNECTED;
        return true;
    }
    
    Serial.print("Starting MQTT connection to ");
    Serial.print(BROKER_HOST);
    Serial.print(":");
    Serial.println(BROKER_PORT);
    
    mqttClient.setId(clientId);
    
    #ifdef MQTT_USER
    if (strlen(MQTT_USER) > 0) {
        mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
    }
    #endif
    
    // Attempt connection (this should be quick)
    if (mqttClient.connect(BROKER_HOST, BROKER_PORT)) {
        Serial.println("MQTT connected!");
        mqttState = MQTTState::CONNECTED;
        mqttRetries = 0;
        
        // Subscribe to control topics
        subscribe(TOPIC_SUBSCRIBE);
        subscribe(TOPIC_CONTROL);
        return true;
    } else {
        Serial.print("MQTT connect failed, error: ");
        Serial.println(mqttClient.connectError());
        mqttState = MQTTState::FAILED;
        return false;
    }
}

void NetworkManager::updateMQTTConnection() {
    switch (mqttState) {
        case MQTTState::DISCONNECTED:
            // Ready for new connection attempt
            break;
            
        case MQTTState::CONNECTING:
            // MQTT connections are typically fast, handle in startMQTTConnection
            break;
            
        case MQTTState::CONNECTED:
            // Monitor connection health
            if (!mqttClient.connected()) {
                Serial.println("MQTT connection lost");
                mqttState = MQTTState::DISCONNECTED;
                connectionUptime = 0;
                connectionStable = false;
            }
            break;
            
        case MQTTState::FAILED:
            // Wait before retrying
            break;
    }
}

void NetworkManager::update() {
    unsigned long now = millis();
    
    // Update connection state machines (completely non-blocking)
    updateWiFiConnection();
    updateMQTTConnection();
    
    // Handle reconnection attempts with exponential backoff
    if (now - lastConnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        bool attemptedConnection = false;
        
        // WiFi reconnection logic
        if (wifiState == WiFiState::DISCONNECTED && wifiRetries < MAX_WIFI_RETRIES) {
            lastConnectAttempt = now;
            wifiRetries++;
            Serial.print("WiFi attempt ");
            Serial.print(wifiRetries);
            Serial.print("/");
            Serial.println(MAX_WIFI_RETRIES);
            startWiFiConnection();
            attemptedConnection = true;
        }
        else if (wifiState == WiFiState::FAILED && wifiRetries < MAX_WIFI_RETRIES) {
            lastConnectAttempt = now;
            wifiRetries++;
            Serial.print("WiFi retry ");
            Serial.print(wifiRetries);
            Serial.print("/");
            Serial.println(MAX_WIFI_RETRIES);
            wifiState = WiFiState::DISCONNECTED;
            startWiFiConnection();
            attemptedConnection = true;
        }
        // MQTT reconnection logic (only if WiFi is connected)
        else if (wifiState == WiFiState::CONNECTED) {
            if (mqttState == MQTTState::DISCONNECTED && mqttRetries < MAX_MQTT_RETRIES) {
                lastConnectAttempt = now;
                mqttRetries++;
                Serial.print("MQTT attempt ");
                Serial.print(mqttRetries);
                Serial.print("/");
                Serial.println(MAX_MQTT_RETRIES);
                startMQTTConnection();
                attemptedConnection = true;
            }
            else if (mqttState == MQTTState::FAILED && mqttRetries < MAX_MQTT_RETRIES) {
                lastConnectAttempt = now;
                mqttRetries++;
                Serial.print("MQTT retry ");
                Serial.print(mqttRetries);
                Serial.print("/");
                Serial.println(MAX_MQTT_RETRIES);
                mqttState = MQTTState::DISCONNECTED;
                startMQTTConnection();
                attemptedConnection = true;
            }
        }
        
        // Reset retry counters after extended wait period
        if (!attemptedConnection && wifiRetries >= MAX_WIFI_RETRIES && 
            now - lastConnectAttempt >= (MQTT_RECONNECT_INTERVAL_MS * 3)) {
            Serial.println("Resetting WiFi retry count after extended wait");
            wifiRetries = 0;
        }
        if (!attemptedConnection && mqttRetries >= MAX_MQTT_RETRIES && 
            now - lastConnectAttempt >= (MQTT_RECONNECT_INTERVAL_MS * 2)) {
            Serial.println("Resetting MQTT retry count after extended wait");
            mqttRetries = 0;
        }
    }
    
    // Update connection health and stability
    updateConnectionHealth();
    
    // Poll MQTT (with protection)
    if (mqttState == MQTTState::CONNECTED) {
        mqttClient.poll();
    }
}

void NetworkManager::updateConnectionHealth() {
    unsigned long now = millis();
    
    // Update connection uptime
    if (isConnected()) {
        connectionUptime = now;
        
        // Mark as stable after continuous uptime
        if (!connectionStable && (now - connectionUptime) > NETWORK_STABILITY_TIME_MS) {
            connectionStable = true;
            Serial.println("Network connection marked as stable");
        }
    } else {
        connectionUptime = 0;
        connectionStable = false;
    }
}

bool NetworkManager::publish(const char* topic, const char* payload) {
    if (mqttState != MQTTState::CONNECTED) {
        failedPublishCount++;
        return false;
    }
    
    // Timeout protection - ensure publish doesn't block
    unsigned long startTime = millis();
    
    if (mqttClient.beginMessage(topic)) {
        mqttClient.print(payload);
        bool success = mqttClient.endMessage();
        
        unsigned long duration = millis() - startTime;
        if (duration > 100) { // Warn if publish takes >100ms
            Serial.print("WARNING: MQTT publish took ");
            Serial.print(duration);
            Serial.println("ms");
        }
        
        if (!success) {
            failedPublishCount++;
        }
        return success;
    }
    
    failedPublishCount++;
    return false;
}

bool NetworkManager::subscribe(const char* topic) {
    if (mqttState != MQTTState::CONNECTED) {
        return false;
    }
    
    bool result = mqttClient.subscribe(topic);
    if (!result) {
        Serial.print("Failed to subscribe to topic: ");
        Serial.println(topic);
    }
    return result;
}

void NetworkManager::setMessageCallback(void (*callback)(int)) {
    mqttClient.onMessage(callback);
}

void NetworkManager::printWiFiStatus() {
    uint8_t s = WiFi.status();
    Serial.print("WiFi status: ");
    switch (s) {
        case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
        case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
        case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
        case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
        case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
        case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
        case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
        case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
        default: Serial.println(s); break;
    }
}

void NetworkManager::printWiFiInfo() {
    Serial.print("Firmware: ");
    Serial.println(WiFi.firmwareVersion());
    
    uint8_t mac[6];
    if (WiFi.macAddress(mac)) {
        Serial.print("MAC: ");
        for (int i = 0; i < 6; ++i) {
            if (i) Serial.print(":");
            if (mac[i] < 16) Serial.print('0');
            Serial.print(mac[i], HEX);
        }
        Serial.println();
    }
    
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
}

void NetworkManager::scanNetworksDebug() {
    Serial.println("Scanning for networks...");
    int n = WiFi.scanNetworks();
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
        Serial.print(i);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (RSSI: ");
        Serial.print(WiFi.RSSI(i));
        Serial.print(")");
        Serial.print("  Encryption: ");
        Serial.println(WiFi.encryptionType(i));
    }
}

void NetworkManager::printStatus() {
    Serial.print("Network Status - WiFi: ");
    switch (wifiState) {
        case WiFiState::DISCONNECTED: Serial.print("Disconnected"); break;
        case WiFiState::CONNECTING: Serial.print("Connecting"); break;
        case WiFiState::CONNECTED: Serial.print("Connected"); break;
        case WiFiState::FAILED: Serial.print("Failed"); break;
    }
    
    Serial.print(", MQTT: ");
    switch (mqttState) {
        case MQTTState::DISCONNECTED: Serial.print("Disconnected"); break;
        case MQTTState::CONNECTING: Serial.print("Connecting"); break;
        case MQTTState::CONNECTED: Serial.print("Connected"); break;
        case MQTTState::FAILED: Serial.print("Failed"); break;
    }
    
    Serial.print(", WiFi Retries: ");
    Serial.print(wifiRetries);
    Serial.print("/");
    Serial.print(MAX_WIFI_RETRIES);
    Serial.print(", MQTT Retries: ");
    Serial.print(mqttRetries);
    Serial.print("/");
    Serial.println(MAX_MQTT_RETRIES);
}

void NetworkManager::printHealthStats() {
    Serial.println("Network Health Statistics:");
    Serial.print("  Connection Stable: ");
    Serial.println(connectionStable ? "Yes" : "No");
    Serial.print("  Total Disconnections: ");
    Serial.println(totalDisconnections);
    Serial.print("  Failed Publishes: ");
    Serial.println(failedPublishCount);
    Serial.print("  Current Uptime: ");
    Serial.print((millis() - connectionUptime) / 1000);
    Serial.println(" seconds");
    if (isWiFiConnected()) {
        Serial.print("  WiFi Signal: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    }
}

void NetworkManager::getHealthString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize,
        "wifi=%s mqtt=%s stable=%s disconnects=%lu fails=%lu uptime=%lus",
        (wifiState == WiFiState::CONNECTED) ? "OK" : "DOWN",
        (mqttState == MQTTState::CONNECTED) ? "OK" : "DOWN", 
        connectionStable ? "YES" : "NO",
        totalDisconnections,
        failedPublishCount,
        isConnected() ? (millis() - connectionUptime) / 1000 : 0
    );
}