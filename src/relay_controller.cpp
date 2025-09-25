#include "relay_controller.h"

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
    delay(10); // Small delay to allow board to process
    
    debugPrintf("Relay cmd -> %s\n", command);
    
    // Update power state tracking
    if (relayNumber == RELAY_POWER_PIN) {
        boardPowered = on;
    }
}

void RelayController::ensurePowerOn() {
    if (!boardPowered) {
        sendCommand(RELAY_POWER_PIN, false); // R9 OFF powers on the board
        boardPowered = true;
        delay(50); // Allow time for power stabilization
    }
}

void RelayController::update() {
    // Handle serial echo from relay board
    if (echoEnabled && Serial1.available()) {
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
    
    // Send command and update state
    sendCommand(relayNumber, on);
    relayStates[relayNumber] = on;
    
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