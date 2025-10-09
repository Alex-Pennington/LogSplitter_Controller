#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <WiFiS3.h>

// Constants
#define FIRMWARE_BUFFER_SIZE 8192    // 8KB buffer for streaming - R4 WiFi has limited RAM
#define HTTP_BUFFER_SIZE 1024
#define CLIENT_TIMEOUT_MS 30000

class OTAServer {
public:
    enum OTAStatus {
        OTA_IDLE,
        OTA_RECEIVING,
        OTA_VALIDATING,
        OTA_FLASHING,
        OTA_SUCCESS,
        OTA_ERROR
    };

    OTAServer();
    
    void begin(uint16_t port = 80);
    void update();
    void stop();
    
    bool isUpdateInProgress() const;
    bool isServerRunning() const;
    float getUpdateProgress() const;
    OTAStatus getStatus() const;
    const char* getStatusString() const;
    const char* getLastError() const;
    
private:
    WiFiServer* server;
    WiFiClient client;
    bool serverRunning;
    uint16_t serverPort;
    
    OTAStatus status;
    size_t expectedSize;
    size_t receivedSize;
    char lastError[100];
    uint8_t* firmwareBuffer;
    
    void handleClient();
    void handleUploadRequest();
    bool parseMultipartForm(WiFiClient& client);
    bool parseBinaryUpload(WiFiClient& client);
    bool validateFirmware(uint8_t* data, size_t size);
    bool flashFirmware(uint8_t* data, size_t size);
    
    bool isAuthorizedClient(IPAddress clientIP);
    bool checkRateLimit(IPAddress clientIP);
    
    void sendUploadPage();
    void sendProgressPage();
    void sendResponse(int code, const char* message, const char* contentType = "text/plain");
    
    uint32_t calculateCRC32(uint8_t* data, size_t size);
};

// Global instance
extern OTAServer otaServer;

#endif // OTA_SERVER_H