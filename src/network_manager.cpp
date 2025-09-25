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
    
    // Initial WiFi connection attempt
    return connectWiFi();
}

void NetworkManager::generateClientId() {
    uint8_t mac[6];
    if (WiFi.macAddress(mac)) {
        snprintf(clientId, sizeof(clientId), "r4-%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        snprintf(clientId, sizeof(clientId), "r4-unknown");
    }
}

bool NetworkManager::connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        return true;
    }
    
    Serial.print("Connecting to WiFi '");
    Serial.print(SECRET_SSID);
    Serial.println("'...");
    
    int status = WiFi.begin(SECRET_SSID, SECRET_PASS);
    unsigned long start = millis();
    
    while (status != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print('.');
        status = WiFi.status();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("WiFi connected, IP: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
        connectRetries = 0; // Reset retry count on success
        printWiFiInfo();
        return true;
    } else {
        Serial.println();
        Serial.println("WiFi connection failed");
        wifiConnected = false;
        printWiFiStatus();
        return false;
    }
}

bool NetworkManager::connectMQTT() {
    if (mqttClient.connected()) {
        mqttConnected = true;
        return true;
    }
    
    if (!wifiConnected) {
        return false;
    }
    
    Serial.print("Connecting to MQTT broker ");
    Serial.print(BROKER_HOST);
    Serial.print(":");
    Serial.println(BROKER_PORT);
    
    mqttClient.setId(clientId);
    
    #ifdef MQTT_USER
    if (strlen(MQTT_USER) > 0) {
        mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
    }
    #endif
    
    if (!mqttClient.connect(BROKER_HOST, BROKER_PORT)) {
        Serial.print("MQTT connect failed, error: ");
        Serial.println(mqttClient.connectError());
        mqttConnected = false;
        return false;
    }
    
    Serial.println("MQTT connected");
    mqttConnected = true;
    connectRetries = 0; // Reset retry count on success
    
    // Subscribe to control topics
    subscribe(TOPIC_SUBSCRIBE);
    subscribe(TOPIC_CONTROL);
    
    return true;
}

void NetworkManager::update() {
    unsigned long now = millis();
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        mqttConnected = false;
    }
    
    // Non-blocking reconnection with retry limits
    if (!wifiConnected || !mqttConnected) {
        if (now - lastConnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            if (connectRetries < MAX_CONNECT_RETRIES) {
                lastConnectAttempt = now;
                connectRetries++;
                
                Serial.print("Connection attempt ");
                Serial.print(connectRetries);
                Serial.print("/");
                Serial.println(MAX_CONNECT_RETRIES);
                
                if (!wifiConnected) {
                    connectWiFi();
                } else if (!mqttConnected) {
                    connectMQTT();
                }
            } else {
                // Max retries reached, wait longer before trying again
                if (now - lastConnectAttempt >= (MQTT_RECONNECT_INTERVAL_MS * 5)) {
                    Serial.println("Resetting connection retry count after extended wait");
                    connectRetries = 0;
                }
            }
        }
    }
    
    // Poll MQTT if connected
    if (mqttConnected) {
        mqttClient.poll();
    }
}

bool NetworkManager::publish(const char* topic, const char* payload) {
    if (!mqttConnected) {
        return false;
    }
    
    if (mqttClient.beginMessage(topic)) {
        mqttClient.print(payload);
        mqttClient.endMessage();
        return true;
    }
    
    return false;
}

bool NetworkManager::subscribe(const char* topic) {
    if (!mqttConnected) {
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
    Serial.print(wifiConnected ? "Connected" : "Disconnected");
    Serial.print(", MQTT: ");
    Serial.print(mqttConnected ? "Connected" : "Disconnected");
    Serial.print(", Retries: ");
    Serial.print(connectRetries);
    Serial.print("/");
    Serial.println(MAX_CONNECT_RETRIES);
}