// OTA Commands for Command Processor

// In command_processor.cpp, add these commands:

bool CommandProcessor::processOTACommands(const char* command, char* response, size_t responseSize) {
    if (strncmp(command, "ota ", 4) == 0) {
        const char* subCmd = command + 4;
        
        if (strcmp(subCmd, "status") == 0) {
            // Get OTA status
            snprintf(response, responseSize, 
                "OTA Status: %s, Progress: %.1f%%, Error: %s",
                otaServer.getStatusString(),
                otaServer.getUpdateProgress(),
                otaServer.getLastError());
            return true;
        }
        else if (strcmp(subCmd, "start") == 0) {
            // Start OTA server
            otaServer.begin(80);
            String ip = networkManager->getLocalIP();
            snprintf(response, responseSize, 
                "OTA server started on http://%s/update", ip.c_str());
            LOG_INFO("OTA: Server started for firmware updates");
            return true;
        }
        else if (strcmp(subCmd, "stop") == 0) {
            // Stop OTA server
            otaServer.stop();
            snprintf(response, responseSize, "OTA server stopped");
            LOG_INFO("OTA: Server stopped");
            return true;
        }
        else if (strcmp(subCmd, "reboot") == 0) {
            // Reboot system (useful after update)
            snprintf(response, responseSize, "System rebooting in 3 seconds...");
            LOG_INFO("OTA: System reboot requested via telnet");
            delay(3000);
            NVIC_SystemReset(); // Arduino R4 WiFi reset
            return true;
        }
        else if (strncmp(subCmd, "url", 3) == 0) {
            // Show upload URL
            String ip = networkManager->getLocalIP();
            snprintf(response, responseSize, 
                "Upload firmware to: http://%s/update\n"
                "Or use curl: curl -X POST -F \"firmware=@firmware.bin\" http://%s/update",
                ip.c_str(), ip.c_str());
            return true;
        }
        else {
            snprintf(response, responseSize, 
                "OTA commands: status, start, stop, reboot, url");
            return true;
        }
    }
    return false;
}

// OTA Upload Web Page (served by HTTP server)
const char* OTA_UPLOAD_PAGE = R"(
<!DOCTYPE html>
<html>
<head>
    <title>LogMonitor OTA Update</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 600px; margin: 0 auto; }
        .button { background: #4CAF50; color: white; padding: 10px 20px; border: none; cursor: pointer; }
        .progress { width: 100%; height: 20px; border: 1px solid #ccc; margin: 10px 0; }
        .progress-bar { height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 LogMonitor Firmware Update</h1>
        <p><strong>Device:</strong> LogMonitor</p>
        <p><strong>Current Version:</strong> 1.0.0</p>
        <p><strong>Flash Usage:</strong> 51.8% (135,664 bytes)</p>
        
        <form action="/update" method="POST" enctype="multipart/form-data" id="uploadForm">
            <h3>Select Firmware File:</h3>
            <input type="file" name="firmware" accept=".bin" required>
            <br><br>
            <input type="submit" value="🚀 Upload Firmware" class="button">
        </form>
        
        <div id="progress" class="progress" style="display:none;">
            <div id="progressBar" class="progress-bar"></div>
        </div>
        <div id="status"></div>
        
        <h3>📋 Instructions:</h3>
        <ol>
            <li>Build firmware: <code>pio run</code></li>
            <li>Firmware file location: <code>.pio/build/uno_r4_wifi/firmware.bin</code></li>
            <li>Select the .bin file above and click Upload</li>
            <li>Wait for upload and verification to complete</li>
            <li>Device will automatically reboot with new firmware</li>
        </ol>
        
        <h3>🛡️ Security Features:</h3>
        <ul>
            <li>✅ CRC32 firmware validation</li>
            <li>✅ Size limits (max 200KB)</li>
            <li>✅ Local network access only</li>
            <li>✅ Syslog audit logging</li>
        </ul>
    </div>
    
    <script>
        document.getElementById('uploadForm').onsubmit = function() {
            document.getElementById('progress').style.display = 'block';
            document.getElementById('status').innerHTML = 'Uploading firmware...';
        };
    </script>
</body>
</html>
)";