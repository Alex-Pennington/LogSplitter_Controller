# Build and Upload via TFTP Script
# Builds monitor firmware and uploads via TFTP if successful

param(
    [string]$TargetHost = "192.168.1.225",
    [int]$TftpPort = 69,
    [int]$TelnetPort = 23
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Green
Write-Host "Build and Upload Monitor via TFTP" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Step 1: Build the firmware
Write-Host "🔧 Building monitor firmware..." -ForegroundColor Yellow
Push-Location "monitor"
try {
    $buildResult = & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "✅ Build successful!" -ForegroundColor Green
} catch {
    Write-Host "❌ Build error: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

# Step 2: Check if firmware binary exists
$firmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
if (-not (Test-Path $firmwarePath)) {
    Write-Host "❌ Firmware binary not found at: $firmwarePath" -ForegroundColor Red
    exit 1
}

$firmwareSize = (Get-Item $firmwarePath).Length
Write-Host "📦 Firmware size: $firmwareSize bytes" -ForegroundColor Cyan

# Step 3: Start TFTP server on device
Write-Host "🌐 Starting TFTP server on device $TargetHost..." -ForegroundColor Yellow

function Send-TelnetCommand {
    param([string]$Host, [int]$Port, [string]$Command)
    
    try {
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        $tcpClient.ReceiveTimeout = 5000
        $tcpClient.SendTimeout = 5000
        $tcpClient.Connect($Host, $Port)
        
        $stream = $tcpClient.GetStream()
        $writer = New-Object System.IO.StreamWriter($stream)
        $reader = New-Object System.IO.StreamReader($stream)
        
        # Wait for connection and skip banner
        Start-Sleep -Seconds 2
        while ($reader.Peek() -ne -1) {
            $reader.ReadLine() | Out-Null
        }
        
        # Send command
        $writer.WriteLine($Command)
        $writer.Flush()
        
        # Read response
        Start-Sleep -Seconds 1
        $response = ""
        while ($reader.Peek() -ne -1) {
            $line = $reader.ReadLine()
            $response += $line + "`n"
        }
        
        $writer.Close()
        $reader.Close()
        $tcpClient.Close()
        
        return $response.Trim()
    } catch {
        Write-Host "⚠️ Telnet command failed: $_" -ForegroundColor Yellow
        return $null
    }
}

# Start TFTP server
$startResult = Send-TelnetCommand -Host $TargetHost -Port $TelnetPort -Command "tftp start"
if ($startResult -match "started") {
    Write-Host "✅ TFTP server started successfully" -ForegroundColor Green
} else {
    Write-Host "⚠️ TFTP server may already be running or failed to start" -ForegroundColor Yellow
    Write-Host "Response: $startResult" -ForegroundColor Gray
}

# Step 4: Upload firmware via TFTP
Write-Host "📤 Uploading firmware via TFTP..." -ForegroundColor Yellow
Write-Host "   Source: $firmwarePath ($firmwareSize bytes)" -ForegroundColor Gray
Write-Host "   Target: ${TargetHost}:${TftpPort}" -ForegroundColor Gray

try {
    $uploadStart = Get-Date
    $uploadOutput = & tftp -i $TargetHost put $firmwarePath firmware.bin 2>&1
    $uploadEnd = Get-Date
    $uploadTime = ($uploadEnd - $uploadStart).TotalSeconds
    
    if ($LASTEXITCODE -eq 0) {
        $transferRate = [math]::Round($firmwareSize / $uploadTime, 0)
        Write-Host "✅ TFTP upload successful!" -ForegroundColor Green
        Write-Host "   Time: $([math]::Round($uploadTime, 1)) seconds" -ForegroundColor Cyan
        Write-Host "   Rate: $transferRate bytes/second" -ForegroundColor Cyan
    } else {
        Write-Host "❌ TFTP upload failed!" -ForegroundColor Red
        Write-Host "Output: $uploadOutput" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ TFTP upload error: $_" -ForegroundColor Red
    exit 1
}

# Step 5: Check TFTP status
Write-Host "🔍 Checking upload status..." -ForegroundColor Yellow
$statusResult = Send-TelnetCommand -Host $TargetHost -Port $TelnetPort -Command "tftp status"
if ($statusResult) {
    Write-Host "📊 TFTP Status:" -ForegroundColor Cyan
    $statusResult -split "`n" | ForEach-Object {
        if ($_ -match "Status|Error|bytes") {
            Write-Host "   $_" -ForegroundColor White
        }
    }
}

Write-Host ""
Write-Host "🎉 Build and TFTP upload complete!" -ForegroundColor Green
Write-Host "   Firmware: $firmwareSize bytes uploaded successfully" -ForegroundColor Cyan
Write-Host "   Device: $TargetHost ready for operation" -ForegroundColor Cyan