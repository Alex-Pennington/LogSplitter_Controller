#pragma once

#include <Arduino.h>
#include <WiFiS3.h>

class TelnetServer {
private:
    WiFiServer server;
    WiFiClient client;
    bool clientConnected;
    uint16_t port;
    char connectionInfo[128];  // Store hostname and IP info for welcome message
    
public:
    TelnetServer(uint16_t telnetPort = 23);
    
    void begin();
    void update();
    void stop();
    void setConnectionInfo(const char* hostname, const char* ipAddress);
    
    bool isClientConnected();
    size_t print(const char* str);
    size_t print(const String& str);
    size_t println(const char* str);
    size_t println(const String& str);
    size_t println();
    size_t printf(const char* fmt, ...);
    
    bool available();
    int read();
    String readString();
    void flush();
    
private:
    void checkClientConnection();
    void handleNewClient();
};

// Global telnet instance
extern TelnetServer telnet;

// Helper macros for easy migration
#define TELNET_PRINT(x) telnet.print(x)
#define TELNET_PRINTLN(x) telnet.println(x)
#define TELNET_PRINTF(...) telnet.printf(__VA_ARGS__)