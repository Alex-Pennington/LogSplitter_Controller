#include "relay_controller.h"
#include "system_error_manager.h"
#include "network_manager.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);
extern NetworkManager* g_networkManager;

void RelayController::begin() {
    // Initialize Serial1 for relay board communication
    Serial1.begin(RELAY_BAUD);
    delay(50);
    
    // Initialize all relay states to OFF
    for (int i = 0; i <= MAX_RELAYS; i++) {
        relayStates[i] = false;
    }
    
    // Power on the relay board (R9 OFF powers on the relay add-on board)
    sendCommand(RELAY_POWER_CONTROL, false);
    boardPowered = true;
    
    LOG_INFO("RelayController initialized");
}

void RelayController::sendCommand(uint8_t relayNumber, bool on) {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        debugPrintf("Invalid relay number: %d\n", relayNumber);
        return;
    }
    
    char command[16];
    snprintf(command, sizeof(command), "R%d %s", relayNumber, on ? "ON" : "OFF");
    
    Serial1.println(command);
    lastCommandTime = millis();
    waitingForResponse = true;
    
    debugPrintf("Relay cmd -> %s\n", command);
    
    // Update power state tracking
    if (relayNumber == RELAY_POWER_CONTROL) {
        boardPowered = on;
    }
}

bool RelayController::sendCommandWithRetry(uint8_t relayNumber, bool on) {
    retryCount = 0;
    lastRelayNumber = relayNumber;
    lastRelayState = on;
    
    while (retryCount <= MAX_RETRIES) {
        sendCommand(relayNumber, on);
        
        if (waitForOkResponse()) {
            return true; // Success
        }
        
        retryCount++;
        if (retryCount <= MAX_RETRIES) {
            debugPrintf("Relay R%d command failed, retry %d/%d\n", relayNumber, retryCount, MAX_RETRIES);
            delay(100); // Brief delay before retry
        }
    }
    
    // All retries failed - report critical error
    LOG_CRITICAL("Relay R%d command failed after %d retries", relayNumber, MAX_RETRIES);
    if (errorManager) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Relay R%d communication timeout", relayNumber);
        errorManager->setError(ERROR_HARDWARE_FAULT, errorMsg);
    }
    
    return false;
}

bool RelayController::waitForOkResponse() {
    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < RESPONSE_TIMEOUT_MS) {
        if (Serial1.available()) {
            char c = Serial1.read();
            if (c == '\n') {
                response.trim();
                if (response.length() > 0) {
                    if (response.indexOf("OK") >= 0) {
                        debugPrintf("Relay response: '%s'\n", response.c_str());
                        waitingForResponse = false;
                        return true;
                    }
                }
                response = "";
            } else if (c != '\r') {
                response += c;
            }
        }
    }
    
    waitingForResponse = false;
    debugPrintf("Relay response timeout (100ms)\n");
    return false;
}

void RelayController::ensurePowerOn() {
    if (!boardPowered) {
        sendCommand(RELAY_POWER_CONTROL, false); // R9 OFF powers on the board
        boardPowered = true;
        delay(100); // Give board time to power up
    }
}

void RelayController::update() {
    // Check for timeouts
    if (waitingForResponse && (millis() - lastCommandTime > RESPONSE_TIMEOUT_MS)) {
        debugPrintf("Relay response timeout (100ms)\n");
        waitingForResponse = false;
    }
    
    // Echo any unexpected relay board responses for debugging
    if (echoEnabled && !waitingForResponse && Serial1.available()) {
        Serial.print("Relay echo: ");
        while (Serial1.available()) {
            Serial.write(Serial1.read());
        }
        Serial.println();
    }
}

void RelayController::setRelay(uint8_t relayNumber, bool on) {
    setRelay(relayNumber, on, false); // Default to automatic (sequence) operation
}

void RelayController::setRelay(uint8_t relayNumber, bool on, bool isManualCommand) {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        debugPrintf("Invalid relay number: %d\n", relayNumber);
        return;
    }
    
    // Check if already in desired state
    if (relayStates[relayNumber] == on) {
        return; // Already in desired state
    }
    
    // Safety check: prevent automatic relay activation during safety mode
    if (safetyActive && on && relayNumber != RELAY_POWER_CONTROL && !isManualCommand) {
        debugPrintf("Safety active - blocking automatic relay R%d ON (use manual override)\n", relayNumber);
        return;
    }
    
    // Log manual override during safety condition
    if (safetyActive && isManualCommand) {
        debugPrintf("Manual override: R%d %s during safety condition\n", relayNumber, on ? "ON" : "OFF");
    }
    
    // Ensure power is on before sending commands (except for power relay itself)
    if (relayNumber != RELAY_POWER_CONTROL) {
        ensurePowerOn();
    }
    
    // Send command with retries
    if (sendCommandWithRetry(relayNumber, on)) {
        relayStates[relayNumber] = on;
        debugPrintf("[RelayController] R%d -> %s %s\n", relayNumber, on ? "ON" : "OFF", 
                   isManualCommand ? "(manual)" : "(auto)");
    } else {
        debugPrintf("Failed to set relay R%d to %s - state not updated\n", relayNumber, on ? "ON" : "OFF");
    }
}

bool RelayController::getRelayState(uint8_t relayNumber) const {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        return false;
    }
    return relayStates[relayNumber];
}

void RelayController::allRelaysOff() {
    debugPrintf("Turning off all relays\n");
    
    // Turn off all relays except power relay (R9)
    for (uint8_t i = 1; i <= MAX_RELAYS; i++) {
        if (i != RELAY_POWER_CONTROL) {
            setRelay(i, false);
        }
    }
}

void RelayController::powerOn() {
    setRelay(RELAY_POWER_CONTROL, false); // R9 OFF = power ON (inverted logic)
    boardPowered = true;
}

void RelayController::powerOff() {
    // Turn off all relays first
    for (uint8_t i = 1; i <= MAX_RELAYS; i++) {
        if (i != RELAY_POWER_CONTROL) {
            setRelay(i, false);
        }
    }
    setRelay(RELAY_POWER_CONTROL, true); // R9 ON = power OFF (inverted logic)
    boardPowered = false;
}

bool RelayController::processCommand(const char* relayToken, const char* stateToken) {
    if (!relayToken || !stateToken) {
        debugPrintf("Invalid relay token format\n");
        return false;
    }
    
    // Parse relay number (expect format like "R1", "R2", etc.)
    int relayNum = atoi(relayToken + 1); // Skip 'R' prefix
    if (relayNum < 1 || relayNum > MAX_RELAYS) {
        debugPrintf("Invalid relay number: %d\n", relayNum);
        return false;
    }
    
    // Parse state
    bool on;
    if (strcasecmp(stateToken, "ON") == 0 || strcasecmp(stateToken, "1") == 0) {
        on = true;
    } else if (strcasecmp(stateToken, "OFF") == 0 || strcasecmp(stateToken, "0") == 0) {
        on = false;
    } else {
        debugPrintf("Invalid relay state: %s\n", stateToken);
        return false;
    }
    
    // Execute command (manual command = true for command interface)
    setRelay(relayNum, on, true);
    
    char response[64];
    snprintf(response, sizeof(response), "relay R%d %s", relayNum, on ? "ON" : "OFF");
    debugPrintf("Relay command executed: %s\n", response);
    
    return true;
}

void RelayController::getStatusString(char* buffer, size_t bufferSize) {
    int offset = snprintf(buffer, bufferSize, "relays:");
    
    for (int i = 1; i <= MAX_RELAYS && offset < bufferSize - 10; i++) {
        offset += snprintf(buffer + offset, bufferSize - offset, 
            " R%d=%s", i, relayStates[i] ? "ON" : "OFF");
    }
    
    // Add safety status
    offset += snprintf(buffer + offset, bufferSize - offset, 
        " safety=%s", safetyActive ? "ACTIVE" : "OFF");
}

void RelayController::publishIndividualValues() {
    if (!g_networkManager || !g_networkManager->isConnected()) {
        return;
    }
    
    char topic[64];
    char value[16];
    
    // Publish individual relay states
    for (int i = 1; i <= MAX_RELAYS; i++) {
        snprintf(topic, sizeof(topic), "controller/relays/%d/state", i);
        snprintf(value, sizeof(value), "%d", relayStates[i] ? 1 : 0);
        g_networkManager->publish(topic, value);
        
        snprintf(topic, sizeof(topic), "controller/relays/%d/active", i);
        snprintf(value, sizeof(value), "%s", relayStates[i] ? "true" : "false");
        g_networkManager->publish(topic, value);
    }
    
    // Publish board power status
    g_networkManager->publish("controller/relays/board_powered", boardPowered ? "true" : "false");
    
    // Publish safety mode status
    g_networkManager->publish("controller/relays/safety_mode", safetyActive ? "true" : "false");
}