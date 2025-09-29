#include "telnet_server.h"
#include <stdarg.h>

// Global telnet instance
TelnetServer telnet;

TelnetServer::TelnetServer(uint16_t telnetPort) : server(telnetPort), port(telnetPort), clientConnected(false) {
}

void TelnetServer::begin() {
    server.begin();
    clientConnected = false;
    Serial.print("Telnet server started on port ");
    Serial.println(port);
}

void TelnetServer::update() {
    checkClientConnection();
    
    if (clientConnected && !client.connected()) {
        Serial.println("Telnet client disconnected");
        client.stop();
        clientConnected = false;
    }
    
    if (!clientConnected) {
        handleNewClient();
    }
}

void TelnetServer::stop() {
    if (clientConnected) {
        client.stop();
        clientConnected = false;
    }
    server.end();
}

bool TelnetServer::isClientConnected() {
    return clientConnected && client.connected();
}

size_t TelnetServer::print(const char* str) {
    if (isClientConnected()) {
        return client.print(str);
    }
    return 0;
}

size_t TelnetServer::print(const String& str) {
    if (isClientConnected()) {
        return client.print(str);
    }
    return 0;
}

size_t TelnetServer::println(const char* str) {
    if (isClientConnected()) {
        return client.println(str);
    }
    return 0;
}

size_t TelnetServer::println(const String& str) {
    if (isClientConnected()) {
        return client.println(str);
    }
    return 0;
}

size_t TelnetServer::println() {
    if (isClientConnected()) {
        return client.println();
    }
    return 0;
}

size_t TelnetServer::printf(const char* fmt, ...) {
    if (isClientConnected()) {
        char buffer[256];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        
        if (len > 0) {
            return client.print(buffer);
        }
    }
    return 0;
}

bool TelnetServer::available() {
    return isClientConnected() && client.available();
}

String TelnetServer::readString() {
    if (available()) {
        return client.readString();
    }
    return "";
}

void TelnetServer::flush() {
    if (isClientConnected()) {
        client.flush();
    }
}

void TelnetServer::checkClientConnection() {
    // Nothing special needed for this simple implementation
}

void TelnetServer::handleNewClient() {
    WiFiClient newClient = server.available();
    if (newClient) {
        client = newClient;
        clientConnected = true;
        Serial.println("New telnet client connected");
        
        // Send welcome message
        client.println();
        client.println("=== LogSplitter Controller Telnet Console ===");
        client.println("Type 'help' for available commands");
        client.println();
    }
}