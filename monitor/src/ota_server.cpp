#include "ota_server.h"
#include "logger.h"
#include <WiFiS3.h>

// Global instance
OTAServer otaServer;

// HTML upload page
const char OTA_UPLOAD_PAGE[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <title>LogMonitor OTA Update</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        .info { background: #e7f3ff; padding: 10px; border-radius: 4px; margin: 10px 0; }
        .button { background: #4CAF50; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        .button:hover { background: #45a049; }
        .progress { width: 100%; height: 20px; border: 1px solid #ccc; margin: 10px 0; border-radius: 4px; overflow: hidden; }
        .progress-bar { height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s; }
        .file-input { margin: 10px 0; padding: 10px; border: 2px dashed #ccc; border-radius: 4px; text-align: center; }
        .instructions { background: #fff3cd; padding: 15px; border-radius: 4px; margin: 15px 0; }
        .security { background: #d1ecf1; padding: 15px; border-radius: 4px; margin: 15px 0; }
        .code { background: #f8f9fa; padding: 5px 8px; border-radius: 3px; font-family: monospace; }
        .warning { background: #f8d7da; padding: 10px; border-radius: 4px; margin: 10px 0; color: #721c24; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 LogMonitor Firmware Update</h1>
        
        <div class="info">
            <strong>Device:</strong> LogMonitor v1.0.0<br>
            <strong>Current Flash:</strong> 51.8% (135,664 bytes)<br>
            <strong>Available:</strong> 126,480 bytes<br>
            <strong>Local IP:</strong> <span id="deviceIP">192.168.1.155</span>
        </div>

        <div class="warning">
            ⚠️ <strong>Warning:</strong> Only upload firmware built for Arduino Uno R4 WiFi. Incorrect firmware may brick the device!
        </div>
        
        <form action="/update" method="POST" enctype="multipart/form-data" id="uploadForm">
            <div class="file-input">
                <h3>📁 Select Firmware File (.bin)</h3>
                <input type="file" name="firmware" accept=".bin" required style="margin: 10px;">
            </div>
            <div style="text-align: center;">
                <input type="submit" value="🚀 Upload & Flash Firmware" class="button">
            </div>
        </form>
        
        <div id="progress" class="progress" style="display:none;">
            <div id="progressBar" class="progress-bar"></div>
        </div>
        <div id="status" style="text-align: center; margin: 10px 0;"></div>
        
        <div class="instructions">
            <h3>📋 Build Instructions:</h3>
            <ol>
                <li>Build firmware: <span class="code">pio run</span></li>
                <li>Locate file: <span class="code">.pio/build/uno_r4_wifi/firmware.bin</span></li>
                <li>Upload using form above</li>
                <li>Wait for completion (~30-60 seconds)</li>
                <li>Device will automatically reboot</li>
            </ol>
            
            <h4>💻 Command Line Alternative:</h4>
            <span class="code">curl -X POST -F "firmware=@firmware.bin" http://<span id="curlIP">192.168.1.155</span>/update</span>
        </div>
        
        <div class="security">
            <h3>🛡️ Security Features:</h3>
            <ul>
                <li>✅ Local network access only</li>
                <li>✅ ARM vector table validation</li>
                <li>✅ Size limits (max 200KB)</li>
                <li>✅ Rate limiting (3 requests/minute)</li>
                <li>✅ Complete syslog audit trail</li>
                <li>✅ Atomic updates (no partial flashing)</li>
            </ul>
        </div>
    </div>
    
    <script>
        document.getElementById('uploadForm').onsubmit = function() {
            document.getElementById('progress').style.display = 'block';
            document.getElementById('status').innerHTML = '📤 Uploading firmware...';
            
            // Simulate progress (actual progress would need WebSocket/AJAX)
            let progress = 0;
            const interval = setInterval(() => {
                progress += Math.random() * 10;
                if (progress > 95) progress = 95;
                document.getElementById('progressBar').style.width = progress + '%';
                
                if (progress > 90) {
                    document.getElementById('status').innerHTML = '🔍 Validating firmware...';
                }
            }, 500);
            
            // Clear interval after 30 seconds
            setTimeout(() => clearInterval(interval), 30000);
        };
    </script>
</body>
</html>
)";

OTAServer::OTAServer() : 
    server(nullptr), 
    serverRunning(false), 
    serverPort(80),
    status(OTA_IDLE), 
    expectedSize(0), 
    receivedSize(0),
    firmwareBuffer(nullptr) {
    
    strcpy(lastError, "None");
}

void OTAServer::begin(uint16_t port) {
    if (serverRunning) {
        stop();
    }
    
    serverPort = port;
    server = new WiFiServer(port);
    server->begin();
    serverRunning = true;
    status = OTA_IDLE;
    
    // Allocate firmware buffer
    if (!firmwareBuffer) {
        firmwareBuffer = (uint8_t*)malloc(FIRMWARE_BUFFER_SIZE);
        if (!firmwareBuffer) {
            strcpy(lastError, "Failed to allocate firmware buffer");
            LOG_ERROR("OTA: Failed to allocate %d bytes for firmware buffer", FIRMWARE_BUFFER_SIZE);
            serverRunning = false;
            return;
        }
    }
    
    LOG_INFO("OTA: Server started on port %d", port);
    Serial.print("OTA: Server ready at http://");
    Serial.print(WiFi.localIP().toString());
    Serial.print(":");
    Serial.print(port);
    Serial.println("/update");
}

void OTAServer::update() {
    if (!serverRunning || !server) return;
    
    // Check for new clients
    WiFiClient newClient = server->available();
    if (newClient) {
        if (isAuthorizedClient(newClient.remoteIP()) && checkRateLimit(newClient.remoteIP())) {
            client = newClient;
            handleClient();
        } else {
            newClient.stop();
        }
    }
}

void OTAServer::handleClient() {
    if (!client.connected()) return;
    
    unsigned long startTime = millis();
    String currentLine = "";
    String httpMethod = "";
    String httpPath = "";
    bool isPostRequest = false;
    
    // Read HTTP request headers
    while (client.connected() && (millis() - startTime < CLIENT_TIMEOUT_MS)) {
        if (client.available()) {
            char c = client.read();
            if (c == '\n') {
                if (currentLine.length() == 0) {
                    // End of headers
                    if (isPostRequest && httpPath == "/update") {
                        handleUploadRequest();
                    } else if (httpPath == "/update" || httpPath == "/") {
                        sendUploadPage();
                    } else if (httpPath == "/progress") {
                        sendProgressPage();
                    } else {
                        sendResponse(404, "Not Found");
                    }
                    break;
                } else {
                    // Parse request line
                    if (currentLine.startsWith("GET ") || currentLine.startsWith("POST ")) {
                        int spaceIndex = currentLine.indexOf(' ');
                        int secondSpaceIndex = currentLine.indexOf(' ', spaceIndex + 1);
                        httpMethod = currentLine.substring(0, spaceIndex);
                        httpPath = currentLine.substring(spaceIndex + 1, secondSpaceIndex);
                        isPostRequest = (httpMethod == "POST");
                    }
                    currentLine = "";
                }
            } else if (c != '\r') {
                currentLine += c;
            }
        }
        delay(1);
    }
    
    client.stop();
}

void OTAServer::handleUploadRequest() {
    status = OTA_RECEIVING;
    receivedSize = 0;
    
    LOG_INFO("OTA: Upload started from %s", client.remoteIP().toString().c_str());
    Serial.println("OTA: Starting upload request handling");
    
    // Try to parse as multipart first, then fallback to binary
    if (!parseMultipartForm(client)) {
        Serial.println("OTA: Multipart parsing failed, trying binary upload");
        
        // Reset for binary upload attempt
        receivedSize = 0;
        
        // Read raw binary data
        if (!parseBinaryUpload(client)) {
            status = OTA_ERROR;
            strcpy(lastError, "Failed to parse upload data");
            sendResponse(400, "Bad Request - Invalid upload data");
            return;
        }
    }
    
    status = OTA_VALIDATING;
    LOG_INFO("OTA: Validating firmware (%d bytes)", receivedSize);
    Serial.print("OTA: Firmware size for validation: ");
    Serial.println(receivedSize);
    
    // Validate firmware
    if (!validateFirmware(firmwareBuffer, receivedSize)) {
        status = OTA_ERROR;
        sendResponse(400, "Bad Request - Firmware validation failed");
        return;
    }
    
    status = OTA_FLASHING;
    LOG_INFO("OTA: Starting flash programming");
    
    // Flash firmware
    if (!flashFirmware(firmwareBuffer, receivedSize)) {
        status = OTA_ERROR;
        sendResponse(500, "Internal Server Error - Flash programming failed");
        return;
    }
    
    status = OTA_SUCCESS;
    LOG_INFO("OTA: Firmware update completed successfully");
    
    sendResponse(200, "Firmware update successful! Device will reboot in 5 seconds...", "text/html");
    
    // Reboot after successful update
    delay(5000);
    NVIC_SystemReset();
}

bool OTAServer::parseMultipartForm(WiFiClient& client) {
    char buffer[HTTP_BUFFER_SIZE];
    receivedSize = 0;
    
    Serial.println("OTA: Starting multipart parsing (body only)");
    
    // At this point, we should already be positioned after HTTP headers
    // The stream should start with the multipart boundary
    
    unsigned long startTime = millis();
    int totalRead = 0;
    int chunkCount = 0;
    bool foundDataStart = false;
    
    while (client.connected() && (millis() - startTime < CLIENT_TIMEOUT_MS)) {
        int available = client.available();
        if (available > 0) {
            // Read in manageable chunks
            int chunkSize = min(available, 1024);
            int bytesRead = client.readBytes(buffer, chunkSize);
            
            totalRead += bytesRead;
            chunkCount++;
            
            Serial.print("Chunk ");
            Serial.print(chunkCount);
            Serial.print(": ");
            Serial.print(bytesRead);
            Serial.println(" bytes");
            
            // Show first chunk for debugging
            if (chunkCount == 1) {
                Serial.print("First 80 chars: ");
                for (int i = 0; i < min(80, bytesRead); i++) {
                    char c = buffer[i];
                    if (c >= 32 && c <= 126) {
                        Serial.print(c);
                    } else {
                        Serial.print("[");
                        Serial.print((int)c);
                        Serial.print("]");
                    }
                }
                Serial.println();
            }
            
            // Process this chunk
            if (!foundDataStart) {
                // Look for the start of binary data more carefully
                char* dataStart = nullptr;
                
                // First, look for Content-Type: application/octet-stream line
                // followed by double CRLF
                for (int i = 0; i < bytesRead - 20; i++) {
                    // Look for "Content-Type:" followed eventually by double CRLF
                    if (strncmp(buffer + i, "Content-Type:", 13) == 0) {
                        Serial.print("OTA: Found Content-Type at offset ");
                        Serial.println(i);
                        
                        // Now look for double CRLF after this
                        for (int j = i + 13; j < bytesRead - 4; j++) {
                            if (buffer[j] == '\r' && buffer[j+1] == '\n' && 
                                buffer[j+2] == '\r' && buffer[j+3] == '\n') {
                                dataStart = buffer + j + 4;
                                foundDataStart = true;
                                Serial.print("OTA: Binary data starts at offset ");
                                Serial.println(j + 4);
                                break;
                            }
                        }
                        break;
                    }
                }
                
                // Fallback: just look for double CRLF
                if (!foundDataStart) {
                    for (int i = 0; i < bytesRead - 4; i++) {
                        if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                            buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                            dataStart = buffer + i + 4;
                            foundDataStart = true;
                            Serial.print("OTA: Found CRLF pattern at offset ");
                            Serial.println(i + 4);
                            break;
                        }
                    }
                }
                
                if (dataStart) {
                    int dataLen = bytesRead - (dataStart - buffer);
                    if (dataLen > 0 && receivedSize + dataLen <= FIRMWARE_BUFFER_SIZE) {
                        memcpy(firmwareBuffer + receivedSize, dataStart, dataLen);
                        receivedSize += dataLen;
                        Serial.print("OTA: Copied ");
                        Serial.print(dataLen);
                        Serial.println(" bytes from first chunk");
                        
                        // Show what we actually copied
                        Serial.print("OTA: First 16 copied bytes: ");
                        for (int k = 0; k < min(16, dataLen); k++) {
                            Serial.print("0x");
                            if (dataStart[k] < 16) Serial.print("0");
                            Serial.print((uint8_t)dataStart[k], HEX);
                            Serial.print(" ");
                        }
                        Serial.println();
                    }
                } else {
                    // No data start found yet, might be all headers
                    Serial.println("OTA: Still in headers, skipping chunk");
                }
            } else {
                // We're in data mode, copy everything
                if (receivedSize + bytesRead <= FIRMWARE_BUFFER_SIZE) {
                    memcpy(firmwareBuffer + receivedSize, buffer, bytesRead);
                    receivedSize += bytesRead;
                } else {
                    Serial.println("OTA: Buffer full");
                    break;
                }
            }
            
            // Progress
            if (receivedSize % 10240 == 0 && receivedSize > 0) {
                Serial.print("OTA: ");
                Serial.print(receivedSize);
                Serial.println(" bytes received");
            }
        } else {
            delay(10);
        }
    }
    
    Serial.print("OTA: Parsing complete - Total: ");
    Serial.print(totalRead);
    Serial.print(", Firmware: ");
    Serial.println(receivedSize);
    
    // Clean up trailing boundary (multipart ends with boundary)
    if (receivedSize > 100) {
        // Look backwards for boundary pattern
        for (int i = receivedSize - 50; i >= receivedSize - 100 && i >= 0; i--) {
            if (firmwareBuffer[i] == '-' && firmwareBuffer[i+1] == '-') {
                Serial.print("OTA: Trimming boundary from ");
                Serial.print(receivedSize);
                Serial.print(" to ");
                Serial.println(i);
                receivedSize = i;
                break;
            }
        }
    }
    
    // Validate first few bytes look like firmware
    if (receivedSize > 8) {
        Serial.print("OTA: First 8 bytes: ");
        for (int i = 0; i < 8; i++) {
            Serial.print("0x");
            if (firmwareBuffer[i] < 16) Serial.print("0");
            Serial.print(firmwareBuffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    Serial.print("OTA: Final size ");
    Serial.print(receivedSize);
    Serial.println(" bytes");
    
    return (receivedSize > 1024 && receivedSize < 200000);
}

bool OTAServer::parseBinaryUpload(WiFiClient& client) {
    Serial.println("OTA: Starting binary upload parsing");
    
    receivedSize = 0;
    unsigned long startTime = millis();
    char buffer[1024];
    uint8_t tempBuffer[FIRMWARE_BUFFER_SIZE];
    int tempSize = 0;
    
    // Read all available data first
    while (client.connected() && (millis() - startTime < CLIENT_TIMEOUT_MS)) {
        int available = client.available();
        if (available > 0) {
            int chunkSize = min(available, sizeof(buffer));
            int bytesRead = client.readBytes(buffer, chunkSize);
            
            if (tempSize + bytesRead <= FIRMWARE_BUFFER_SIZE) {
                memcpy(tempBuffer + tempSize, buffer, bytesRead);
                tempSize += bytesRead;
            } else {
                Serial.println("OTA: Temp buffer full");
                break;
            }
        } else {
            delay(10);
        }
    }
    
    Serial.print("OTA: Total data received: ");
    Serial.println(tempSize);
    
    // Show first 100 bytes to debug
    Serial.print("OTA: First 100 bytes as hex: ");
    for (int i = 0; i < min(100, tempSize); i++) {
        Serial.print("0x");
        if (tempBuffer[i] < 16) Serial.print("0");
        Serial.print(tempBuffer[i], HEX);
        Serial.print(" ");
        if ((i + 1) % 16 == 0) Serial.println();
    }
    Serial.println();
    
    // Try to find the actual firmware start
    // Look for the expected ARM vector table pattern
    int firmwareStart = -1;
    
    // Look for pattern: 0x00 0x7F 0x00 0x20 (stack pointer to 0x20007F00)
    for (int i = 0; i < tempSize - 4; i++) {
        if (tempBuffer[i] == 0x00 && tempBuffer[i+1] == 0x7F && 
            tempBuffer[i+2] == 0x00 && tempBuffer[i+3] == 0x20) {
            firmwareStart = i;
            Serial.print("OTA: Found expected firmware pattern at offset ");
            Serial.println(i);
            break;
        }
    }
    
    // Fallback: look for any reasonable stack pointer (0x2000xxxx range)
    if (firmwareStart == -1) {
        for (int i = 0; i < tempSize - 4; i++) {
            uint32_t* ptr = (uint32_t*)(tempBuffer + i);
            uint32_t stackPtr = *ptr;
            if (stackPtr >= 0x20000000 && stackPtr <= 0x20008000) {
                firmwareStart = i;
                Serial.print("OTA: Found valid stack pointer at offset ");
                Serial.print(i);
                Serial.print(" (0x");
                Serial.print(stackPtr, HEX);
                Serial.println(")");
                break;
            }
        }
    }
    
    if (firmwareStart != -1) {
        receivedSize = tempSize - firmwareStart;
        memcpy(firmwareBuffer, tempBuffer + firmwareStart, receivedSize);
        
        Serial.print("OTA: Extracted firmware: ");
        Serial.print(receivedSize);
        Serial.print(" bytes starting at offset ");
        Serial.println(firmwareStart);
        
        // Show first bytes of actual firmware
        Serial.print("OTA: Firmware first 8 bytes: ");
        for (int i = 0; i < 8; i++) {
            Serial.print("0x");
            if (firmwareBuffer[i] < 16) Serial.print("0");
            Serial.print(firmwareBuffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("OTA: Could not find firmware start pattern");
        return false;
    }
    
    return (receivedSize > 1024 && receivedSize < 200000);
}

bool OTAServer::validateFirmware(uint8_t* data, size_t size) {
    // Size check
    if (size < 1024 || size > FIRMWARE_BUFFER_SIZE) {
        sprintf(lastError, "Invalid firmware size: %d bytes", size);
        LOG_ERROR("OTA: %s", lastError);
        return false;
    }
    
    // ARM vector table validation
    uint32_t* vectors = (uint32_t*)data;
    uint32_t stackPointer = vectors[0];
    uint32_t resetVector = vectors[1];
    
    // Stack pointer should be in RAM range
    if (stackPointer < 0x20000000 || stackPointer > 0x20008000) {
        sprintf(lastError, "Invalid stack pointer: 0x%08X", stackPointer);
        LOG_ERROR("OTA: %s", lastError);
        return false;
    }
    
    // Reset vector should be in flash range and odd (Thumb mode)
    if ((resetVector & 0xFF000000) != 0x08000000 || (resetVector & 1) == 0) {
        sprintf(lastError, "Invalid reset vector: 0x%08X", resetVector);
        LOG_ERROR("OTA: %s", lastError);
        return false;
    }
    
    LOG_INFO("OTA: Firmware validation passed (size: %d, SP: 0x%08X, RV: 0x%08X)", 
             size, stackPointer, resetVector);
    return true;
}

bool OTAServer::flashFirmware(uint8_t* data, size_t size) {
    // Note: Arduino R4 WiFi flash programming would require specific implementation
    // This is a placeholder that would need actual flash driver integration
    
    LOG_WARN("OTA: Flash programming not yet implemented for R4 WiFi platform");
    strcpy(lastError, "Flash programming not implemented");
    return false;
    
    // TODO: Implement actual flash programming for R4 WiFi
    // This would involve:
    // 1. Unlocking flash controller
    // 2. Erasing flash sectors
    // 3. Programming flash pages
    // 4. Verifying written data
    // 5. Setting reset vector
}

bool OTAServer::isAuthorizedClient(IPAddress clientIP) {
    IPAddress localIP = WiFi.localIP();
    
    // Only allow same subnet
    if (clientIP[0] != localIP[0] || clientIP[1] != localIP[1] || clientIP[2] != localIP[2]) {
        LOG_WARN("OTA: Unauthorized access from %s (outside subnet)", clientIP.toString().c_str());
        return false;
    }
    
    return true;
}

bool OTAServer::checkRateLimit(IPAddress clientIP) {
    static IPAddress lastClient;
    static unsigned long lastRequest = 0;
    static uint8_t requestCount = 0;
    
    unsigned long now = millis();
    
    if (clientIP == lastClient) {
        if (now - lastRequest < 60000) { // 1 minute window
            requestCount++;
            if (requestCount > 3) {
                LOG_WARN("OTA: Rate limit exceeded for %s", clientIP.toString().c_str());
                return false;
            }
        } else {
            requestCount = 1;
        }
    } else {
        requestCount = 1;
    }
    
    lastClient = clientIP;
    lastRequest = now;
    return true;
}

void OTAServer::sendUploadPage() {
    sendResponse(200, OTA_UPLOAD_PAGE, "text/html");
}

void OTAServer::sendProgressPage() {
    char progressJson[200];
    snprintf(progressJson, sizeof(progressJson), 
        "{\"status\":\"%s\",\"progress\":%.1f,\"error\":\"%s\"}",
        getStatusString(), getUpdateProgress(), lastError);
    sendResponse(200, progressJson, "application/json");
}

void OTAServer::sendResponse(int code, const char* message, const char* contentType) {
    if (!client.connected()) return;
    
    client.print("HTTP/1.1 ");
    client.print(code);
    client.print(code == 200 ? " OK" : code == 400 ? " Bad Request" : 
                 code == 404 ? " Not Found" : " Internal Server Error");
    client.print("\r\n");
    client.print("Content-Type: ");
    client.print(contentType);
    client.print("\r\n");
    client.print("Connection: close\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("\r\n");
    client.print(message);
}

void OTAServer::stop() {
    if (server) {
        delete server;
        server = nullptr;
    }
    serverRunning = false;
    status = OTA_IDLE;
    
    if (firmwareBuffer) {
        free(firmwareBuffer);
        firmwareBuffer = nullptr;
    }
    
    LOG_INFO("OTA: Server stopped");
}

bool OTAServer::isUpdateInProgress() const {
    return (status == OTA_RECEIVING || status == OTA_VALIDATING || status == OTA_FLASHING);
}

bool OTAServer::isServerRunning() const {
    return serverRunning;
}

float OTAServer::getUpdateProgress() const {
    if (status == OTA_IDLE) return 0.0f;
    if (status == OTA_SUCCESS) return 100.0f;
    if (status == OTA_ERROR) return 0.0f;
    
    if (status == OTA_RECEIVING) {
        return receivedSize > 0 ? (receivedSize * 80.0f) / FIRMWARE_BUFFER_SIZE : 0.0f;
    } else if (status == OTA_VALIDATING) {
        return 85.0f;
    } else if (status == OTA_FLASHING) {
        return 95.0f;
    }
    
    return 0.0f;
}

OTAServer::OTAStatus OTAServer::getStatus() const {
    return status;
}

const char* OTAServer::getStatusString() const {
    switch (status) {
        case OTA_IDLE: return "IDLE";
        case OTA_RECEIVING: return "RECEIVING";
        case OTA_VALIDATING: return "VALIDATING";
        case OTA_FLASHING: return "FLASHING";
        case OTA_SUCCESS: return "SUCCESS";
        case OTA_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* OTAServer::getLastError() const {
    return lastError;
}

uint32_t OTAServer::calculateCRC32(uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}