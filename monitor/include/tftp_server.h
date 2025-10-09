#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

// TFTP Operation Codes (RFC 1350)
#define TFTP_OPCODE_RRQ   1  // Read Request
#define TFTP_OPCODE_WRQ   2  // Write Request
#define TFTP_OPCODE_DATA  3  // Data
#define TFTP_OPCODE_ACK   4  // Acknowledgment
#define TFTP_OPCODE_ERROR 5  // Error

// TFTP Error Codes
#define TFTP_ERROR_NOT_DEFINED      0
#define TFTP_ERROR_FILE_NOT_FOUND   1
#define TFTP_ERROR_ACCESS_VIOLATION 2
#define TFTP_ERROR_DISK_FULL        3
#define TFTP_ERROR_ILLEGAL_OP       4
#define TFTP_ERROR_UNKNOWN_TID      5
#define TFTP_ERROR_FILE_EXISTS      6
#define TFTP_ERROR_NO_SUCH_USER     7

// TFTP Configuration
#define TFTP_PORT 69
#define TFTP_BLOCK_SIZE 512
#define TFTP_TIMEOUT_MS 5000
#define TFTP_MAX_RETRIES 3
#define TFTP_BUFFER_SIZE 516  // 512 bytes data + 4 bytes header

class TFTPServer {
public:
    TFTPServer();
    
    // Initialize TFTP server
    bool begin();
    
    // Stop TFTP server
    void stop();
    
    // Update/process TFTP requests (call in loop)
    void update();
    
    // Check if server is running
    bool isRunning() const { return running; }
    
    // Get server status
    void getStatus(char* buffer, size_t bufferSize);
    
    // Get last error
    const char* getLastError() const { return lastError; }

private:
    WiFiUDP udp;
    bool running;
    
    // Current transfer state
    bool transferActive;
    IPAddress clientIP;
    uint16_t clientPort;
    uint16_t currentBlock;
    unsigned long lastPacketTime;
    uint8_t retryCount;
    
    // Firmware buffer for receiving updates
    uint8_t* firmwareBuffer;
    size_t firmwareSize;
    size_t firmwareReceived;
    const size_t maxFirmwareSize = 256 * 1024; // 256KB max
    
    // Error tracking
    char lastError[64];
    
    // Helper methods
    void handlePacket();
    void handleWriteRequest(uint8_t* packet, size_t length);
    void handleData(uint8_t* packet, size_t length);
    void sendAck(uint16_t blockNum);
    void sendError(uint16_t errorCode, const char* errorMsg);
    void resetTransfer();
    bool validateFirmware();
    bool flashFirmware();
    
    // Parse TFTP packet fields
    uint16_t getOpcode(uint8_t* packet);
    uint16_t getBlockNum(uint8_t* packet);
};
