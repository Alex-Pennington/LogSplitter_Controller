#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include <WiFiS3.h>
#include <WiFiUdp.h>

// TFTP Constants
#define TFTP_DEFAULT_PORT 69
#define TFTP_DATA_SIZE 512      // Standard TFTP data block size
#define TFTP_TIMEOUT_MS 5000    // 5 second timeout
#define FIRMWARE_BUFFER_SIZE 8192    // 8KB buffer for streaming - R4 WiFi has limited RAM

// TFTP Opcodes
#define TFTP_RRQ      1   // Read Request
#define TFTP_WRQ      2   // Write Request  
#define TFTP_DATA     3   // Data packet
#define TFTP_ACK      4   // Acknowledgment
#define TFTP_ERROR_OP 5   // Error packet

// TFTP Error Codes
#define TFTP_ERR_NOT_DEFINED     0
#define TFTP_ERR_FILE_NOT_FOUND  1
#define TFTP_ERR_ACCESS_VIOLATION 2
#define TFTP_ERR_DISK_FULL       3
#define TFTP_ERR_ILLEGAL_OP      4
#define TFTP_ERR_UNKNOWN_TID     5
#define TFTP_ERR_FILE_EXISTS     6
#define TFTP_ERR_NO_SUCH_USER    7

class TFTPServer {
public:
    enum TFTPStatus {
        TFTP_IDLE,
        TFTP_RECEIVING,
        TFTP_VALIDATING,
        TFTP_FLASHING,
        TFTP_SUCCESS,
        TFTP_ERROR_STATE
    };

    TFTPServer();
    
    void begin(uint16_t port = TFTP_DEFAULT_PORT);
    void update();
    void stop();
    
    bool isUpdateInProgress() const;
    bool isServerRunning() const;
    float getUpdateProgress() const;
    TFTPStatus getStatus() const;
    const char* getStatusString() const;
    const char* getLastError() const;
    
private:
    WiFiUDP udp;
    bool serverRunning;
    uint16_t serverPort;
    
    TFTPStatus status;
    size_t expectedSize;
    size_t receivedSize;
    uint16_t currentBlock;
    uint16_t clientPort;
    IPAddress clientIP;
    char lastError[100];
    uint8_t* firmwareBuffer;
    
    void handlePacket();
    void handleWriteRequest(uint8_t* packet, int packetSize, IPAddress clientIP, uint16_t clientPort);
    void handleDataPacket(uint8_t* packet, int packetSize, IPAddress clientIP, uint16_t clientPort);
    void sendAck(uint16_t blockNum, IPAddress clientIP, uint16_t clientPort);
    void sendError(uint16_t errorCode, const char* errorMsg, IPAddress clientIP, uint16_t clientPort);
    bool validateFirmware(uint8_t* data, size_t size);
    bool flashFirmware(uint8_t* data, size_t size);
    void reset();
    
    // Utility functions for TFTP packet handling
    uint16_t getOpcode(uint8_t* packet);
    uint16_t getBlockNumber(uint8_t* packet);
    void setOpcode(uint8_t* packet, uint16_t opcode);
    void setBlockNumber(uint8_t* packet, uint16_t blockNum);
};

// Global TFTP server instance
extern TFTPServer tftpServer;

#endif // TFTP_SERVER_H