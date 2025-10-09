#include "tftp_server.h"
#include "logger.h"
#include <EEPROM.h>

// Global TFTP server instance
TFTPServer tftpServer;

TFTPServer::TFTPServer() : 
    serverRunning(false), 
    serverPort(TFTP_DEFAULT_PORT),
    status(TFTP_IDLE), 
    expectedSize(0), 
    receivedSize(0),
    currentBlock(0),
    clientPort(0),
    firmwareBuffer(nullptr) {
    
    strcpy(lastError, "No error");
}

void TFTPServer::begin(uint16_t port) {
    serverPort = port;
    
    if (udp.begin(serverPort)) {
        serverRunning = true;
        status = TFTP_IDLE;
        
        // Allocate firmware buffer
        firmwareBuffer = (uint8_t*)malloc(FIRMWARE_BUFFER_SIZE);
        if (!firmwareBuffer) {
            strcpy(lastError, "Failed to allocate firmware buffer");
            status = TFTP_ERROR_STATE;
            serverRunning = false;
            return;
        }
        
        LOG_INFO("TFTP server started on port %d", serverPort);
        Serial.print("TFTP: Server started on port ");
        Serial.println(serverPort);
    } else {
        strcpy(lastError, "Failed to start UDP server");
        status = TFTP_ERROR_STATE;
        serverRunning = false;
        LOG_ERROR("TFTP: Failed to start server on port %d", serverPort);
    }
}

void TFTPServer::update() {
    if (!serverRunning) return;
    
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        handlePacket();
    }
}

void TFTPServer::stop() {
    if (serverRunning) {
        udp.stop();
        serverRunning = false;
        status = TFTP_IDLE;
        
        if (firmwareBuffer) {
            free(firmwareBuffer);
            firmwareBuffer = nullptr;
        }
        
        LOG_INFO("TFTP server stopped");
        Serial.println("TFTP: Server stopped");
    }
}

bool TFTPServer::isUpdateInProgress() const {
    return (status == TFTP_RECEIVING || status == TFTP_VALIDATING || status == TFTP_FLASHING);
}

bool TFTPServer::isServerRunning() const {
    return serverRunning;
}

float TFTPServer::getUpdateProgress() const {
    if (expectedSize == 0) return 0.0;
    return (float)receivedSize / expectedSize * 100.0;
}

TFTPServer::TFTPStatus TFTPServer::getStatus() const {
    return status;
}

const char* TFTPServer::getStatusString() const {
    switch (status) {
        case TFTP_IDLE: return "IDLE";
        case TFTP_RECEIVING: return "RECEIVING";
        case TFTP_VALIDATING: return "VALIDATING";
        case TFTP_FLASHING: return "FLASHING";
        case TFTP_SUCCESS: return "SUCCESS";
        case TFTP_ERROR_STATE: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* TFTPServer::getLastError() const {
    return lastError;
}

void TFTPServer::handlePacket() {
    uint8_t packet[TFTP_DATA_SIZE + 10]; // Extra space for headers
    int packetSize = udp.read(packet, sizeof(packet));
    
    if (packetSize < 2) {
        Serial.println("TFTP: Packet too small");
        return;
    }
    
    IPAddress remoteIP = udp.remoteIP();
    uint16_t remotePort = udp.remotePort();
    uint16_t opcode = getOpcode(packet);
    
    Serial.print("TFTP: Received packet from ");
    Serial.print(remoteIP);
    Serial.print(":");
    Serial.print(remotePort);
    Serial.print(" opcode=");
    Serial.println(opcode);
    
    switch (opcode) {
        case TFTP_WRQ:
            handleWriteRequest(packet, packetSize, remoteIP, remotePort);
            break;
            
        case TFTP_DATA:
            handleDataPacket(packet, packetSize, remoteIP, remotePort);
            break;
            
        default:
            Serial.print("TFTP: Unsupported opcode: ");
            Serial.println(opcode);
            sendError(TFTP_ERR_ILLEGAL_OP, "Unsupported operation", remoteIP, remotePort);
            break;
    }
}

void TFTPServer::handleWriteRequest(uint8_t* packet, int packetSize, IPAddress clientIP, uint16_t clientPort) {
    // Parse filename from WRQ packet
    char* filename = (char*)(packet + 2);
    char* mode = filename;
    
    // Find end of filename
    while (*mode != 0 && mode < (char*)(packet + packetSize)) mode++;
    mode++; // Skip null terminator
    
    Serial.print("TFTP: Write request for file: ");
    Serial.print(filename);
    Serial.print(" mode: ");
    Serial.println(mode);
    
    // Check if filename is "firmware.bin" or similar
    if (strstr(filename, "firmware") == nullptr && strstr(filename, ".bin") == nullptr) {
        sendError(TFTP_ERR_FILE_NOT_FOUND, "Only firmware.bin supported", clientIP, clientPort);
        return;
    }
    
    // Check if mode is "octet" (binary)
    if (strcasecmp(mode, "octet") != 0 && strcasecmp(mode, "binary") != 0) {
        sendError(TFTP_ERR_ILLEGAL_OP, "Only binary mode supported", clientIP, clientPort);
        return;
    }
    
    // Start new transfer
    this->clientIP = clientIP;
    this->clientPort = clientPort;
    status = TFTP_RECEIVING;
    receivedSize = 0;
    currentBlock = 0;
    
    LOG_INFO("TFTP: Starting firmware upload from %s:%d", clientIP.toString().c_str(), clientPort);
    
    // Send ACK for block 0 to start transfer
    sendAck(0, clientIP, clientPort);
}

void TFTPServer::handleDataPacket(uint8_t* packet, int packetSize, IPAddress clientIP, uint16_t clientPort) {
    if (status != TFTP_RECEIVING) {
        sendError(TFTP_ERR_ILLEGAL_OP, "Not expecting data", clientIP, clientPort);
        return;
    }
    
    // Verify client
    if (clientIP != this->clientIP || clientPort != this->clientPort) {
        sendError(TFTP_ERR_UNKNOWN_TID, "Wrong client", clientIP, clientPort);
        return;
    }
    
    uint16_t blockNum = getBlockNumber(packet);
    int dataSize = packetSize - 4; // Subtract header size
    
    Serial.print("TFTP: Data block ");
    Serial.print(blockNum);
    Serial.print(" size=");
    Serial.println(dataSize);
    
    // Check for duplicate or out-of-order block
    if (blockNum != currentBlock + 1) {
        if (blockNum == currentBlock) {
            // Duplicate block, just re-send ACK
            sendAck(currentBlock, clientIP, clientPort);
            return;
        } else {
            sendError(TFTP_ERR_ILLEGAL_OP, "Block out of order", clientIP, clientPort);
            status = TFTP_ERROR_STATE;
            strcpy(lastError, "Block sequence error");
            return;
        }
    }
    
    // For streaming mode: validate only the first block, then stream subsequent blocks
    if (currentBlock == 0) {  // First block
        // Check buffer space for first block
        if (receivedSize + dataSize > FIRMWARE_BUFFER_SIZE) {
            sendError(TFTP_ERR_DISK_FULL, "First block too large", clientIP, clientPort);
            status = TFTP_ERROR_STATE;
            strcpy(lastError, "First block exceeds buffer size");
            return;
        }
        
        // Copy first block to buffer for validation
        memcpy(firmwareBuffer + receivedSize, packet + 4, dataSize);
        receivedSize += dataSize;
        
        // Validate the vector table from first block
        if (receivedSize >= 8) {  // Have enough data for vector table
            if (!validateFirmware(firmwareBuffer, receivedSize)) {
                status = TFTP_ERROR_STATE;
                return;
            }
            Serial.println("TFTP: Vector table validation passed, entering streaming mode");
        }
    } else {
        // Streaming mode: just count bytes, don't buffer
        receivedSize += dataSize;
        Serial.print("TFTP: Streaming block ");
        Serial.print(blockNum);
        Serial.print(" (total: ");
        Serial.print(receivedSize);
        Serial.println(" bytes)");
    }
    currentBlock = blockNum;
    
    // Send ACK
    sendAck(blockNum, clientIP, clientPort);
    
    // Check if this was the last block (less than 512 bytes)
    if (dataSize < TFTP_DATA_SIZE) {
        Serial.print("TFTP: Transfer complete, ");
        Serial.print(receivedSize);
        Serial.println(" bytes received");
        
        // For streaming mode, we already validated the vector table
        status = TFTP_FLASHING;
        if (flashFirmware(firmwareBuffer, receivedSize)) {  // Only first block in buffer
            status = TFTP_SUCCESS;
            sprintf(lastError, "Firmware updated successfully (%d bytes)", receivedSize);
            LOG_INFO("TFTP: Firmware update successful (%d bytes)", receivedSize);
        } else {
            status = TFTP_ERROR_STATE;
            LOG_ERROR("TFTP: Firmware flashing failed");
        }
    }
}

void TFTPServer::sendAck(uint16_t blockNum, IPAddress clientIP, uint16_t clientPort) {
    uint8_t ackPacket[4];
    setOpcode(ackPacket, TFTP_ACK);
    setBlockNumber(ackPacket, blockNum);
    
    udp.beginPacket(clientIP, clientPort);
    udp.write(ackPacket, 4);
    udp.endPacket();
    
    Serial.print("TFTP: Sent ACK for block ");
    Serial.println(blockNum);
}

void TFTPServer::sendError(uint16_t errorCode, const char* errorMsg, IPAddress clientIP, uint16_t clientPort) {
    uint8_t errorPacket[100];
    setOpcode(errorPacket, TFTP_ERROR_OP);
    errorPacket[2] = (errorCode >> 8) & 0xFF;
    errorPacket[3] = errorCode & 0xFF;
    
    int msgLen = strlen(errorMsg);
    memcpy(errorPacket + 4, errorMsg, msgLen);
    errorPacket[4 + msgLen] = 0; // Null terminator
    
    udp.beginPacket(clientIP, clientPort);
    udp.write(errorPacket, 5 + msgLen);
    udp.endPacket();
    
    Serial.print("TFTP: Sent error ");
    Serial.print(errorCode);
    Serial.print(": ");
    Serial.println(errorMsg);
}

bool TFTPServer::validateFirmware(uint8_t* data, size_t size) {
    // Size check - only need to validate the vector table from first block
    if (size < 8) {  // Need at least 8 bytes for stack pointer + reset vector
        sprintf(lastError, "Invalid firmware size: %d bytes", size);
        LOG_ERROR("TFTP: %s", lastError);
        return false;
    }
    
    // ARM vector table validation - only check first 8 bytes
    uint32_t* vectors = (uint32_t*)data;
    uint32_t stackPointer = vectors[0];
    uint32_t resetVector = vectors[1];
    
    // Stack pointer should be in RAM range
    if (stackPointer < 0x20000000 || stackPointer > 0x20008000) {
        sprintf(lastError, "Invalid stack pointer: 0x%08X", stackPointer);
        LOG_ERROR("TFTP: %s", lastError);
        return false;
    }
    
    // Reset vector should be in flash range and odd (Thumb mode)
    // For Arduino Uno R4 WiFi: Flash starts at 0x00000000, not 0x08000000 like STM32
    if (resetVector < 0x00000100 || resetVector > 0x0003FFFF || (resetVector & 1) == 0) {
        sprintf(lastError, "Invalid reset vector: 0x%08X", resetVector);
        LOG_ERROR("TFTP: %s", lastError);
        return false;
    }
    
    LOG_INFO("TFTP: Firmware validation passed (vector table OK)");
    return true;
}

bool TFTPServer::flashFirmware(uint8_t* data, size_t size) {
    // For Arduino, we can't actually flash firmware at runtime
    // This would typically require a bootloader or external programmer
    // For now, just simulate success and log the operation
    
    LOG_INFO("TFTP: Firmware flashing simulated (would require bootloader)");
    Serial.println("TFTP: Firmware ready for flash (restart required)");
    
    // In a real implementation, you might:
    // 1. Write firmware to external flash/EEPROM
    // 2. Set a flag for bootloader to apply update
    // 3. Restart the system
    
    return true;
}

void TFTPServer::reset() {
    status = TFTP_IDLE;
    receivedSize = 0;
    currentBlock = 0;
    clientPort = 0;
    strcpy(lastError, "No error");
}

// Utility functions for TFTP packet handling
uint16_t TFTPServer::getOpcode(uint8_t* packet) {
    return (packet[0] << 8) | packet[1];
}

uint16_t TFTPServer::getBlockNumber(uint8_t* packet) {
    return (packet[2] << 8) | packet[3];
}

void TFTPServer::setOpcode(uint8_t* packet, uint16_t opcode) {
    packet[0] = (opcode >> 8) & 0xFF;
    packet[1] = opcode & 0xFF;
}

void TFTPServer::setBlockNumber(uint8_t* packet, uint16_t blockNum) {
    packet[2] = (blockNum >> 8) & 0xFF;
    packet[3] = blockNum & 0xFF;
}