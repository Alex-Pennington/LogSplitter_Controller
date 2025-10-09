#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>

/**
 * LCD Display Manager for 20x4 I2C LCD (LCD2004A)
 * 
 * Manages a 20x4 character LCD display connected via I2C using Wire1 bus
 * for Arduino Uno R4 WiFi compatibility. Provides real-time display of
 * system status, sensor readings, and network information.
 * 
 * This implementation uses direct I2C communication with Wire1 bus
 * instead of the LiquidCrystal_I2C library for full control.
 * 
 * Display Layout:
 * Line 1: System status, network status (W/M), and runtime in decimal hours
 * Line 2: Temperature readings (local and remote)
 * Line 3: Weight readings
 * Line 4: Error messages or additional info
 */
class LCDDisplay {
public:
    /**
     * Constructor
     * @param address I2C address of the LCD (default 0x27)
     * @param cols Number of columns (20 for LCD2004A)
     * @param rows Number of rows (4 for LCD2004A)
     */
    LCDDisplay(uint8_t address = 0x27, uint8_t cols = 20, uint8_t rows = 4);
    
    /**
     * Initialize the LCD display
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * Check if LCD is available and responding
     * @return true if LCD is responding, false otherwise
     */
    bool isAvailable();
    
    /**
     * Update system status display (line 1) with network status and runtime
     * @param state System state (0-4)
     * @param uptime Uptime in seconds
     * @param wifiConnected WiFi connection status
     * @param mqttConnected MQTT connection status
     * @param syslogWorking Syslog functionality status
     */
    void updateSystemStatus(uint8_t state, unsigned long uptime, bool wifiConnected, bool mqttConnected, bool syslogWorking = false);
    
    /**
     * Update network status display (line 2)
     * @param wifiConnected WiFi connection status
     * @param mqttConnected MQTT connection status
     * @param ipAddress IP address string
     */
    void updateNetworkStatus(bool wifiConnected, bool mqttConnected, const char* ipAddress);
    
    /**
     * Update sensor readings display (line 3)
     * @param localTemp Local temperature in Celsius
     * @param weight Weight reading
     * @param remoteTemp Remote temperature in Celsius (optional)
     */
    void updateSensorReadings(float localTemp, float weight, float remoteTemp = -999.0);
    
    /**
     * Update additional sensor data (power, ADC) on line 4
     * @param voltage Bus voltage from power sensor
     * @param current Current from power sensor in mA
     * @param adcVoltage ADC voltage reading
     */
    void updateAdditionalSensors(float voltage, float current, float adcVoltage);
    
    /**
     * Display error message (line 4)
     * @param error Error message (max 20 characters)
     */
    void showError(const char* error);
    
    /**
     * Display info message (line 4)
     * @param info Info message (max 20 characters)
     */
    void showInfo(const char* info);
    
    /**
     * Clear a specific line
     * @param line Line number (0-3)
     */
    void clearLine(uint8_t line);
    
    /**
     * Clear entire display
     */
    void clear();
    
    /**
     * Turn display on/off
     * @param enabled true to turn on, false to turn off
     */
    void setEnabled(bool enabled);
    
    /**
     * Set backlight on/off
     * @param enabled true to turn on backlight, false to turn off
     */
    void setBacklight(bool enabled);
    
    /**
     * Display startup message
     */
    void showStartupMessage();
    
    /**
     * Display connecting message
     */
    void showConnectingMessage();
    
    /**
     * Display calibration message
     * @param step Calibration step description
     */
    void showCalibrationMessage(const char* step);
    
    /**
     * Get display enabled status
     * @return true if display is enabled
     */
    bool isEnabled() const { return displayEnabled; }
    
    /**
     * Get backlight enabled status
     * @return true if backlight is enabled
     */
    bool isBacklightEnabled() const { return backlightEnabled; }

private:
    uint8_t i2cAddress;             // I2C address
    uint8_t columns;                // Number of columns
    uint8_t rows;                   // Number of rows
    bool initialized;               // Initialization status
    bool displayEnabled;            // Display enabled state
    bool backlightEnabled;          // Backlight enabled state
    unsigned long lastUpdate;       // Last update timestamp
    
    // Current display content for change detection
    String line1Content;
    String line2Content;
    String line3Content;
    String line4Content;
    
    // LCD command constants for HD44780 controller
    static const uint8_t LCD_CLEARDISPLAY = 0x01;
    static const uint8_t LCD_RETURNHOME = 0x02;
    static const uint8_t LCD_ENTRYMODESET = 0x04;
    static const uint8_t LCD_DISPLAYCONTROL = 0x08;
    static const uint8_t LCD_CURSORSHIFT = 0x10;
    static const uint8_t LCD_FUNCTIONSET = 0x20;
    static const uint8_t LCD_SETCGRAMADDR = 0x40;
    static const uint8_t LCD_SETDDRAMADDR = 0x80;
    
    // Flags for display entry mode
    static const uint8_t LCD_ENTRYRIGHT = 0x00;
    static const uint8_t LCD_ENTRYLEFT = 0x02;
    static const uint8_t LCD_ENTRYSHIFTINCREMENT = 0x01;
    static const uint8_t LCD_ENTRYSHIFTDECREMENT = 0x00;
    
    // Flags for display on/off control
    static const uint8_t LCD_DISPLAYON = 0x04;
    static const uint8_t LCD_DISPLAYOFF = 0x00;
    static const uint8_t LCD_CURSORON = 0x02;
    static const uint8_t LCD_CURSOROFF = 0x00;
    static const uint8_t LCD_BLINKON = 0x01;
    static const uint8_t LCD_BLINKOFF = 0x00;
    
    // Flags for function set
    static const uint8_t LCD_8BITMODE = 0x10;
    static const uint8_t LCD_4BITMODE = 0x00;
    static const uint8_t LCD_2LINE = 0x08;
    static const uint8_t LCD_1LINE = 0x00;
    static const uint8_t LCD_5x10DOTS = 0x04;
    static const uint8_t LCD_5x8DOTS = 0x00;
    
    // Flags for backlight control
    static const uint8_t LCD_BACKLIGHT = 0x08;
    static const uint8_t LCD_NOBACKLIGHT = 0x00;
    
    static const uint8_t En = 0b00000100;  // Enable bit
    static const uint8_t Rw = 0b00000010;  // Read/Write bit
    static const uint8_t Rs = 0b00000001;  // Register select bit
    
    /**
     * Check if I2C device is present at address
     * @param address I2C address to check
     * @return true if device responds
     */
    bool checkI2CDevice(uint8_t address);
    
    /**
     * Send data to LCD via I2C
     * @param data Data byte to send
     */
    void i2cWrite(uint8_t data);
    
    /**
     * Send command to LCD
     * @param command Command byte
     */
    void sendCommand(uint8_t command);
    
    /**
     * Send data to LCD
     * @param data Data byte
     */
    void sendData(uint8_t data);
    
    /**
     * Write 4 bits to LCD
     * @param data 4-bit data
     */
    void write4bits(uint8_t data);
    
    /**
     * Pulse enable pin
     * @param data Data with enable bit
     */
    void pulseEnable(uint8_t data);
    
    /**
     * Format uptime string
     * @param uptime Uptime in seconds
     * @return Formatted uptime string
     */
    String formatUptime(unsigned long uptime);
    
    /**
     * Format uptime as decimal hours with one decimal precision
     * @param uptime Uptime in seconds
     * @return Formatted uptime string (e.g., "123.4h")
     */
    String formatUptimeDecimal(unsigned long uptime);
    
    /**
     * Format system state string
     * @param state System state value
     * @return State description string
     */
    String formatSystemState(uint8_t state);
    
    /**
     * Update line if content changed
     * @param line Line number (0-3)
     * @param content New content
     * @param currentContent Current content reference
     */
    void updateLineIfChanged(uint8_t line, const String& content, String& currentContent);
    
    /**
     * Set cursor to beginning of line
     * @param line Line number (0-3)
     */
    void setCursorToLine(uint8_t line);
    
    /**
     * Print string to current cursor position
     * @param str String to print
     */
    void printString(const String& str);
};

#endif // LCD_DISPLAY_H