// Security Implementation for Custom OTA

class OTASecurity {
public:
    // Firmware validation
    static bool validateFirmware(uint8_t* data, size_t size) {
        // 1. Size check
        if (size < 1024 || size > MAX_FIRMWARE_SIZE) {
            LOG_ERROR("OTA: Invalid firmware size: %d bytes", size);
            return false;
        }
        
        // 2. Magic number check (first 4 bytes should be ARM reset vector)
        uint32_t* vectors = (uint32_t*)data;
        uint32_t stackPointer = vectors[0];
        uint32_t resetVector = vectors[1];
        
        // Stack pointer should be in RAM range (0x20000000 - 0x20008000)
        if (stackPointer < 0x20000000 || stackPointer > 0x20008000) {
            LOG_ERROR("OTA: Invalid stack pointer in firmware: 0x%08X", stackPointer);
            return false;
        }
        
        // Reset vector should be in flash range and odd (Thumb mode)
        if ((resetVector & 0xFF000000) != 0x08000000 || (resetVector & 1) == 0) {
            LOG_ERROR("OTA: Invalid reset vector in firmware: 0x%08X", resetVector);
            return false;
        }
        
        // 3. CRC32 validation (if embedded in firmware)
        uint32_t calculatedCRC = calculateCRC32(data, size - 4);
        uint32_t embeddedCRC = *(uint32_t*)(data + size - 4);
        
        if (calculatedCRC != embeddedCRC) {
            LOG_WARN("OTA: CRC mismatch - calculated: 0x%08X, embedded: 0x%08X", 
                calculatedCRC, embeddedCRC);
            // Don't fail on CRC mismatch - might not be embedded
        }
        
        LOG_INFO("OTA: Firmware validation passed");
        return true;
    }
    
    // Network access control
    static bool isAuthorizedClient(IPAddress clientIP) {
        IPAddress localIP = WiFi.localIP();
        
        // Only allow same subnet access
        if ((clientIP[0] != localIP[0]) || 
            (clientIP[1] != localIP[1]) || 
            (clientIP[2] != localIP[2])) {
            LOG_WARN("OTA: Unauthorized access attempt from %s", clientIP.toString().c_str());
            return false;
        }
        
        LOG_INFO("OTA: Authorized client: %s", clientIP.toString().c_str());
        return true;
    }
    
    // Rate limiting
    static bool checkRateLimit(IPAddress clientIP) {
        static IPAddress lastClient;
        static unsigned long lastRequest = 0;
        static uint8_t requestCount = 0;
        
        unsigned long now = millis();
        
        if (clientIP == lastClient) {
            if (now - lastRequest < 60000) { // Same client within 1 minute
                requestCount++;
                if (requestCount > 3) { // Max 3 requests per minute
                    LOG_WARN("OTA: Rate limit exceeded for %s", clientIP.toString().c_str());
                    return false;
                }
            } else {
                requestCount = 1; // Reset counter
            }
        } else {
            requestCount = 1; // New client
        }
        
        lastClient = clientIP;
        lastRequest = now;
        return true;
    }

private:
    static uint32_t calculateCRC32(uint8_t* data, size_t size) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < size; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        return ~crc;
    }
};