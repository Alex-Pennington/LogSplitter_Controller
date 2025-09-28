#include "relay_controller.h"
#include "system_error_manager.h"

extern void debugPrintf(const char* fmt, ...);

void RelayController::begin() {
    // Initialize Serial1 for relay board communication
    Serial1.begin(RELAY_BAUD);
    delay(50);
    
    // Initialize all relay states to OFF
    for (int i = 0; i <= MAX_RELAYS; i++) {
        relayStates[i] = false;
    }
    
    // Power on the relay board (R9 OFF powers on the relay add-on board)
    sendCommand(RELAY_POWER_PIN, false);
    boardPowered = true;
    
    Serial.println("RelayController initialized");
}

void RelayController::sendCommand(uint8_t relayNumber, bool on) {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        Serial.print("Invalid relay number: ");
        Serial.println(relayNumber);
        return;
    }
    
    char command[16];
    snprintf(command, sizeof(command), "R%d %s", relayNumber, on ? "ON" : "OFF");
    
    Serial1.println(command);
    lastCommandTime = millis();
    waitingForResponse = true;
    
    debugPrintf("Relay cmd -> %s\n", command);
    
    // Update power state tracking
    if (relayNumber == RELAY_POWER_PIN) {
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
    debugPrintf("CRITICAL: Relay R%d command failed after %d retries\n", relayNumber, MAX_RETRIES);
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
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    response.trim();
                    debugPrintf("Relay response: '%s'\n", response.c_str());
                    
                    if (response.equalsIgnoreCase("OK")) {
                        waitingForResponse = false;
                        return true;
                    }
                    response = "";
                }
            } else if (c >= 32 && c <= 126) { // Printable ASCII
                response += c;
            }
        }
        delay(1); // Small delay to prevent busy waiting
    }
    
    waitingForResponse = false;
    return false; // Timeout
}

void RelayController::ensurePowerOn() {
    if (!boardPowered) {
        sendCommand(RELAY_POWER_PIN, false); // R9 OFF powers on the board
        boardPowered = true;
        delay(50); // Allow time for power stabilization
    }
}

void RelayController::update() {
    // Check for response timeout
    if (waitingForResponse && (millis() - lastCommandTime > RESPONSE_TIMEOUT_MS)) {
        debugPrintf("Relay response timeout\n");
        waitingForResponse = false;
    }
    
    // Handle serial echo from relay board (only when not waiting for response)
    if (echoEnabled && !waitingForResponse && Serial1.available()) {
        while (Serial1.available()) {
            int byte = Serial1.read();
            if (byte >= 0) {
                Serial.write((uint8_t)byte);
            }
        }
    }
}

void RelayController::setRelay(uint8_t relayNumber, bool on) {
    setRelay(relayNumber, on, false); // Default to automatic (sequence) operation
}

void RelayController::setRelay(uint8_t relayNumber, bool on, bool isManualCommand) {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        return;
    }
    
    // Check if state actually changed
    if (relayStates[relayNumber] == on) {
        return; // No change needed
    }
    
    // Safety check - block automatic operations when safety is active, but allow manual
    if (safetyActive && on && relayNumber != RELAY_POWER_PIN && !isManualCommand) {
        debugPrintf("Safety active - blocking automatic relay R%d ON (use manual override)\n", relayNumber);
        return;
    }
    
    // Log manual override during safety condition
    if (safetyActive && isManualCommand) {
        debugPrintf("Manual override: R%d %s during safety condition\n", relayNumber, on ? "ON" : "OFF");
    }
    
    // Ensure relay board is powered (except when controlling power relay itself)
    if (relayNumber != RELAY_POWER_PIN) {
        ensurePowerOn();
    }
    
    // Send command with retry and update state only on success
    if (sendCommandWithRetry(relayNumber, on)) {
        relayStates[relayNumber] = on;
    } else {
        // Command failed - don't update state
        debugPrintf("Failed to set relay R%d to %s - state not updated\n", relayNumber, on ? "ON" : "OFF");
        return;
    }
    
    debugPrintf("[RelayController] R%d -> %s %s\n", relayNumber, on ? "ON" : "OFF", 
        isManualCommand ? "(manual)" : "(auto)");
}

bool RelayController::getRelayState(uint8_t relayNumber) const {
    if (relayNumber < 1 || relayNumber > MAX_RELAYS) {
        return false;
    }
    return relayStates[relayNumber];
}

void RelayController::allRelaysOff() {
    debugPrintf("Turning off all relays\n");
    
    // Turn off relays 1-8 (keep power relay R9 as-is)
    for (uint8_t i = 1; i < RELAY_POWER_PIN; i++) {
        setRelay(i, false);
    }
}

void RelayController::powerOn() {
    sendCommand(RELAY_POWER_PIN, false); // R9 OFF = power on
    boardPowered = true;
    delay(50);
}

void RelayController::powerOff() {
    // Turn off all relays first
    allRelaysOff();
    delay(100);
    
    // Then power off the board
    sendCommand(RELAY_POWER_PIN, true); // R9 ON = power off
    boardPowered = false;
}

bool RelayController::processCommand(const char* relayToken, const char* stateToken) {
    if (!relayToken || !stateToken) {
        return false;
    }
    
    // Parse relay token (expect "R1" to "R9" or "r1" to "r9")
    if ((relayToken[0] != 'R' && relayToken[0] != 'r') || strlen(relayToken) < 2) {
        Serial.println("Invalid relay token format");
        return false;
    }
    
    int relayNum = atoi(relayToken + 1);
    if (relayNum < 1 || relayNum > MAX_RELAYS) {
        Serial.print("Invalid relay number: ");
        Serial.println(relayNum);
        return false;
    }
    
    // Parse state token
    bool on = (strcasecmp(stateToken, "ON") == 0);
    bool off = (strcasecmp(stateToken, "OFF") == 0);
    
    if (!on && !off) {
        Serial.print("Invalid relay state: ");
        Serial.println(stateToken);
        return false;
    }
    
    // Execute command as manual operation (allows override during safety)
    setRelay(relayNum, on, true); // Manual command flag set
    
    // Provide feedback
    char response[32];
    snprintf(response, sizeof(response), "relay %s %s", relayToken, on ? "ON" : "OFF");
    Serial.print("Relay command executed: ");
    Serial.println(response);
    
    return true;
}

void RelayController::getStatusString(char* buffer, size_t bufferSize) {
    int offset = 0;
    offset += snprintf(buffer + offset, bufferSize - offset, "relays:");
    
    for (uint8_t i = 1; i <= MAX_RELAYS && offset < (int)bufferSize - 10; i++) {
        offset += snprintf(buffer + offset, bufferSize - offset, 
            " R%d=%s", i, relayStates[i] ? "ON" : "OFF");
    }
    
    if (offset < (int)bufferSize - 20) {
        offset += snprintf(buffer + offset, bufferSize - offset, 
            " safety=%s", safetyActive ? "ACTIVE" : "OFF");
    }
}