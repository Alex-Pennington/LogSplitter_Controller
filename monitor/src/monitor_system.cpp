#include "monitor_system.h"
#include "network_manager.h"
#include "lcd_display.h"
#include "logger.h"
#include <Arduino.h>
#include <WiFiS3.h>

extern void debugPrintf(const char* fmt, ...);
extern NetworkManager* g_networkManager;
extern LCDDisplay* g_lcdDisplay;
extern MCP9600Sensor* g_mcp9600Sensor;

MonitorSystem::MonitorSystem() :
    currentState(SYS_INITIALIZING),
    systemStartTime(0),
    localTemperature(0.0),
    remoteTemperature(0.0),
    humidity(0.0),
    lastSensorRead(0),
    lastTemperatureRead(0),
    currentWeight(0.0),
    currentRawWeight(0),
    lastWeightRead(0),
    currentVoltage(0.0),
    currentCurrent(0.0),
    currentPower(0.0),
    lastPowerRead(0),
    powerSensorAvailable(false),
    currentAdcVoltage(0.0),
    currentAdcRaw(0),
    lastAdcRead(0),
    adcSensorAvailable(false),
    publishInterval(STATUS_PUBLISH_INTERVAL_MS),
    heartbeatInterval(HEARTBEAT_INTERVAL_MS),
    lastStatusPublish(0),
    lastHeartbeat(0),
    lastSensorAvailable(false),
    i2cMux(0x70) {  // TCA9548A at address 0x70
    
    // Initialize digital I/O states
    for (int i = 0; i < 8; i++) {
        digitalInputStates[i] = false;
        digitalOutputStates[i] = false;
    }
}

void MonitorSystem::begin() {
    systemStartTime = millis();
    initializePins();
    
    // Set global pointer for external access
    g_mcp9600Sensor = &temperatureSensor;
    
    // Initialize I2C multiplexer first with retry logic
    debugPrintf("MonitorSystem: Initializing TCA9548A I2C multiplexer...\n");
    bool muxInitialized = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (i2cMux.begin()) {
            LOG_INFO("MonitorSystem: TCA9548A multiplexer initialized successfully on attempt %d", attempt);
            debugPrintf("MonitorSystem: I2C multiplexer ready\n");
            muxInitialized = true;
            break;
        } else {
            debugPrintf("MonitorSystem: TCA9548A initialization attempt %d failed\n", attempt);
            if (attempt < 3) {
                delay(100); // Brief delay before retry
            }
        }
    }
    
    if (!muxInitialized) {
        LOG_ERROR("MonitorSystem: TCA9548A multiplexer initialization failed after 3 attempts - sensors may not work");
        debugPrintf("MonitorSystem: I2C multiplexer NOT found - sensors may not work!\n");
    }
    
    // Initialize MCP9600 temperature sensor with enhanced error handling
    debugPrintf("MonitorSystem: Initializing MCP9600 temperature sensor on channel %d...\n", MCP9600_CHANNEL);
    if (muxInitialized) {
        i2cMux.selectChannel(MCP9600_CHANNEL);
        delay(10); // Allow multiplexer channel to settle
    }
    
    bool tempSensorInitialized = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (temperatureSensor.begin()) {
            LOG_INFO("MonitorSystem: MCP9600 temperature sensor initialized successfully");
            
            // Enable temperature filtering to smooth readings
            temperatureSensor.enableFiltering(true, 5);  // 5-sample moving average
            
            // Set thermocouple type to K-type (most common)
            temperatureSensor.setThermocoupleType(0x00); // Type K
            debugPrintf("MonitorSystem: MCP9600 configured (Type K, 5-sample filter)\n");
            tempSensorInitialized = true;
            break;
        } else {
            debugPrintf("MonitorSystem: MCP9600 initialization attempt %d failed\n", attempt);
            if (attempt < 2) {
                delay(50);
            }
        }
    }
    
    if (!tempSensorInitialized) {
        LOG_WARN("MonitorSystem: MCP9600 temperature sensor not found at I2C address 0x60-0x67");
        debugPrintf("MonitorSystem: Temperature readings will not be available\n");
    }
    
    // Initialize sensor availability tracking
    lastSensorAvailable = temperatureSensor.isAvailable();
    
    // Initialize NAU7802 weight sensor with enhanced error handling
    debugPrintf("MonitorSystem: Initializing NAU7802 weight sensor on channel %d...\n", NAU7802_CHANNEL);
    if (muxInitialized) {
        i2cMux.selectChannel(NAU7802_CHANNEL);
        delay(10); // Allow multiplexer channel to settle
    }
    
    bool weightSensorInitialized = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        NAU7802Status status = weightSensor.begin();
        if (status == NAU7802_OK) {
            LOG_INFO("MonitorSystem: NAU7802 weight sensor initialized successfully");
            debugPrintf("MonitorSystem: NAU7802 weight sensor initialized successfully\n");
            // Enable filtering for stable readings
            weightSensor.enableFiltering(true, 10);
            weightSensorInitialized = true;
            break;
        } else {
            debugPrintf("MonitorSystem: NAU7802 initialization attempt %d failed: %s\n", 
                attempt, weightSensor.getStatusString());
            if (attempt < 2) {
                delay(100); // Longer delay for NAU7802
            }
        }
    }
    
    if (!weightSensorInitialized) {
        LOG_WARN("MonitorSystem: NAU7802 load cell ADC not found at I2C address 0x2A");
        debugPrintf("MonitorSystem: Weight measurements will not be available\n");
    }
    
    // Initialize INA219 power sensor with enhanced error handling
    debugPrintf("MonitorSystem: Initializing INA219 power sensor on channel %d...\n", INA219_CHANNEL);
    if (muxInitialized) {
        i2cMux.selectChannel(INA219_CHANNEL);
        delay(10); // Allow multiplexer channel to settle
    }
    
    bool powerSensorFound = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (powerSensor.begin()) {
            LOG_INFO("MonitorSystem: INA219 power sensor initialized successfully");
            powerSensorAvailable = true;
            powerSensorFound = true;
            debugPrintf("MonitorSystem: INA219 power sensor ready\n");
            break;
        } else {
            debugPrintf("MonitorSystem: INA219 initialization attempt %d failed\n", attempt);
            if (attempt < 2) {
                delay(50);
            }
        }
    }
    
    if (!powerSensorFound) {
        LOG_WARN("MonitorSystem: INA219 power monitor not found at I2C addresses 0x40-0x45");
        powerSensorAvailable = false;
        debugPrintf("MonitorSystem: Power measurements will not be available\n");
    }
    
    // Initialize MCP3421 ADC sensor with enhanced error handling
    debugPrintf("MonitorSystem: Initializing MCP3421 ADC sensor on channel %d...\n", MCP3421_CHANNEL);
    if (muxInitialized) {
        i2cMux.selectChannel(MCP3421_CHANNEL);
        delay(10); // Allow multiplexer channel to settle
    }
    
    bool adcSensorFound = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (adcSensor.begin()) {
            LOG_INFO("MonitorSystem: MCP3421 ADC sensor initialized successfully");
            adcSensorAvailable = true;
            adcSensorFound = true;
            debugPrintf("MonitorSystem: MCP3421 ADC sensor ready\n");
            break;
        } else {
            debugPrintf("MonitorSystem: MCP3421 initialization attempt %d failed\n", attempt);
            if (attempt < 2) {
                delay(50);
            }
        }
    }
    
    if (!adcSensorFound) {
        LOG_WARN("MonitorSystem: MCP3421 18-bit ADC not found at I2C address 0x68");
        adcSensorAvailable = false;
        debugPrintf("MonitorSystem: ADC measurements will not be available\n");
    }
    
    // Log sensor initialization summary
    int availableSensors = 0;
    if (muxInitialized) availableSensors++;
    if (tempSensorInitialized) availableSensors++;
    if (weightSensorInitialized) availableSensors++;
    if (powerSensorAvailable) availableSensors++;
    if (adcSensorAvailable) availableSensors++;
    
    LOG_INFO("MonitorSystem: Sensor initialization complete - %d of 5 sensors available", availableSensors);
    if (availableSensors < 5) {
        LOG_WARN("MonitorSystem: Some sensors are missing - check I2C connections and power");
    }
    
    // Disable all multiplexer channels when done with initialization
    if (muxInitialized) {
        i2cMux.disableAllChannels();
    }
    
    setSystemState(SYS_CONNECTING);
    debugPrintf("MonitorSystem: All sensors initialized\n");
}

void MonitorSystem::initializePins() {
    // Configure status LED
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // Configure digital inputs with pullup
    pinMode(DIGITAL_INPUT_1, INPUT_PULLUP);
    pinMode(DIGITAL_INPUT_2, INPUT_PULLUP);
    
    // Configure digital outputs
    pinMode(DIGITAL_OUTPUT_1, OUTPUT);
    pinMode(DIGITAL_OUTPUT_2, OUTPUT);
    digitalWrite(DIGITAL_OUTPUT_1, LOW);
    digitalWrite(DIGITAL_OUTPUT_2, LOW);
    
    // Configure additional watch pins as inputs
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        if (WATCH_PINS[i] != DIGITAL_OUTPUT_1 && WATCH_PINS[i] != DIGITAL_OUTPUT_2) {
            pinMode(WATCH_PINS[i], INPUT_PULLUP);
        }
    }
    
    debugPrintf("MonitorSystem: Pins initialized\n");
}

void MonitorSystem::update() {
    unsigned long now = millis();
    
    // Read sensors periodically
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
        readSensors();
        lastSensorRead = now;
    }
    
    // Read weight sensor at its own interval
    if (now - lastWeightRead >= NAU7802_READ_INTERVAL_MS) {
        readWeightSensor();
        lastWeightRead = now;
    }
    
    // Read power sensor periodically
    if (now - lastPowerRead >= 2000) { // Read every 2 seconds
        readPowerSensor();
        lastPowerRead = now;
    }
    
    // Read ADC sensor periodically  
    if (now - lastAdcRead >= 1500) { // Read every 1.5 seconds
        readAdcSensor();
        lastAdcRead = now;
    }
    
    // Check I2C device health periodically
    checkI2CDeviceHealth();
    
    // Update LCD display periodically
    static unsigned long lastLCDUpdate = 0;
    if (now - lastLCDUpdate >= 1000) { // Update LCD every second
        updateLCDDisplay();
        lastLCDUpdate = now;
    }
    
    // Publish status periodically
    if (now - lastStatusPublish >= publishInterval) {
        publishStatus();
        lastStatusPublish = now;
    }
    
    // Publish heartbeat periodically
    if (now - lastHeartbeat >= heartbeatInterval) {
        publishHeartbeat();
        lastHeartbeat = now;
    }
    
    // Update status LED based on system state
    switch (currentState) {
        case SYS_INITIALIZING:
            // Fast blink during initialization
            digitalWrite(STATUS_LED_PIN, (now / 100) % 2);
            break;
        case SYS_CONNECTING:
            // Slow blink while connecting
            digitalWrite(STATUS_LED_PIN, (now / 500) % 2);
            break;
        case SYS_MONITORING:
            // Solid on when monitoring
            digitalWrite(STATUS_LED_PIN, HIGH);
            break;
        case SYS_ERROR:
            // Very fast blink for errors
            digitalWrite(STATUS_LED_PIN, (now / 50) % 2);
            break;
        case SYS_MAINTENANCE:
            // Alternating pattern for maintenance
            digitalWrite(STATUS_LED_PIN, ((now / 200) % 4) < 2);
            break;
    }
}

void MonitorSystem::readSensors() {
    readAnalogSensors();
    readTemperatureSensor();
    readDigitalInputs();
}

void MonitorSystem::readAnalogSensors() {
    // Analog sensors removed - using dedicated I2C sensors instead
    // MAX6656 provides temperature readings via I2C
    // Voltage monitoring can be added later with proper sensor
    humidity = 0.0; // No humidity sensor connected
}

void MonitorSystem::readTemperatureSensor() {
    unsigned long now = millis();
    if (now - lastTemperatureRead >= 1000) { // Read every second
        // Select MCP9600 multiplexer channel
        i2cMux.selectChannel(MCP9600_CHANNEL);
        
        bool currentAvailable = temperatureSensor.isAvailable();
        
        // Check for sensor availability state changes
        if (lastSensorAvailable != currentAvailable) {
            if (currentAvailable) {
                LOG_INFO("MCP9600 temperature sensor reconnected");
                debugPrintf("\n=== MCP9600 sensor reconnected ===\n");
            } else {
                LOG_CRITICAL("MCP9600 temperature sensor disconnected or failed");
                debugPrintf("\n*** CRITICAL - MCP9600 sensor disconnected! ***\n");
            }
            lastSensorAvailable = currentAvailable;
        }
        
        if (currentAvailable) {
            // Only show verbose debug if temperature sensor debug is enabled
            bool tempDebugEnabled = temperatureSensor.isDebugEnabled();
            
            if (tempDebugEnabled) {
                debugPrintf("\n--- Temperature Reading Cycle ---\n");
            }
            
            float newLocalTemp = temperatureSensor.getLocalTemperature();
            float newRemoteTemp = temperatureSensor.getRemoteTemperature();
            
            // Static variables to track previous temperatures for validation
            static float lastLocalTemp = newLocalTemp;
            static float lastRemoteTemp = newRemoteTemp;
            static bool firstRead = true;
            
            if (!firstRead) {
                float localChange = newLocalTemp - lastLocalTemp;
                float remoteChange = newRemoteTemp - lastRemoteTemp;
                
                if (tempDebugEnabled) {
                    debugPrintf("MonitorSystem: Local change: %.3fC (%.1f -> %.1f)\n", localChange, lastLocalTemp, newLocalTemp);
                    debugPrintf("MonitorSystem: Remote change: %.3fC (%.1f -> %.1f)\n", remoteChange, lastRemoteTemp, newRemoteTemp);
                }
                
                // Rate limiting: reject readings with unreasonable changes (>20C/second for thermocouple)
                if (abs(localChange) > 20.0 && newLocalTemp > -990.0) {
                    if (tempDebugEnabled) {
                        debugPrintf("MonitorSystem: *** REJECTED local temp jump: %.1fC ***\n", localChange);
                    }
                    newLocalTemp = lastLocalTemp; // Keep previous value
                }
                
                if (abs(remoteChange) > 20.0 && newRemoteTemp > -990.0) {
                    if (tempDebugEnabled) {
                        debugPrintf("MonitorSystem: *** REJECTED remote temp jump: %.1fC ***\n", remoteChange);
                    }
                    newRemoteTemp = lastRemoteTemp; // Keep previous value
                }
            } else {
                if (tempDebugEnabled) {
                    debugPrintf("MonitorSystem: First temperature reading\n");
                }
            }
            
            // Only update if readings are reasonable
            if (newLocalTemp > -990.0) {
                localTemperature = newLocalTemp;
                lastLocalTemp = newLocalTemp;
            }
            
            if (newRemoteTemp > -990.0) {
                remoteTemperature = newRemoteTemp;
                lastRemoteTemp = newRemoteTemp;
            }
            
            if (tempDebugEnabled) {
                debugPrintf("MonitorSystem: FINAL - Local: %.1fC, Remote: %.1fC\n", localTemperature, remoteTemperature);
                debugPrintf("--- End Temperature Cycle ---\n\n");
            }
            
            firstRead = false;
            lastTemperatureRead = now;
        } else {
            // Sensor not available - use last known values but don't update timestamp
            // This will prevent stale data from being reported as current
        }
    }
}

void MonitorSystem::readDigitalInputs() {
    // Read all digital inputs
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        uint8_t pin = WATCH_PINS[i];
        // Skip output pins
        if (pin != DIGITAL_OUTPUT_1 && pin != DIGITAL_OUTPUT_2) {
            bool currentState = digitalRead(pin) == LOW; // Active low with pullup
            size_t index = i < 8 ? i : 7; // Bounds check
            
            // Detect state change
            if (digitalInputStates[index] != currentState) {
                digitalInputStates[index] = currentState;
                LOG_INFO("MonitorSystem: Digital input %d changed to %s", 
                    pin, currentState ? "ACTIVE" : "INACTIVE");
                
                // Publish state change to MQTT
                if (g_networkManager && g_networkManager->isMQTTConnected()) {
                    char topic[64];
                    snprintf(topic, sizeof(topic), "r4/monitor/input/%d", pin);
                    g_networkManager->publish(topic, currentState ? "1" : "0");
                }
            }
        }
    }
}

void MonitorSystem::readWeightSensor() {
    // Select NAU7802 multiplexer channel
    i2cMux.selectChannel(NAU7802_CHANNEL);
    
    if (!weightSensor.isConnected()) {
        return;
    }
    
    if (weightSensor.dataAvailable()) {
        weightSensor.updateReading();
        currentRawWeight = weightSensor.getRawReading();
        currentWeight = weightSensor.getFilteredWeight();
        
        // Publish weight data to MQTT
        if (g_networkManager && g_networkManager->isMQTTConnected()) {
            char valueBuffer[16];
            
            // Publish filtered weight
            snprintf(valueBuffer, sizeof(valueBuffer), "%.3f", currentWeight);
            g_networkManager->publish(TOPIC_NAU7802_WEIGHT, valueBuffer);
            
            // Publish raw reading
            snprintf(valueBuffer, sizeof(valueBuffer), "%ld", currentRawWeight);
            g_networkManager->publish(TOPIC_NAU7802_RAW, valueBuffer);
            
            // Publish comprehensive weight sensor status
            char statusBuffer[128];
            snprintf(statusBuffer, sizeof(statusBuffer), 
                "status: %s, ready: %s, weight: %.3f, raw: %ld",
                weightSensor.getStatusString(),
                weightSensor.isReady() ? "YES" : "NO",
                currentWeight,
                currentRawWeight);
            g_networkManager->publish(TOPIC_NAU7802_STATUS, statusBuffer);
        }
    }
}

void MonitorSystem::readPowerSensor() {
    debugPrintf("MonitorSystem: readPowerSensor() called, available=%s\n", powerSensorAvailable ? "YES" : "NO");
    
    // Only attempt to read if sensor was successfully initialized
    if (!powerSensorAvailable) {
        debugPrintf("MonitorSystem: Power sensor not available, skipping read\n");
        return;
    }
    
    // Select INA219 multiplexer channel
    i2cMux.selectChannel(INA219_CHANNEL);
    
    debugPrintf("MonitorSystem: Attempting power sensor reading...\n");
    
    // Read power sensor values with error checking
    if (powerSensor.takereading()) {
        currentVoltage = powerSensor.getBusVoltage();
        currentCurrent = powerSensor.getCurrent();
        currentPower = powerSensor.getPower();
        
        debugPrintf("MonitorSystem: Power reading successful: %.3fV, %.2fmA, %.2fmW\n", 
                   currentVoltage, currentCurrent, currentPower);
        
        // Publish power data to MQTT
        if (g_networkManager && g_networkManager->isMQTTConnected()) {
            debugPrintf("MonitorSystem: Publishing power data to MQTT\n");
            char valueBuffer[16];
            
            // Publish bus voltage
            snprintf(valueBuffer, sizeof(valueBuffer), "%.3f", currentVoltage);
            g_networkManager->publish(TOPIC_INA219_VOLTAGE, valueBuffer);
            
            // Publish current in mA
            snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", currentCurrent);
            g_networkManager->publish(TOPIC_INA219_CURRENT, valueBuffer);
            
            // Publish power in mW
            snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", currentPower);
            g_networkManager->publish(TOPIC_INA219_POWER, valueBuffer);
            
            // Publish comprehensive power sensor status
            char statusBuffer[128];
            snprintf(statusBuffer, sizeof(statusBuffer), 
                "ready: %s, voltage: %.3fV, current: %.2fmA, power: %.2fmW",
                powerSensorAvailable ? "YES" : "NO",
                currentVoltage,
                currentCurrent,
                currentPower);
            g_networkManager->publish(TOPIC_INA219_STATUS, statusBuffer);
            debugPrintf("MonitorSystem: Power data published to MQTT successfully\n");
        } else {
            debugPrintf("MonitorSystem: MQTT not connected, cannot publish power data\n");
        }
    } else {
        debugPrintf("MonitorSystem: Power sensor reading failed\n");
    }
}

void MonitorSystem::readAdcSensor() {
    // Only attempt to read if sensor was successfully initialized
    if (!adcSensorAvailable) {
        return;
    }
    
    // Select MCP3421 multiplexer channel
    i2cMux.selectChannel(MCP3421_CHANNEL);
    
    // Take ADC reading with error checking
    if (adcSensor.takeReading()) {
        currentAdcVoltage = adcSensor.getVoltage();
        currentAdcRaw = adcSensor.getRawValue();
        
        // Publish ADC data to MQTT
        if (g_networkManager && g_networkManager->isMQTTConnected()) {
            char valueBuffer[16];
            
            // Publish voltage reading
            snprintf(valueBuffer, sizeof(valueBuffer), "%.6f", currentAdcVoltage);
            g_networkManager->publish(TOPIC_MCP3421_VOLTAGE, valueBuffer);
            
            // Publish raw ADC value
            snprintf(valueBuffer, sizeof(valueBuffer), "%ld", currentAdcRaw);
            g_networkManager->publish(TOPIC_MCP3421_RAW, valueBuffer);
            
            // Publish comprehensive ADC sensor status
            char statusBuffer[128];
            snprintf(statusBuffer, sizeof(statusBuffer), 
                "ready: %s, voltage: %.6fV, raw: %ld, resolution: %d-bit",
                adcSensorAvailable ? "YES" : "NO",
                currentAdcVoltage,
                currentAdcRaw,
                adcSensor.getResolution());
            g_networkManager->publish(TOPIC_MCP3421_STATUS, statusBuffer);
        }
    }
}

void MonitorSystem::publishStatus() {
    if (!g_networkManager || !g_networkManager->isMQTTConnected()) {
        return;
    }
    
    // Publish individual sensor readings
    char valueBuffer[16];
    
    // Publish MCP9600 local temperature (in Fahrenheit)
    float localTempF = (localTemperature * 9.0 / 5.0) + 32.0;
    snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", localTempF);
    g_networkManager->publish(TOPIC_SENSOR_TEMPERATURE, valueBuffer);
    
    // Publish MCP9600 remote temperature if available (in Fahrenheit)
    if (temperatureSensor.isAvailable()) {
        float remoteTempF = (remoteTemperature * 9.0 / 5.0) + 32.0;
        snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", remoteTempF);
        g_networkManager->publish("r4/monitor/temperature/remote", valueBuffer);
    }
    
    snprintf(valueBuffer, sizeof(valueBuffer), "%lu", getUptime());
    g_networkManager->publish(TOPIC_SYSTEM_UPTIME, valueBuffer);
    
    snprintf(valueBuffer, sizeof(valueBuffer), "%lu", getFreeMemory());
    g_networkManager->publish(TOPIC_SYSTEM_MEMORY, valueBuffer);
    
    LOG_DEBUG("MonitorSystem: Status published");
}

void MonitorSystem::publishHeartbeat() {
    if (!g_networkManager || !g_networkManager->isMQTTConnected()) {
        return;
    }
    
    char heartbeatMsg[64];
    snprintf(heartbeatMsg, sizeof(heartbeatMsg), "uptime=%lu state=%d", 
        getUptime(), (int)currentState);
    
    g_networkManager->publish(TOPIC_MONITOR_HEARTBEAT, heartbeatMsg);
    LOG_DEBUG("MonitorSystem: Heartbeat sent");
}

void MonitorSystem::getStatusString(char* buffer, size_t bufferSize) {
    // Convert cached Celsius temperatures to Fahrenheit for display
    float localTempF = (localTemperature * 9.0 / 5.0) + 32.0;
    float remoteTempF = (remoteTemperature * 9.0 / 5.0) + 32.0;
    
    snprintf(buffer, bufferSize,
        "local=%.1f remote=%.1f weight=%.3f uptime=%lu state=%d memory=%lu inputs=%d%d%d%d",
        localTempF, remoteTempF, currentWeight, getUptime(), (int)currentState, getFreeMemory(),
        digitalInputStates[0] ? 1 : 0,
        digitalInputStates[1] ? 1 : 0,
        digitalInputStates[2] ? 1 : 0,
        digitalInputStates[3] ? 1 : 0);
}

float MonitorSystem::getLocalTemperature() const {
    return localTemperature;
}

float MonitorSystem::getRemoteTemperature() const {
    return remoteTemperature;
}

float MonitorSystem::getLocalTemperatureF() const {
    return temperatureSensor.getLocalTemperatureF();
}

float MonitorSystem::getRemoteTemperatureF() const {
    return temperatureSensor.getRemoteTemperatureF();
}

float MonitorSystem::getHumidity() const {
    return humidity;
}

bool MonitorSystem::isTemperatureSensorReady() {
    // Select MCP9600 multiplexer channel before checking availability
    i2cMux.selectChannel(MCP9600_CHANNEL);
    return temperatureSensor.isAvailable();
}

void MonitorSystem::getTemperatureSensorStatus(char* buffer, size_t bufferSize) {
    temperatureSensor.getStatusString(buffer, bufferSize);
}

void MonitorSystem::setTemperatureOffset(float localOffset, float remoteOffset) {
    temperatureSensor.setTemperatureOffset(localOffset, remoteOffset);
}

MCP9600Sensor* MonitorSystem::getTemperatureSensor() {
    return &temperatureSensor;
}

unsigned long MonitorSystem::getUptime() const {
    return (millis() - systemStartTime) / 1000; // Uptime in seconds
}

unsigned long MonitorSystem::getFreeMemory() const {
    // Rough estimate of free memory for Arduino
    // This is a simplified calculation
    return 32768 - 11000; // Assume ~11KB used, 32KB total for UNO R4
}

SystemState MonitorSystem::getSystemState() const {
    return currentState;
}

void MonitorSystem::setSystemState(SystemState state) {
    if (currentState != state) {
        debugPrintf("MonitorSystem: State changed from %d to %d\n", (int)currentState, (int)state);
        currentState = state;
    }
}

bool MonitorSystem::getDigitalInput(uint8_t pin) const {
    // Find the pin in the watch pins array
    for (size_t i = 0; i < WATCH_PIN_COUNT && i < 8; i++) {
        if (WATCH_PINS[i] == pin) {
            return digitalInputStates[i];
        }
    }
    return false;
}

void MonitorSystem::setDigitalOutput(uint8_t pin, bool state) {
    if (pin == DIGITAL_OUTPUT_1 || pin == DIGITAL_OUTPUT_2) {
        digitalWrite(pin, state ? HIGH : LOW);
        
        // Update internal state tracking
        if (pin == DIGITAL_OUTPUT_1) {
            digitalOutputStates[0] = state;
        } else if (pin == DIGITAL_OUTPUT_2) {
            digitalOutputStates[1] = state;
        }
        
        debugPrintf("MonitorSystem: Digital output %d set to %s\n", 
            pin, state ? "HIGH" : "LOW");
    }
}

void MonitorSystem::setPublishInterval(unsigned long interval) {
    publishInterval = interval;
    debugPrintf("MonitorSystem: Publish interval set to %lu ms\n", interval);
}

void MonitorSystem::setHeartbeatInterval(unsigned long interval) {
    heartbeatInterval = interval;
    debugPrintf("MonitorSystem: Heartbeat interval set to %lu ms\n", interval);
}

unsigned long MonitorSystem::getPublishInterval() const {
    return publishInterval;
}

unsigned long MonitorSystem::getHeartbeatInterval() const {
    return heartbeatInterval;
}

// NAU7802 Weight Sensor Functions
float MonitorSystem::getWeight() {
    return currentWeight;
}

float MonitorSystem::getFilteredWeight() {
    return weightSensor.getFilteredWeight();
}

long MonitorSystem::getRawWeight() const {
    return currentRawWeight;
}

bool MonitorSystem::isWeightSensorReady() {
    return weightSensor.isReady();
}

NAU7802Status MonitorSystem::getWeightSensorStatus() const {
    return weightSensor.getLastError();
}

// INA219 Power Sensor Functions
float MonitorSystem::getBusVoltage() {
    return currentVoltage;
}

float MonitorSystem::getCurrent() {
    return currentCurrent;
}

float MonitorSystem::getPower() {
    return currentPower;
}

bool MonitorSystem::isPowerSensorReady() {
    return powerSensorAvailable;
}

// MCP3421 ADC Sensor Functions
float MonitorSystem::getAdcVoltage() {
    return currentAdcVoltage;
}

int32_t MonitorSystem::getAdcRawValue() {
    return currentAdcRaw;
}

float MonitorSystem::getFilteredAdcVoltage(uint8_t samples) {
    return adcSensor.getFilteredVoltage(samples);
}

bool MonitorSystem::isAdcSensorReady() {
    return adcSensorAvailable;
}

void MonitorSystem::getAdcSensorStatus(char* buffer, size_t bufferSize) {
    adcSensor.getStatusString(buffer, bufferSize);
}

MCP3421_Sensor* MonitorSystem::getAdcSensor() {
    return &adcSensor;
}

bool MonitorSystem::calibrateWeightSensorZero() {
    debugPrintf("MonitorSystem: Starting weight sensor zero calibration\n");
    NAU7802Status status = weightSensor.calibrateZero();
    if (status == NAU7802_OK) {
        debugPrintf("MonitorSystem: Weight sensor zero calibration completed\n");
        return true;
    } else {
        debugPrintf("MonitorSystem: Weight sensor zero calibration failed: %s\n", 
            weightSensor.getStatusString());
        return false;
    }
}

bool MonitorSystem::calibrateWeightSensorScale(float knownWeight) {
    debugPrintf("MonitorSystem: Starting weight sensor scale calibration with %.2f\n", knownWeight);
    NAU7802Status status = weightSensor.calibrateScale(knownWeight);
    if (status == NAU7802_OK) {
        debugPrintf("MonitorSystem: Weight sensor scale calibration completed\n");
        return true;
    } else {
        debugPrintf("MonitorSystem: Weight sensor scale calibration failed: %s\n", 
            weightSensor.getStatusString());
        return false;
    }
}

void MonitorSystem::tareWeightSensor() {
    debugPrintf("MonitorSystem: Taring weight sensor\n");
    weightSensor.tareScale();
}

bool MonitorSystem::saveWeightCalibration() {
    return weightSensor.saveCalibration();
}

bool MonitorSystem::loadWeightCalibration() {
    return weightSensor.loadCalibration();
}

void MonitorSystem::updateLCDDisplay() {
    // Only update LCD if it's available
    if (!g_lcdDisplay) return;
    
    // Select LCD multiplexer channel
    i2cMux.selectChannel(LCD_CHANNEL);
    
    // Get network status for combined display
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool syslogWorking = false;
    
    if (g_networkManager) {
        wifiConnected = g_networkManager->isWiFiConnected();
        mqttConnected = g_networkManager->isMQTTConnected();
        syslogWorking = g_networkManager->isSyslogWorking();
    }
    
    // Update system status with network status and runtime (line 1)
    unsigned long uptime = getUptime();
    g_lcdDisplay->updateSystemStatus(currentState, uptime, wifiConnected, mqttConnected, syslogWorking);
    
    // Update sensor readings (lines 2-3) - Show MCP9600 temperatures in Fahrenheit and weight
    // Use temperature values directly - sensor availability checked during sensor reads
    float displayLocalTempF = (localTemperature > -100.0) ? 
                               (localTemperature * 9.0 / 5.0) + 32.0 : -999.0;
    float displayRemoteTempF = (remoteTemperature > -100.0) ? 
                               (remoteTemperature * 9.0 / 5.0) + 32.0 : -999.0;
    g_lcdDisplay->updateSensorReadings(displayLocalTempF, currentWeight, displayRemoteTempF);
    
    // Update additional sensor data (line 4) - Power and ADC sensors
    g_lcdDisplay->updateAdditionalSensors(currentVoltage, currentCurrent, currentAdcVoltage);
}

// I2C Health monitoring functions
void MonitorSystem::checkI2CDeviceHealth() {
    static unsigned long lastHealthCheck = 0;
    unsigned long now = millis();
    
    // Check I2C device health every 30 seconds
    if (now - lastHealthCheck < 30000) {
        return;
    }
    lastHealthCheck = now;
    
    debugPrintf("MonitorSystem: Performing I2C device health check...\n");
    
    // Check critical I2C devices
    bool muxResponding = isI2CDeviceResponding(0x70);
    bool mcp9600Responding = false;
    bool nau7802Responding = false;
    bool ina219Responding = false;
    bool mcp3421Responding = false;
    
    // Check MCP9600 on multiple possible addresses
    for (uint8_t addr = 0x60; addr <= 0x67 && !mcp9600Responding; addr++) {
        if (muxResponding) {
            i2cMux.selectChannel(MCP9600_CHANNEL);
            delay(5);
        }
        mcp9600Responding = isI2CDeviceResponding(addr);
    }
    
    // Check other sensors through multiplexer if available
    if (muxResponding) {
        i2cMux.selectChannel(NAU7802_CHANNEL);
        delay(5);
        nau7802Responding = isI2CDeviceResponding(0x2A);
        
        i2cMux.selectChannel(INA219_CHANNEL);
        delay(5);
        ina219Responding = isI2CDeviceResponding(0x40);
        
        i2cMux.selectChannel(MCP3421_CHANNEL);
        delay(5);
        mcp3421Responding = isI2CDeviceResponding(0x68);
        
        i2cMux.disableAllChannels();
    }
    
    // Log any devices that have become unresponsive
    if (!muxResponding) {
        LOG_ERROR("MonitorSystem: TCA9548A I2C multiplexer is not responding - all sensors unavailable");
    }
    
    if (!mcp9600Responding && lastSensorAvailable) {
        LOG_WARN("MonitorSystem: MCP9600 temperature sensor is not responding");
    }
    
    if (!nau7802Responding) {
        LOG_WARN("MonitorSystem: NAU7802 weight sensor is not responding");
    }
    
    if (!ina219Responding && powerSensorAvailable) {
        LOG_WARN("MonitorSystem: INA219 power monitor is not responding");
    }
    
    if (!mcp3421Responding && adcSensorAvailable) {
        LOG_WARN("MonitorSystem: MCP3421 ADC sensor is not responding");
    }
    
    // Update availability flags based on health check
    powerSensorAvailable = ina219Responding;
    adcSensorAvailable = mcp3421Responding;
    
    debugPrintf("MonitorSystem: I2C health check complete - Mux:%s MCP9600:%s NAU7802:%s INA219:%s MCP3421:%s\n",
        muxResponding ? "OK" : "FAIL",
        mcp9600Responding ? "OK" : "FAIL", 
        nau7802Responding ? "OK" : "FAIL",
        ina219Responding ? "OK" : "FAIL",
        mcp3421Responding ? "OK" : "FAIL");
}

bool MonitorSystem::isI2CDeviceResponding(uint8_t address) {
    Wire1.beginTransmission(address);
    byte error = Wire1.endTransmission();
    return (error == 0); // 0 means success (ACK received)
}
