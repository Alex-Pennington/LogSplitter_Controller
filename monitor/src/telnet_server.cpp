#include "telnet_server.h"
#include <WiFiS3.h>
#include <stdarg.h>

extern void debugPrintf(const char* fmt, ...);

TelnetServer::TelnetServer() : 
    server(23),
    clientConnected(false) {
    
    // Set default connection info
    strncpy(hostname, "LogMonitor", sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = '\0';
    
    strncpy(version, "1.0.0", sizeof(version) - 1);
    version[sizeof(version) - 1] = '\0';
}

void TelnetServer::begin(int port) {
    server = WiFiServer(port);
    server.begin();
    debugPrintf("TelnetServer: Started on port %d\n", port);
}

void TelnetServer::update() {
    // Check for new client connections
    if (!clientConnected) {
        client = server.available();
        if (client) {
            handleNewConnection();
        }
    } else {
        // Check if existing client is still connected
        if (!client.connected()) {
            handleClientDisconnection();
        }
    }
}

void TelnetServer::handleNewConnection() {
    clientConnected = true;
    inputBuffer = "";
    
    debugPrintf("TelnetServer: Client connected from %s\n", client.remoteIP().toString().c_str());
    
    // Show welcome message
    showWelcomeMessage();
    
    // Show prompt
    client.print("> ");
}

void TelnetServer::handleClientDisconnection() {
    debugPrintf("TelnetServer: Client disconnected\n");
    client.stop();
    clientConnected = false;
    inputBuffer = "";
}

void TelnetServer::showWelcomeMessage() {
    if (!clientConnected) return;
    
    client.print("\r\n");
    client.print("===============================================\r\n");
    client.print("   LogSplitter Monitor\r\n");
    client.print("   Version: ");
    client.print(version);
    client.print("\r\n");
    client.print("   Type 'help' for available commands\r\n");
    client.print("===============================================\r\n");
    client.print("Connected to: ");
    client.print(WiFi.localIP().toString().c_str());
    client.print("\r\n");
    client.print("Hostname: ");
    client.print(hostname);
    client.print("\r\n\r\n");
}

void TelnetServer::setConnectionInfo(const char* newHostname, const char* newVersion) {
    if (newHostname) {
        strncpy(hostname, newHostname, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }
    
    if (newVersion) {
        strncpy(version, newVersion, sizeof(version) - 1);
        version[sizeof(version) - 1] = '\0';
    }
}

void TelnetServer::stop() {
    if (clientConnected) {
        client.stop();
        clientConnected = false;
    }
    server.end();
    debugPrintf("TelnetServer: Stopped\n");
}

bool TelnetServer::isConnected() {
    return clientConnected && client.connected();
}

bool TelnetServer::hasData() {
    return clientConnected && client.available() > 0;
}

void TelnetServer::print(const char* str) {
    if (clientConnected && client.connected()) {
        client.print(str);
    }
}

void TelnetServer::println(const char* str) {
    if (clientConnected && client.connected()) {
        client.print(str);
        client.print("\r\n");
    }
}

void TelnetServer::printf(const char* fmt, ...) {
    if (!clientConnected || !client.connected()) return;
    
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    client.print(buffer);
}

String TelnetServer::readLine() {
    String line = "";
    
    while (hasData()) {
        char c = client.read();
        
        // Handle telnet protocol sequences (IAC commands)
        if ((unsigned char)c == 255) { // IAC (Interpret as Command)
            // Skip telnet protocol sequences
            if (hasData()) client.read(); // Command
            if (hasData()) client.read(); // Option
            continue;
        }
        
        if (c == '\n') {
            if (inputBuffer.length() > 0) {
                line = inputBuffer;
                inputBuffer = "";
                println(""); // Move to next line
                break;
            }
        } else if (c == '\r') {
            // Ignore carriage return, wait for line feed
            continue;
        } else if (c == '\b' || c == 127) { // Backspace or DEL
            if (inputBuffer.length() > 0) {
                inputBuffer.remove(inputBuffer.length() - 1);
                print("\b \b"); // Erase character on screen
            }
        } else if (c >= 32 && c <= 126) { // Printable ASCII characters only
            inputBuffer += c;
            client.write(c); // Echo character back
        }
        // Ignore all other control characters
    }
    
    return line;
}

void TelnetServer::flushInput() {
    inputBuffer = "";
    while (hasData()) {
        client.read();
    }
}