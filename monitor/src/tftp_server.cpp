#include "tftp_server.h"
#include <Arduino.h>

extern void debugPrintf(const char* fmt, ...);

TFTPServer::TFTPServer() : 
    running(false),
    transferActive(false),
    clientPort(0),
    currentBlock(0),
    lastPacketTime(0),
    retryCount(0),
    firmwareBuffer(nullptr),
    firmwareSize(0),
    firmwareReceived(0) {
    
    strcpy(lastError, "None");
}

bool TFTPServer::begin() {
    if (running) {
        debugPrintf("TFTP: Server already running\n");
        return true;
    }
    
    // Allocate firmware buffer (small size for R4 WiFi's 32KB RAM)
    // Use 8KB buffer for streaming approach
    firmwareBuffer = (uint8_t*)malloc(8192);
    if (!firmwareBuffer) {
        snprintf(lastError, sizeof(lastError), "Failed to allocate buffer");
        debugPrintf("TFTP: %s\n", lastError);
        return false;
    }
    
    if (udp.begin(TFTP_PORT)) {
        running = true;
        resetTransfer();
        debugPrintf("TFTP: Server started on port %d\n", TFTP_PORT);
        return true;
    } else {
        free(firmwareBuffer);
        firmwareBuffer = nullptr;
        snprintf(lastError, sizeof(lastError), "Failed to bind to port %d", TFTP_PORT);
        debugPrintf("TFTP: %s\n", lastError);
        return false;
    }
}

void TFTPServer::stop() {
    if (!running) {
        return;
    }
    
    udp.stop();
    running = false;
    resetTransfer();
    
    if (firmwareBuffer) {
        free(firmwareBuffer);
        firmwareBuffer = nullptr;
    }
    
    debugPrintf("TFTP: Server stopped\n");
}

void TFTPServer::update() {
    if (!running) {
        return;
    }
    
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        handlePacket();
    }
    
    // Check for transfer timeout
    if (transferActive && (millis() - lastPacketTime > TFTP_TIMEOUT_MS)) {
        debugPrintf("TFTP: Transfer timeout\n");
        sendError(TFTP_ERROR_NOT_DEFINED, "Transfer timeout");
        resetTransfer();
    }
}

void TFTPServer::handlePacket() {
    uint8_t packet[TFTP_BUFFER_SIZE];
    int len = udp.read(packet, sizeof(packet));
    
    if (len < 2) {
        debugPrintf("TFTP: Invalid packet size: %d\n", len);
        return;
    }
    
    uint16_t opcode = getOpcode(packet);
    
    // Store client info for responses
    clientIP = udp.remoteIP();
    clientPort = udp.remotePort();
    
    debugPrintf("TFTP: Received opcode %d from %s:%d\n", 
                opcode, clientIP.toString().c_str(), clientPort);
    
    switch (opcode) {
        case TFTP_OPCODE_WRQ:  // Write Request (client wants to upload)
            handleWriteRequest(packet, len);
            break;
            
        case TFTP_OPCODE_DATA:  // Data packet
            handleData(packet, len);
            break;
            
        case TFTP_OPCODE_RRQ:   // Read Request (not supported)
            sendError(TFTP_ERROR_ILLEGAL_OP, "Read not supported");
            break;
            
        default:
            debugPrintf("TFTP: Unsupported opcode: %d\n", opcode);
            sendError(TFTP_ERROR_ILLEGAL_OP, "Unsupported operation");
            break;
    }
}

void TFTPServer::handleWriteRequest(uint8_t* packet, size_t length) {
    // Parse filename from WRQ packet
    // Format: [2 bytes opcode][filename][0][mode][0]
    char* filename = (char*)(packet + 2);
    
    debugPrintf("TFTP: Write request for file: %s\n", filename);
    
    // For firmware updates, accept "firmware.bin" or similar
    if (strstr(filename, "firmware") == nullptr && 
        strstr(filename, ".bin") == nullptr) {
        sendError(TFTP_ERROR_FILE_NOT_FOUND, "Only firmware.bin accepted");
        return;
    }
    
    // Check if already in transfer
    if (transferActive) {
        sendError(TFTP_ERROR_NOT_DEFINED, "Transfer already in progress");
        return;
    }
    
    // Initialize transfer
    transferActive = true;
    currentBlock = 0;
    firmwareReceived = 0;
    lastPacketTime = millis();
    retryCount = 0;
    
    debugPrintf("TFTP: Starting firmware transfer\n");
    
    // Send ACK 0 to accept the write request
    sendAck(0);
}

void TFTPServer::handleData(uint8_t* packet, size_t length) {
    if (!transferActive) {
        sendError(TFTP_ERROR_NOT_DEFINED, "No active transfer");
        return;
    }
    
    if (length < 4) {
        sendError(TFTP_ERROR_NOT_DEFINED, "Invalid data packet");
        resetTransfer();
        return;
    }
    
    uint16_t blockNum = getBlockNum(packet);
    size_t dataLen = length - 4;  // Subtract 4-byte header
    
    debugPrintf("TFTP: Received block %d, size %d\n", blockNum, dataLen);
    
    // Check if this is the expected block
    if (blockNum != currentBlock + 1) {
        debugPrintf("TFTP: Unexpected block number: %d (expected %d)\n", 
                    blockNum, currentBlock + 1);
        // Resend ACK for current block
        sendAck(currentBlock);
        return;
    }
    
    // Process the data
    // For now, we'll just accumulate bytes received
    // In a full implementation, this would write to flash memory
    firmwareReceived += dataLen;
    
    // Update state
    currentBlock = blockNum;
    lastPacketTime = millis();
    retryCount = 0;
    
    // Send ACK for this block
    sendAck(blockNum);
    
    // Check if transfer is complete (last block is < 512 bytes)
    if (dataLen < TFTP_BLOCK_SIZE) {
        debugPrintf("TFTP: Transfer complete. Received %d bytes\n", firmwareReceived);
        
        // For R4 WiFi, we can't actually flash firmware from code
        // This would require a bootloader or external tool
        snprintf(lastError, sizeof(lastError), 
                 "Transfer complete: %d bytes (flashing not supported)", 
                 firmwareReceived);
        
        resetTransfer();
    }
}

void TFTPServer::sendAck(uint16_t blockNum) {
    uint8_t ackPacket[4];
    ackPacket[0] = 0;
    ackPacket[1] = TFTP_OPCODE_ACK;
    ackPacket[2] = (blockNum >> 8) & 0xFF;
    ackPacket[3] = blockNum & 0xFF;
    
    udp.beginPacket(clientIP, clientPort);
    udp.write(ackPacket, 4);
    udp.endPacket();
    
    debugPrintf("TFTP: Sent ACK for block %d\n", blockNum);
}

void TFTPServer::sendError(uint16_t errorCode, const char* errorMsg) {
    size_t msgLen = strlen(errorMsg);
    size_t packetLen = 4 + msgLen + 1;  // opcode + error code + message + null
    uint8_t* errorPacket = (uint8_t*)malloc(packetLen);
    
    if (!errorPacket) {
        debugPrintf("TFTP: Failed to allocate error packet\n");
        return;
    }
    
    errorPacket[0] = 0;
    errorPacket[1] = TFTP_OPCODE_ERROR;
    errorPacket[2] = (errorCode >> 8) & 0xFF;
    errorPacket[3] = errorCode & 0xFF;
    memcpy(errorPacket + 4, errorMsg, msgLen + 1);
    
    udp.beginPacket(clientIP, clientPort);
    udp.write(errorPacket, packetLen);
    udp.endPacket();
    
    free(errorPacket);
    
    snprintf(lastError, sizeof(lastError), "Error %d: %s", errorCode, errorMsg);
    debugPrintf("TFTP: Sent error: %s\n", lastError);
}

void TFTPServer::resetTransfer() {
    transferActive = false;
    currentBlock = 0;
    firmwareReceived = 0;
    clientPort = 0;
    lastPacketTime = 0;
    retryCount = 0;
}

void TFTPServer::getStatus(char* buffer, size_t bufferSize) {
    if (!running) {
        snprintf(buffer, bufferSize, "TFTP: STOPPED");
        return;
    }
    
    if (transferActive) {
        snprintf(buffer, bufferSize, 
                 "TFTP: ACTIVE - Block %d, %d bytes received from %s:%d",
                 currentBlock, firmwareReceived, 
                 clientIP.toString().c_str(), clientPort);
    } else {
        snprintf(buffer, bufferSize, 
                 "TFTP: RUNNING on port %d - Waiting for transfers", TFTP_PORT);
    }
}

uint16_t TFTPServer::getOpcode(uint8_t* packet) {
    return (packet[0] << 8) | packet[1];
}

uint16_t TFTPServer::getBlockNum(uint8_t* packet) {
    return (packet[2] << 8) | packet[3];
}
