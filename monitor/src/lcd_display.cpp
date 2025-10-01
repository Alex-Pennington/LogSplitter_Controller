#include "lcd_display.h"

extern void debugPrintf(const char* fmt, ...);

LCDDisplay::LCDDisplay(uint8_t address, uint8_t cols, uint8_t rows) 
    : i2cAddress(address), columns(cols), rows(rows), initialized(false), 
      displayEnabled(true), backlightEnabled(true), lastUpdate(0)
{
    // Initialize content strings
    line1Content = "";
    line2Content = "";
    line3Content = "";
    line4Content = "";
}

bool LCDDisplay::begin() {
    debugPrintf("LCD: Initializing LCD display at address 0x%02X using Wire1", i2cAddress);
    
    // Initialize Wire1 for all I2C devices on Arduino R4 WiFi
    Wire1.begin();
    delay(50); // Give I2C time to initialize
    
    // Check if device is present first
    if (!checkI2CDevice(i2cAddress)) {
        debugPrintf("LCD: Device not found at address 0x%02X", i2cAddress);
        return false;
    }
    
    // Set up backlight first
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(0x08);  // Backlight on, enable bit, D7-D4 all 0
    if (Wire1.endTransmission() != 0) {
        debugPrintf("LCD: Failed to set initial backlight state");
        return false;
    }
    
    delay(50);
    
    // Initialize LCD in 4-bit mode according to HD44780 datasheet
    
    // Wait for LCD to power up (>40ms after Vcc rises to 2.7V)
    delay(50);
    
    // Special sequence for 4-bit mode initialization
    write4bits(0x30);  // 8-bit mode, try 1
    delay(5);
    write4bits(0x30);  // 8-bit mode, try 2  
    delay(1);
    write4bits(0x30);  // 8-bit mode, try 3
    delay(1);
    write4bits(0x20);  // Switch to 4-bit mode
    delay(1);
    
    // Now we can use standard commands
    sendCommand(0x28);  // Function set: 4-bit mode, 2 lines, 5x8 dots
    delay(1);
    sendCommand(0x0C);  // Display on, cursor off, blink off
    delay(1);
    sendCommand(0x06);  // Entry mode: increment cursor, no shift
    delay(1);
    sendCommand(0x01);  // Clear display
    delay(2);
    
    initialized = true;
    debugPrintf("LCD: Initialization complete");
    return true;
}

bool LCDDisplay::isAvailable() {
    return initialized;
}

bool LCDDisplay::checkI2CDevice(uint8_t address) {
    Wire1.beginTransmission(address);
    return (Wire1.endTransmission() == 0);
}

void LCDDisplay::i2cWrite(uint8_t data) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(data);
    Wire1.endTransmission();
}

void LCDDisplay::write4bits(uint8_t data) {
    uint8_t val = data | (backlightEnabled ? 0x08 : 0x00);
    i2cWrite(val);
    pulseEnable(val);
}

void LCDDisplay::pulseEnable(uint8_t data) {
    i2cWrite(data | 0x04);   // Enable high
    delayMicroseconds(1);
    i2cWrite(data & ~0x04);  // Enable low
    delayMicroseconds(50);
}

void LCDDisplay::sendCommand(uint8_t command) {
    uint8_t highnib = command & 0xF0;
    uint8_t lownib = (command << 4) & 0xF0;
    write4bits(highnib);
    write4bits(lownib);
}

void LCDDisplay::sendData(uint8_t data) {
    uint8_t highnib = (data & 0xF0) | 0x01;  // RS bit high for data
    uint8_t lownib = ((data << 4) & 0xF0) | 0x01;
    write4bits(highnib);
    write4bits(lownib);
}

void LCDDisplay::updateSystemStatus(uint8_t state, unsigned long uptime, bool wifiConnected, bool mqttConnected) {
    if (!initialized || !displayEnabled) return;
    
    // Format: "ST:RUN [WM] 123.4h" (20 chars max)
    String content = "ST:" + formatSystemState(state) + " [";
    content += wifiConnected ? "W" : "-";
    content += mqttConnected ? "M" : "-";
    content += "] ";
    content += formatUptimeDecimal(uptime);
    
    // Pad or truncate to exactly 20 characters
    while (content.length() < 20) content += " ";
    if (content.length() > 20) content = content.substring(0, 20);
    
    updateLineIfChanged(0, content, line1Content);
}

void LCDDisplay::updateNetworkStatus(bool wifiConnected, bool mqttConnected, const char* ipAddress) {
    // This method is now used for displaying temperature on line 2
    // Keeping for compatibility but functionality moved to updateTemperatureReadings
}

void LCDDisplay::updateSensorReadings(float localTemp, float weight, float remoteTemp) {
    if (!initialized || !displayEnabled) return;
    
    // Line 2: System Temperature (remote/thermocouple only)
    String tempContent = "ST: ";
    if (remoteTemp >= -100.0) {
        tempContent += String(remoteTemp, 1) + "F";
    } else {
        tempContent += "---F";
    }
    
    // Pad or truncate to exactly 20 characters
    while (tempContent.length() < 20) tempContent += " ";
    if (tempContent.length() > 20) tempContent = tempContent.substring(0, 20);
    
    updateLineIfChanged(1, tempContent, line2Content);
    
    // Line 3: Weight reading
    String weightContent = "Weight: ";
    if (weight >= 0) {
        if (weight < 1000) {
            weightContent += String(weight, 1) + "g";
        } else {
            weightContent += String(weight/1000.0, 2) + "kg";
        }
    } else {
        weightContent += "---";
    }
    
    // Pad or truncate to exactly 20 characters
    while (weightContent.length() < 20) weightContent += " ";
    if (weightContent.length() > 20) weightContent = weightContent.substring(0, 20);
    
    updateLineIfChanged(2, weightContent, line3Content);
}

void LCDDisplay::showError(const char* error) {
    if (!initialized || !displayEnabled) return;
    
    String content = "ERR: ";
    content += error;
    
    // Pad or truncate to exactly 20 characters
    while (content.length() < 20) content += " ";
    if (content.length() > 20) content = content.substring(0, 20);
    
    updateLineIfChanged(3, content, line4Content);
}

void LCDDisplay::showInfo(const char* info) {
    if (!initialized || !displayEnabled) return;
    
    String content = info;
    
    // Pad or truncate to exactly 20 characters
    while (content.length() < 20) content += " ";
    if (content.length() > 20) content = content.substring(0, 20);
    
    updateLineIfChanged(3, content, line4Content);
}

void LCDDisplay::clearLine(uint8_t line) {
    if (!initialized || !displayEnabled || line >= 4) return;
    
    setCursorToLine(line);
    for (int i = 0; i < 20; i++) {
        sendData(' ');
    }
    
    // Update our content tracking
    switch (line) {
        case 0: line1Content = ""; break;
        case 1: line2Content = ""; break;
        case 2: line3Content = ""; break;
        case 3: line4Content = ""; break;
    }
}

void LCDDisplay::clear() {
    if (!initialized || !displayEnabled) return;
    
    sendCommand(0x01);  // Clear display
    delay(2);
    
    // Clear our content tracking
    line1Content = "";
    line2Content = "";
    line3Content = "";
    line4Content = "";
}

void LCDDisplay::setEnabled(bool enabled) {
    displayEnabled = enabled;
    if (!enabled && initialized) {
        // Turn off display
        sendCommand(0x08);  // Display off
    } else if (enabled && initialized) {
        // Turn on display
        sendCommand(0x0C);  // Display on, cursor off, blink off
    }
}

void LCDDisplay::setBacklight(bool enabled) {
    backlightEnabled = enabled;
    if (initialized) {
        // Send a dummy command to update backlight state
        uint8_t val = backlightEnabled ? 0x08 : 0x00;
        i2cWrite(val);
    }
}

void LCDDisplay::showStartupMessage() {
    if (!initialized) return;
    
    clear();
    
    setCursorToLine(0);
    printString("Log Splitter Monitor");
    
    setCursorToLine(1);
    printString("System Starting...");
    
    setCursorToLine(2);
    printString("Please Wait");
}

void LCDDisplay::showConnectingMessage() {
    if (!initialized || !displayEnabled) return;
    
    setCursorToLine(1);
    printString("Connecting WiFi...  ");
    
    setCursorToLine(2);
    printString("                    ");
    
    setCursorToLine(3);
    printString("                    ");
}

void LCDDisplay::showCalibrationMessage(const char* step) {
    if (!initialized || !displayEnabled) return;
    
    setCursorToLine(1);
    printString("Calibrating Scale   ");
    
    setCursorToLine(2);
    String content = step;
    while (content.length() < 20) content += " ";
    if (content.length() > 20) content = content.substring(0, 20);
    printString(content);
    
    setCursorToLine(3);
    printString("                    ");
}

String LCDDisplay::formatUptime(unsigned long uptime) {
    unsigned long seconds = uptime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    if (days > 0) {
        return String(days) + "d" + String(hours % 24) + "h";
    } else if (hours > 0) {
        return String(hours) + "h" + String(minutes % 60) + "m";
    } else {
        return String(minutes) + ":" + String(seconds % 60, DEC);
    }
}

String LCDDisplay::formatUptimeDecimal(unsigned long uptime) {
    float hours = uptime / 3600.0;  // Convert seconds to hours (uptime is already in seconds)
    return String(hours, 1) + "h";
}

String LCDDisplay::formatSystemState(uint8_t state) {
    switch (state) {
        case 0: return "INIT";
        case 1: return "IDLE";
        case 2: return "RUN";
        case 3: return "ERR";
        default: return "UNK";
    }
}

void LCDDisplay::updateLineIfChanged(uint8_t line, const String& content, String& currentContent) {
    if (content != currentContent) {
        setCursorToLine(line);
        printString(content);
        currentContent = content;
    }
}

void LCDDisplay::setCursorToLine(uint8_t line) {
    uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};  // HD44780 memory addresses for 20x4
    if (line < 4) {
        sendCommand(0x80 | rowOffsets[line]);
    }
}

void LCDDisplay::printString(const String& str) {
    for (unsigned int i = 0; i < str.length(); i++) {
        sendData(str.charAt(i));
    }
}