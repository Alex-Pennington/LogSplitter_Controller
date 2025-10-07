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
    currentAdcVoltage(0.0),
    currentAdcRaw(0),
    lastAdcRead(0),
    publishInterval(STATUS_PUBLISH_INTERVAL_MS),
    heartbeatInterval(HEARTBEAT_INTERVAL_MS),
    lastStatusPublish(0),
    lastHeartbeat(0),
    lastSensorAvailable(false) {
    
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
    
    // Initialize MCP9600 temperature sensor first
    debugPrintf("MonitorSystem: Initializing MCP9600 temperature sensor...");
    if (temperatureSensor.begin()) {
        LOG_INFO("MonitorSystem: MCP9600 temperature sensor initialized successfully");
        
        // Enable temperature filtering to smooth readings
        temperatureSensor.enableFiltering(true, 5);  // 5-sample moving average
        
        // Set thermocouple type to K-type (most common)
        temperatureSensor.setThermocoupleType(0x00); // Type K
        debugPrintf("MonitorSystem: MCP9600 configured (Type K, 5-sample filter)\n");
    } else {
        LOG_WARN("MonitorSystem: MCP9600 temperature sensor initialization failed or not present");
    }
    
    // Initialize sensor availability tracking
    lastSensorAvailable = temperatureSensor.isAvailable();
    
    // Initialize NAU7802 weight sensor
    debugPrintf("MonitorSystem: Initializing NAU7802 weight sensor...\n");
    NAU7802Status status = weightSensor.begin();
    if (status == NAU7802_OK) {
        debugPrintf("MonitorSystem: NAU7802 weight sensor initialized successfully\n");
        // Enable filtering for stable readings
        weightSensor.enableFiltering(true, 10);
    } else {
        debugPrintf("MonitorSystem: NAU7802 weight sensor initialization failed: %s\n", 
            weightSensor.getStatusString());
    }
    
    // Initialize INA219 power sensor
    debugPrintf("MonitorSystem: Initializing INA219 power sensor...\n");
    if (powerSensor.begin(INA219_Range::RANGE_16V_400MA)) {
        debugPrintf("MonitorSystem: INA219 power sensor initialized successfully (16V/400mA range)\n");
    } else {
        debugPrintf("MonitorSystem: INA219 power sensor initialization failed or not present\n");
    }
    
    // Initialize MCP3421 ADC sensor
    debugPrintf("MonitorSystem: Initializing MCP3421 ADC sensor...\n");
    if (adcSensor.begin()) {
        // Set default configuration: 16-bit, 15 SPS, Gain 1x, Continuous mode
        if (adcSensor.setConfiguration(MCP3421_16_BIT_15_SPS, MCP3421_GAIN_1X, MCP3421_CONTINUOUS)) {
            debugPrintf("MonitorSystem: MCP3421 ADC sensor initialized successfully (16-bit, 15 SPS, 1x gain)\n");
        } else {
            debugPrintf("MonitorSystem: MCP3421 ADC sensor configuration failed\n");
        }
    } else {
        debugPrintf("MonitorSystem: MCP3421 ADC sensor initialization failed or not present\n");
    }
    
    setSystemState(SYS_CONNECTING);
    debugPrintf("MonitorSystem: Initialized\n");
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
    if (now - lastPowerRead >= POWER_READ_INTERVAL_MS) {
        readPowerSensor();
        lastPowerRead = now;
    }
    
    // Read ADC sensor periodically
    if (now - lastAdcRead >= ADC_READ_INTERVAL_MS) {
        readAdcSensor();
        lastAdcRead = now;
    }
    
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
                
                // Rate limiting: reject readings with unreasonable changes (>5C/second)
                if (abs(localChange) > 5.0 && newLocalTemp > -990.0) {
                    if (tempDebugEnabled) {
                        debugPrintf("MonitorSystem: *** REJECTED local temp jump: %.1fC ***\n", localChange);
                    }
                    newLocalTemp = lastLocalTemp; // Keep previous value
                }
                
                if (abs(remoteChange) > 5.0 && newRemoteTemp > -990.0) {
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
    if (!powerSensor.isConnected()) {
        return;
    }
    
    // Read power sensor values
    currentVoltage = powerSensor.getBusVoltage();
    currentCurrent = powerSensor.getCurrent();
    currentPower = powerSensor.getPower();
    
    // Publish power data to MQTT
    if (g_networkManager && g_networkManager->isMQTTConnected()) {
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
            powerSensor.isConnected() ? "YES" : "NO",
            currentVoltage,
            currentCurrent,
            currentPower);
        g_networkManager->publish(TOPIC_INA219_STATUS, statusBuffer);
    }
}

void MonitorSystem::readAdcSensor() {
    if (!adcSensor.isConnected()) {
        return;
    }
    
    // Take ADC reading
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
                adcSensor.isConnected() ? "YES" : "NO",
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
    return powerSensor.isConnected();
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
    return adcSensor.isConnected();
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
    
    // Get network status for combined display
    bool wifiConnected = false;
    bool mqttConnected = false;
    
    if (g_networkManager) {
        wifiConnected = g_networkManager->isWiFiConnected();
        mqttConnected = g_networkManager->isMQTTConnected();
    }
    
    // Update system status with network status and runtime (line 1)
    unsigned long uptime = getUptime();
    g_lcdDisplay->updateSystemStatus(currentState, uptime, wifiConnected, mqttConnected);
    
    // Update sensor readings (lines 2-3) - Show MCP9600 temperatures in Fahrenheit and weight
    float displayLocalTempF = (temperatureSensor.isAvailable() && localTemperature > -100.0) ? 
                               (localTemperature * 9.0 / 5.0) + 32.0 : -999.0;
    float displayRemoteTempF = (temperatureSensor.isAvailable() && remoteTemperature > -100.0) ? 
                               (remoteTemperature * 9.0 / 5.0) + 32.0 : -999.0;
    g_lcdDisplay->updateSensorReadings(displayLocalTempF, currentWeight, displayRemoteTempF);
}
