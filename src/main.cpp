/*
 * LogSplitter Mesh Test Sketch
 * 
 * Dedicated Arduino sketch for testing mesh transport functionality
 * Sends realistic telemetry data via A2/A3 pins to Meshtastic node
 * 
 * Hardware:
 * - Arduino UNO R4 WiFi
 * - A2 pin → Meshtastic Controller Node RX
 * - A3 pin ← Meshtastic Controller Node TX (optional)
 * - USB Serial for debug output (115200 baud)
 * 
 * Data Flow:
 * Arduino Serial (COM7) → Debug messages
 * Arduino A2/A3 → Controller Node (COM13) → Mesh → Diagnostic Node (COM8) → LCARS
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

// Pin Configuration
const uint8_t MESHTASTIC_TX_PIN = A2;  // Arduino A2 → Meshtastic RX
const uint8_t MESHTASTIC_RX_PIN = A3;  // Arduino A3 ← Meshtastic TX  
const unsigned long MESHTASTIC_BAUD = 115200;

// Timing Configuration
const unsigned long TELEMETRY_INTERVAL_MS = 1000;  // 1Hz telemetry
const unsigned long HEARTBEAT_INTERVAL_MS = 10000; // 10s heartbeat
const unsigned long SEQUENCE_DURATION_MS = 30000;  // 30s hydraulic cycle

// Protobuf Message Types (from telemetry.proto)
const uint8_t MSG_TELEMETRY_DIGITAL_IO = 0x10;
const uint8_t MSG_TELEMETRY_RELAYS = 0x11;
const uint8_t MSG_TELEMETRY_PRESSURE = 0x12;
const uint8_t MSG_TELEMETRY_SEQUENCE = 0x13;
const uint8_t MSG_TELEMETRY_SAFETY = 0x14;
const uint8_t MSG_TELEMETRY_SYSTEM = 0x15;

// Global Objects
SoftwareSerial meshSerial(MESHTASTIC_RX_PIN, MESHTASTIC_TX_PIN);

// Timing Variables
unsigned long lastTelemetryTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long sequenceStartTime = 0;
uint32_t messageCounter = 0;

// Mock Telemetry Data
struct MockData {
    // Digital I/O State
    bool manualExtend = false;
    bool manualRetract = false;
    bool limitExtend = false;
    bool limitRetract = true;  // Start retracted
    bool sequenceStart = false;
    bool emergencyStop = false;
    
    // Relay State  
    bool relayExtend = false;
    bool relayRetract = false;
    bool engineStop = false;
    
    // Pressure Readings (PSI)
    float mainPressure = 45.0;    // Idle pressure
    float auxPressure = 15.0;     // Low pressure system
    
    // Sequence State
    uint8_t currentState = 0;     // 0=IDLE, 1=EXTENDING, 2=EXTENDED, 3=RETRACTING
    uint32_t sequenceTime = 0;    // Time in current sequence
    
    // System Metrics
    uint32_t uptime = 0;
    uint16_t cycleCount = 0;
    uint8_t errorCount = 0;
} mockData;

// Function Declarations
void updateMockData(unsigned long now);
void sendTelemetrySet();
void sendDigitalIOTelemetry(uint32_t timestamp, uint8_t sequenceId);
void sendRelayTelemetry(uint32_t timestamp, uint8_t sequenceId);
void sendPressureTelemetry(uint32_t timestamp, uint8_t sequenceId);
void sendSequenceTelemetry(uint32_t timestamp, uint8_t sequenceId);
void sendHeartbeat();
void handleSerialCommands();

void setup() {
    // Initialize Serial for debug output
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    
    Serial.println();
    Serial.println("🚀 LOGSPLITTER MESH TEST SKETCH v1.0");
    Serial.println("=====================================");
    Serial.println("Purpose: Test mesh transport via A2/A3 pins");
    Serial.println();
    
    // Initialize Meshtastic communication
    meshSerial.begin(MESHTASTIC_BAUD);
    Serial.print("📡 Mesh Serial initialized: A2/A3 @ ");
    Serial.print(MESHTASTIC_BAUD);
    Serial.println(" baud");
    
    // Initialize timing
    sequenceStartTime = millis();
    lastTelemetryTime = millis();
    lastHeartbeatTime = millis();
    
    Serial.println("✅ Initialization complete");
    Serial.println("🎯 Starting telemetry generation...");
    Serial.println();
    
    // Send initial status
    sendHeartbeat();
}

void loop() {
    unsigned long now = millis();
    
    // Update mock telemetry data
    updateMockData(now);
    
    // Send telemetry at regular intervals
    if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryTime = now;
        sendTelemetrySet();
    }
    
    // Send heartbeat periodically  
    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        sendHeartbeat();
    }
    
    // Handle any incoming serial commands
    handleSerialCommands();
    
    delay(10);  // Small delay for stability
}

void updateMockData(unsigned long now) {
    mockData.uptime = now;
    
    // Simulate 30-second hydraulic cycle
    unsigned long cycleTime = (now - sequenceStartTime) % SEQUENCE_DURATION_MS;
    
    if (cycleTime < 2000) {
        // Idle phase (0-2s)
        mockData.currentState = 0;
        mockData.relayExtend = false;
        mockData.relayRetract = false;
        mockData.limitExtend = false;
        mockData.limitRetract = true;
        mockData.mainPressure = 45.0 + random(-5, 6);
        
    } else if (cycleTime < 12000) {
        // Extending phase (2-12s)
        mockData.currentState = 1;
        mockData.relayExtend = true;
        mockData.relayRetract = false;
        mockData.limitExtend = false;
        mockData.limitRetract = false;
        mockData.mainPressure = 1800.0 + random(-50, 51);
        
    } else if (cycleTime < 15000) {
        // Extended hold (12-15s)
        mockData.currentState = 2;
        mockData.relayExtend = false;
        mockData.relayRetract = false;
        mockData.limitExtend = true;
        mockData.limitRetract = false;
        mockData.mainPressure = 2200.0 + random(-100, 101);
        
    } else if (cycleTime < 25000) {
        // Retracting phase (15-25s)
        mockData.currentState = 3;
        mockData.relayExtend = false;
        mockData.relayRetract = true;
        mockData.limitExtend = false;
        mockData.limitRetract = false;
        mockData.mainPressure = 1200.0 + random(-50, 51);
        
    } else {
        // Return to idle (25-30s)
        mockData.currentState = 0;
        mockData.relayExtend = false;
        mockData.relayRetract = false;
        mockData.limitExtend = false;
        mockData.limitRetract = true;
        mockData.mainPressure = 45.0 + random(-5, 6);
    }
    
    mockData.sequenceTime = cycleTime;
    mockData.auxPressure = 12.0 + random(-2, 3);
}

void sendTelemetrySet() {
    uint32_t timestamp = millis();
    uint8_t sequenceId = (messageCounter++) % 256;
    
    // Debug output to Serial
    const char* stateNames[] = {"IDLE", "EXTENDING", "EXTENDED", "RETRACTING"};
    Serial.print("📡 [");
    Serial.print(messageCounter);
    Serial.print("] ");
    Serial.print(stateNames[mockData.currentState]);
    Serial.print(" P:");
    Serial.print(mockData.mainPressure, 0);
    Serial.print("psi E:");
    Serial.print(mockData.relayExtend ? "ON" : "OFF");
    Serial.print(" R:");
    Serial.print(mockData.relayRetract ? "ON" : "OFF");
    Serial.print(" → A2");
    Serial.println();
    
    // Send Digital I/O Telemetry
    sendDigitalIOTelemetry(timestamp, sequenceId);
    
    // Send Relay Telemetry
    sendRelayTelemetry(timestamp, sequenceId);
    
    // Send Pressure Telemetry
    sendPressureTelemetry(timestamp, sequenceId);
    
    // Send Sequence Status (less frequently)
    if (messageCounter % 5 == 0) {
        sendSequenceTelemetry(timestamp, sequenceId);
    }
}

void sendDigitalIOTelemetry(uint32_t timestamp, uint8_t sequenceId) {
    uint8_t data[12];
    data[0] = MSG_TELEMETRY_DIGITAL_IO;
    data[1] = sequenceId;
    *((uint32_t*)(data + 2)) = timestamp;
    data[6] = (mockData.manualExtend ? 0x01 : 0) |
              (mockData.manualRetract ? 0x02 : 0) |
              (mockData.limitExtend ? 0x04 : 0) |
              (mockData.limitRetract ? 0x08 : 0) |
              (mockData.sequenceStart ? 0x10 : 0) |
              (mockData.emergencyStop ? 0x80 : 0);
    data[7] = 0x00;  // Reserved
    *((uint32_t*)(data + 8)) = mockData.sequenceTime;
    
    meshSerial.write(data, sizeof(data));
}

void sendRelayTelemetry(uint32_t timestamp, uint8_t sequenceId) {
    uint8_t data[10];
    data[0] = MSG_TELEMETRY_RELAYS;
    data[1] = sequenceId;
    *((uint32_t*)(data + 2)) = timestamp;
    data[6] = (mockData.relayExtend ? 0x01 : 0) |
              (mockData.relayRetract ? 0x02 : 0) |
              (mockData.engineStop ? 0x80 : 0);
    data[7] = 0x03;  // relay_mask (R1, R2 monitored)
    data[8] = 0x00;  // Reserved
    data[9] = 0x00;  // Reserved
    
    meshSerial.write(data, sizeof(data));
}

void sendPressureTelemetry(uint32_t timestamp, uint8_t sequenceId) {
    uint8_t data[16];
    data[0] = MSG_TELEMETRY_PRESSURE;
    data[1] = sequenceId;
    *((uint32_t*)(data + 2)) = timestamp;
    *((float*)(data + 6)) = mockData.mainPressure;
    *((float*)(data + 10)) = mockData.auxPressure;
    data[14] = 0x03;  // sensor_mask (both sensors active)
    data[15] = 0x00;  // Reserved
    
    meshSerial.write(data, sizeof(data));
}

void sendSequenceTelemetry(uint32_t timestamp, uint8_t sequenceId) {
    uint8_t data[14];
    data[0] = MSG_TELEMETRY_SEQUENCE;
    data[1] = sequenceId;
    *((uint32_t*)(data + 2)) = timestamp;
    data[6] = mockData.currentState;
    data[7] = 0x00;  // Reserved
    *((uint32_t*)(data + 8)) = mockData.sequenceTime;
    *((uint16_t*)(data + 12)) = mockData.cycleCount;
    
    meshSerial.write(data, sizeof(data));
}

void sendHeartbeat() {
    Serial.print("💓 Heartbeat - Uptime: ");
    Serial.print(mockData.uptime / 1000);
    Serial.print("s, Messages: ");
    Serial.print(messageCounter);
    Serial.print(", Cycles: ");
    Serial.println(mockData.cycleCount);
    
    // Send system status telemetry
    uint32_t timestamp = millis();
    uint8_t sequenceId = (messageCounter++) % 256;
    
    uint8_t data[18];
    data[0] = MSG_TELEMETRY_SYSTEM;
    data[1] = sequenceId;
    *((uint32_t*)(data + 2)) = timestamp;
    *((uint32_t*)(data + 6)) = mockData.uptime;
    *((uint16_t*)(data + 10)) = mockData.cycleCount;
    data[12] = mockData.errorCount;
    data[13] = 0x01;  // system_state (running)
    *((float*)(data + 14)) = 3.3;  // mock voltage
    
    meshSerial.write(data, sizeof(data));
}

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "help") {
            Serial.println("📋 MESH TEST COMMANDS:");
            Serial.println("=== SYSTEM CONTROL ===");
            Serial.println("  help      - Show this help");
            Serial.println("  status    - Show current status");  
            Serial.println("  ping      - Test Arduino responsiveness");
            Serial.println("  reboot    - Restart Arduino (software reset)");
            Serial.println();
            Serial.println("=== TELEMETRY CONTROL ===");
            Serial.println("  send      - Send immediate telemetry set");
            Serial.println("  burst 10  - Send 10 rapid telemetry messages");
            Serial.println("  heartbeat - Send immediate heartbeat");
            Serial.println("  stop      - Stop automatic telemetry");
            Serial.println("  start     - Start automatic telemetry");
            Serial.println();
            Serial.println("=== SEQUENCE CONTROL ===");
            Serial.println("  idle      - Force to IDLE state");
            Serial.println("  extend    - Force to EXTENDING state"); 
            Serial.println("  retract   - Force to RETRACTING state");
            Serial.println("  cycle     - Complete cycle and restart");
            Serial.println("  reset     - Reset sequence timer");
            Serial.println();
            Serial.println("=== TESTING ===");
            Serial.println("  pressure <psi> - Set main pressure (e.g. 'pressure 1500')");
            Serial.println("  relay <on|off> - Set extend relay state");
            Serial.println("  pins      - Show current pin states");
            Serial.println("  mesh      - Test mesh serial output");
            
        } else if (command == "ping") {
            Serial.println("🏓 PONG! Arduino is responsive");
            Serial.print("   Uptime: ");
            Serial.print(millis() / 1000);
            Serial.println(" seconds");
            
        } else if (command == "status") {
            Serial.println("📊 CURRENT STATUS:");
            Serial.print("  Uptime: ");
            Serial.print(mockData.uptime / 1000);
            Serial.println("s");
            Serial.print("  Messages sent: ");
            Serial.println(messageCounter);
            Serial.print("  Current state: ");
            const char* stateNames[] = {"IDLE", "EXTENDING", "EXTENDED", "RETRACTING"};
            Serial.println(stateNames[mockData.currentState]);
            Serial.print("  Main pressure: ");
            Serial.print(mockData.mainPressure, 1);
            Serial.println(" PSI");
            Serial.print("  Relays: Extend=");
            Serial.print(mockData.relayExtend ? "ON" : "OFF");
            Serial.print(" Retract=");
            Serial.println(mockData.relayRetract ? "ON" : "OFF");
            Serial.print("  Limits: Extend=");
            Serial.print(mockData.limitExtend ? "ACTIVE" : "INACTIVE");
            Serial.print(" Retract=");
            Serial.println(mockData.limitRetract ? "ACTIVE" : "INACTIVE");
            
        } else if (command == "send") {
            Serial.println("📡 Sending immediate telemetry set...");
            sendTelemetrySet();
            Serial.println("✅ Telemetry sent");
            
        } else if (command.startsWith("burst ")) {
            int count = command.substring(6).toInt();
            if (count > 0 && count <= 100) {
                Serial.print("🚀 Sending ");
                Serial.print(count);
                Serial.println(" burst telemetry messages...");
                
                for (int i = 0; i < count; i++) {
                    sendTelemetrySet();
                    delay(50); // Small delay between messages
                    Serial.print(".");
                }
                Serial.println();
                Serial.println("✅ Burst complete");
            } else {
                Serial.println("❌ Invalid count (1-100)");
            }
            
        } else if (command == "heartbeat") {
            Serial.println("💓 Sending immediate heartbeat...");
            sendHeartbeat();
            Serial.println("✅ Heartbeat sent");
            
        } else if (command == "idle") {
            mockData.currentState = 0;
            mockData.relayExtend = false;
            mockData.relayRetract = false;
            mockData.limitExtend = false;
            mockData.limitRetract = true;
            mockData.mainPressure = 45.0;
            Serial.println("⏸️ Forced to IDLE state");
            
        } else if (command == "extend") {
            mockData.currentState = 1;
            mockData.relayExtend = true;
            mockData.relayRetract = false;
            mockData.limitExtend = false;
            mockData.limitRetract = false;
            mockData.mainPressure = 1800.0;
            Serial.println("⬆️ Forced to EXTENDING state");
            
        } else if (command == "retract") {
            mockData.currentState = 3;
            mockData.relayExtend = false;
            mockData.relayRetract = true;
            mockData.limitExtend = false;
            mockData.limitRetract = false;
            mockData.mainPressure = 1200.0;
            Serial.println("⬇️ Forced to RETRACTING state");
            
        } else if (command == "reset") {
            sequenceStartTime = millis();
            Serial.println("🔄 Sequence timer reset");
            
        } else if (command == "cycle") {
            mockData.cycleCount++;
            sequenceStartTime = millis();
            Serial.println("⏭️ Cycle completed, starting new sequence");
            
        } else if (command.startsWith("pressure ")) {
            float newPressure = command.substring(9).toFloat();
            if (newPressure >= 0 && newPressure <= 3000) {
                mockData.mainPressure = newPressure;
                Serial.print("🔧 Main pressure set to ");
                Serial.print(newPressure, 1);
                Serial.println(" PSI");
            } else {
                Serial.println("❌ Invalid pressure (0-3000 PSI)");
            }
            
        } else if (command.startsWith("relay ")) {
            String relayState = command.substring(6);
            if (relayState == "on") {
                mockData.relayExtend = true;
                Serial.println("🔧 Extend relay ON");
            } else if (relayState == "off") {
                mockData.relayExtend = false;
                Serial.println("🔧 Extend relay OFF");
            } else {
                Serial.println("❌ Use 'relay on' or 'relay off'");
            }
            
        } else if (command == "pins") {
            Serial.println("📌 PIN STATES:");
            Serial.print("  A2 (Mesh TX): ");
            Serial.println("OUTPUT");
            Serial.print("  A3 (Mesh RX): ");
            Serial.println("INPUT");
            Serial.println("  Digital I/O simulation:");
            Serial.print("    Manual Extend: ");
            Serial.println(mockData.manualExtend ? "ACTIVE" : "INACTIVE");
            Serial.print("    Manual Retract: ");
            Serial.println(mockData.manualRetract ? "ACTIVE" : "INACTIVE");
            Serial.print("    Limit Extend: ");
            Serial.println(mockData.limitExtend ? "ACTIVE" : "INACTIVE");
            Serial.print("    Limit Retract: ");
            Serial.println(mockData.limitRetract ? "ACTIVE" : "INACTIVE");
            
        } else if (command == "mesh") {
            Serial.println("🌐 Testing mesh serial output...");
            Serial.println("   Sending test data to A2 pin...");
            
            // Send a test message to mesh serial
            uint8_t testData[] = {0xFF, 0xAA, 0x55, 0x00, 0x11, 0x22, 0x33, 0x44};
            meshSerial.write(testData, sizeof(testData));
            
            Serial.print("   Test data sent: ");
            for (int i = 0; i < sizeof(testData); i++) {
                Serial.print("0x");
                Serial.print(testData[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            Serial.println("✅ Mesh test complete");
            
        } else if (command == "reboot") {
            Serial.println("🔄 Rebooting Arduino in 3 seconds...");
            Serial.println("   3...");
            delay(1000);
            Serial.println("   2...");
            delay(1000);
            Serial.println("   1...");
            delay(1000);
            Serial.println("   Restarting...");
            delay(100);
            // Software reset for ARM Cortex-M4
            NVIC_SystemReset();
            
        } else if (command == "stop") {
            // Implementation depends on adding a global telemetry enable flag
            Serial.println("⏹️ Telemetry stopped (not implemented yet)");
            
        } else if (command == "start") {
            // Implementation depends on adding a global telemetry enable flag  
            Serial.println("▶️ Telemetry started (not implemented yet)");
            
        } else if (command.length() > 0) {
            Serial.print("❓ Unknown command: '");
            Serial.print(command);
            Serial.println("'");
            Serial.println("💡 Type 'help' for available commands");
        }
    }
}