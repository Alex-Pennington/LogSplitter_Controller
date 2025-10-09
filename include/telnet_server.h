#pragma once

#include <WiFiS3.h>
#include "constants.h"

class TelnetServer {
public:
    TelnetServer();
    
    void begin(int port = 23);
    void update();
    void stop();
    
    bool isConnected();
    bool hasData();
    
    // Output functions
    void print(const char* str);
    void println(const char* str);
    void printf(const char* fmt, ...);
    
    // Input functions
    String readLine();
    void flushInput();
    
    // Connection info
    void setConnectionInfo(const char* hostname, const char* version);
    void showWelcomeMessage();

private:
    WiFiServer server;
    WiFiClient client;
    bool clientConnected;
    String inputBuffer;
    
    // Connection info
    char hostname[32];
    char version[16];
    
    void handleNewConnection();
    void handleClientDisconnection();
};