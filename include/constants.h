#pragma once

#include <Arduino.h>

// System Constants
const unsigned long WATCHDOG_TIMEOUT_MS = 15000;
const unsigned long MAIN_LOOP_TIMEOUT_MS = 10000;

// Network Constants - Default fallback values (configure in arduino_secrets.h)
const char* const BROKER_HOST = "192.168.1.155";  // Default MQTT broker (override in secrets)
const int BROKER_PORT = 1883;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_CONNECT_CHECK_INTERVAL_MS = 500;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long NETWORK_STABILITY_TIME_MS = 10000;
const uint8_t MAX_CONNECT_RETRIES = 3;
const uint8_t MAX_WIFI_RETRIES = 3;
const uint8_t MAX_MQTT_RETRIES = 3;

// Syslog Constants - Default fallback values (configure in arduino_secrets.h)
const char* const SYSLOG_SERVER = "192.168.1.155";  // Default syslog server (override in secrets)
const int SYSLOG_PORT = 514;                        // Standard syslog UDP port
const char* const SYSLOG_HOSTNAME = "LogSplitter";  // Hostname for syslog messages
const char* const SYSLOG_TAG = "logsplitter";       // Application tag for syslog
const int SYSLOG_FACILITY = 16;                     // Local use facility (local0 = 16)
const int SYSLOG_SEVERITY = 6;                      // Info level (0=emergency, 6=info, 7=debug)

// MQTT Topics - Hierarchical structure starting with /controller
const char TOPIC_CONTROL[] PROGMEM = "controller/control";  // Command input - kept as is
const char TOPIC_CONTROL_RESP[] PROGMEM = "controller/control/resp";  // Command responses - kept as is

// New hierarchical topic structure under /controller
const char TOPIC_PRESSURE[] PROGMEM = "controller/pressure";
const char TOPIC_HYDRAULIC_SYSTEM_PRESSURE[] PROGMEM = "controller/pressure/hydraulic_system";
const char TOPIC_HYDRAULIC_FILTER_PRESSURE[] PROGMEM = "controller/pressure/hydraulic_filter";
const char TOPIC_HYDRAULIC_SYSTEM_VOLTAGE[] PROGMEM = "controller/pressure/hydraulic_system_voltage";
const char TOPIC_HYDRAULIC_FILTER_VOLTAGE[] PROGMEM = "controller/pressure/hydraulic_filter_voltage";

// Safety system individual values
const char TOPIC_SAFETY_ACTIVE[] PROGMEM = "controller/safety/active";
const char TOPIC_SAFETY_ESTOP[] PROGMEM = "controller/safety/estop";
const char TOPIC_SAFETY_ENGINE[] PROGMEM = "controller/safety/engine";
const char TOPIC_SAFETY_PRESSURE_CURRENT[] PROGMEM = "controller/safety/pressure_current";
const char TOPIC_SAFETY_PRESSURE_THRESHOLD[] PROGMEM = "controller/safety/pressure_threshold";
const char TOPIC_SAFETY_HIGH_PRESSURE_ACTIVE[] PROGMEM = "controller/safety/high_pressure_active";
const char TOPIC_SAFETY_HIGH_PRESSURE_ELAPSED[] PROGMEM = "controller/safety/high_pressure_elapsed";

// Sequence controller individual values
const char TOPIC_SEQUENCE_ACTIVE[] PROGMEM = "controller/sequence/active";
const char TOPIC_SEQUENCE_STAGE[] PROGMEM = "controller/sequence/stage";
const char TOPIC_SEQUENCE_ELAPSED[] PROGMEM = "controller/sequence/elapsed";
const char TOPIC_SEQUENCE_DISABLED[] PROGMEM = "controller/sequence/disabled";
const char TOPIC_SEQUENCE_EVENT[] PROGMEM = "controller/sequence/event";
const char TOPIC_SEQUENCE_STATE[] PROGMEM = "controller/sequence/state";

// System status individual values
const char TOPIC_SYSTEM_UPTIME[] PROGMEM = "controller/system/uptime";
const char TOPIC_SYSTEM_MEMORY_FREE[] PROGMEM = "controller/system/memory_free";
const char TOPIC_SYSTEM_NETWORK_CONNECTED[] PROGMEM = "controller/system/network_connected";

// Input monitoring individual values
const char TOPIC_INPUTS_PIN2[] PROGMEM = "controller/inputs/pin2";
const char TOPIC_INPUTS_PIN3[] PROGMEM = "controller/inputs/pin3";
const char TOPIC_INPUTS_PIN4[] PROGMEM = "controller/inputs/pin4";
const char TOPIC_INPUTS_PIN5[] PROGMEM = "controller/inputs/pin5";
const char TOPIC_INPUTS_PIN6[] PROGMEM = "controller/inputs/pin6";
const char TOPIC_INPUTS_PIN7[] PROGMEM = "controller/inputs/pin7";
const char TOPIC_INPUTS_PIN12[] PROGMEM = "controller/inputs/pin12";

// Relay status individual values
const char TOPIC_RELAYS_R1[] PROGMEM = "controller/relays/r1";
const char TOPIC_RELAYS_R2[] PROGMEM = "controller/relays/r2";
const char TOPIC_RELAYS_R8[] PROGMEM = "controller/relays/r8";
const char TOPIC_RELAYS_R9[] PROGMEM = "controller/relays/r9";


// Pin Configuration
const uint8_t WATCH_PINS[] = {2, 3, 4, 5, 6, 7, 8, 12};
const size_t WATCH_PIN_COUNT = sizeof(WATCH_PINS) / sizeof(WATCH_PINS[0]);
const unsigned long DEBOUNCE_DELAY_MS = 5;  // Reduced for fast-moving hydraulic cylinder limit switches

// Specific Pin Definitions
const uint8_t E_STOP_PIN = 12;
const uint8_t SAFETY_CLEAR_PIN = 4;
const uint8_t LIMIT_EXTEND_PIN = 6;
const uint8_t LIMIT_RETRACT_PIN = 7;

// Pin-specific debounce delays (milliseconds)
const unsigned long LIMIT_SWITCH_DEBOUNCE_MS = 10;  // Pins 6,7 - Balanced for moving cylinder with switch bounce filtering
const unsigned long BUTTON_DEBOUNCE_MS = 15;        // Pins 2,3,4,5 - Normal for manual buttons

// Relay Configuration Labels
const uint8_t RELAY_EXTEND = 1;       // Relay 1 - Cylinder extend (hydraulic valve)
const uint8_t RELAY_RETRACT = 2;      // Relay 2 - Cylinder retract (hydraulic valve)
const uint8_t RELAY_ENGINE_STOP = 8;  // Relay 8 - Engine stop relay (safety)
const uint8_t RELAY_POWER_CONTROL = 9; // Relay 9 - Relay board power control

// Safety System Pin Configuration
const uint8_t MILL_LAMP_PIN = 9; // Pin 9 - Mill Lamp (Yellow) output
const uint8_t SAFETY_STATUS_PIN = 11; // Pin 11 - Safety System Status output

// Relay Configuration
const unsigned long RELAY_BAUD = 115200;
const uint8_t MAX_RELAYS = 9;

// Pressure Sensor Configuration
const unsigned long SAMPLE_INTERVAL_MS = 100;
const unsigned long SAMPLE_WINDOW_MS = 1000;
const size_t SAMPLE_WINDOW_COUNT = SAMPLE_WINDOW_MS / SAMPLE_INTERVAL_MS;
const uint8_t ADC_RESOLUTION_BITS = 10;  // Arduino UNO R4 WiFi has 10-bit ADC (1024 counts)
const float DEFAULT_ADC_VREF = 5.0f;     // Arduino UNO R4 WiFi uses 5V reference voltage
const float DEFAULT_MAX_PRESSURE_PSI = 3000.0f;
const float DEFAULT_SENSOR_GAIN = 1.0f;
const float DEFAULT_SENSOR_OFFSET = 0.0f;

// Dual Pressure Sensor Pins
const uint8_t HYDRAULIC_PRESSURE_PIN = A1;      // Main hydraulic pressure (A1)
const uint8_t HYDRAULIC_OIL_PRESSURE_PIN = A5;  // Hydraulic oil pressure (A5)

// Pressure Sensor Specifications (0-4.5V sensors)
const float SENSOR_MIN_VOLTAGE = 0.0f;    // 0V = 0 PSI
const float SENSOR_MAX_VOLTAGE = 4.5f;    // 4.5V = max PSI
const float HYDRAULIC_MAX_PRESSURE_PSI = 3000.0f; // Hydraulic system pressure range

// Individual sensor defaults (A1 - Main Hydraulic)  
const float DEFAULT_A1_MAX_PRESSURE_PSI = 5000.0f;  // Change this to your sensor's max range
const float DEFAULT_A1_ADC_VREF = 5.0f;  // Corrected to 5V reference
const float DEFAULT_A1_SENSOR_GAIN = 1.0f;
const float DEFAULT_A1_SENSOR_OFFSET = 0.0f;

// Individual sensor defaults (A5 - Hydraulic Oil) 
// NOTE: A5 pin conflict resolution - use different pins for Meshtastic in test mode
const float DEFAULT_A5_MAX_PRESSURE_PSI = 30.0f;
const float DEFAULT_A5_ADC_VREF = 5.0f;  // Corrected to 5V reference
const float DEFAULT_A5_SENSOR_GAIN = 1.0f;
const float DEFAULT_A5_SENSOR_OFFSET = 0.0f;

// Meshtastic Communication Pins (for mesh test mode)
// Using different pins to avoid conflict with A5 pressure sensor
const uint8_t MESHTASTIC_TX_PIN = A2;  // Arduino A2 → Meshtastic RX
const uint8_t MESHTASTIC_RX_PIN = A3;  // Arduino A3 ← Meshtastic TX
const unsigned long MESHTASTIC_BAUD = 115200;

// Mesh Test Mode Constants
const bool MESH_TEST_MODE_DEFAULT = true;   // Default ON for mesh testing
const unsigned long MESH_TEST_INTERVAL_MS = 1000;  // 1 second telemetry bursts
const unsigned long MESH_TEST_SEQUENCE_DURATION_MS = 30000;  // 30 second cycle

// Current loop sensor constants (4-20mA)
const float CURRENT_LOOP_MIN_VOLTAGE = 1.0f;  // 4mA × 250Ω = 1V
const float CURRENT_LOOP_MAX_VOLTAGE = 5.0f;  // 20mA × 250Ω = 5V

// Pressure sensor extension constants
const float MAIN_PRESSURE_EXT_NEG_FRAC = 0.2f;
const float MAIN_PRESSURE_EXT_POS_FRAC = 0.3f;
const float MAIN_PRESSURE_EXT_FSV = 5.0f;

// Safety Constants
const float SAFETY_THRESHOLD_PSI = 2500.0f;
const float SAFETY_HYSTERESIS_PSI = 10.0f;

// Pressure-based sequence control
const float EXTEND_PRESSURE_LIMIT_PSI = 2300.0f;  // Pressure that triggers extend limit reached
const float RETRACT_PRESSURE_LIMIT_PSI = 2300.0f; // Pressure that triggers retract limit reached

// Sequence Control Constants
const unsigned long DEFAULT_SEQUENCE_STABLE_MS = 15;  // Reduced for fast limit switch detection on moving cylinder
const unsigned long DEFAULT_SEQUENCE_START_STABLE_MS = 100;
const unsigned long DEFAULT_SEQUENCE_TIMEOUT_MS = 30000;
const unsigned long SEQUENCE_STAGE_TIMEOUT_MS = 30000;

// Memory Management
const size_t SHARED_BUFFER_SIZE = 256;  // Increased for comprehensive status output
const size_t TOPIC_BUFFER_SIZE = 64;
const size_t COMMAND_BUFFER_SIZE = 80;
const size_t MAX_CMD_LENGTH = 16;

// EEPROM Configuration
const uint32_t CALIB_MAGIC = 0x43414C49; // 'CALI'
const int CALIB_EEPROM_ADDR = 0;

// Filter Types
enum FilterMode { 
    FILTER_NONE = 0, 
    FILTER_MEDIAN3 = 1, 
    FILTER_EMA = 2 
};

// System States
enum SystemState {
    SYS_INITIALIZING,
    SYS_CONNECTING,
    SYS_RUNNING,
    SYS_ERROR,
    SYS_SAFE_MODE
};

// Command validation
extern const char* const ALLOWED_COMMANDS[];
extern const char* const ALLOWED_SET_PARAMS[];