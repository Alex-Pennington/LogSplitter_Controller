# Build and Upload via TFTP Script
# Builds monitor firmware and uploads via TFTP if successful

param(
    [string]$TargetHost = "192.168.1.225"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Green
Write-Host "Build and Upload Monitor via TFTP" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Step 1: Build the firmware
Write-Host "Building monitor firmware..." -ForegroundColor Yellow
Push-Location "monitor"
try {
    $buildResult = & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "Build successful!" -ForegroundColor Green
} catch {
    Write-Host "Build error: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}

# Step 2: Check if firmware binary exists
$firmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
if (-not (Test-Path $firmwarePath)) {
    Write-Host "Firmware binary not found at: $firmwarePath" -ForegroundColor Red
    exit 1
}

$firmwareSize = (Get-Item $firmwarePath).Length
Write-Host "Firmware size: $firmwareSize bytes" -ForegroundColor Cyan

# Step 3: Start TFTP server on device
Write-Host "Starting TFTP server on device..." -ForegroundColor Yellow

try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $tcpClient.ReceiveTimeout = 5000
    $tcpClient.Connect($TargetHost, 23)
    
    $stream = $tcpClient.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    
    # Wait and send command
    Start-Sleep -Seconds 2
    $writer.WriteLine("tftp start")
    $writer.Flush()
    Start-Sleep -Seconds 1
    
    $writer.Close()
    $tcpClient.Close()
    
    Write-Host "TFTP server start command sent" -ForegroundColor Green
} catch {
    Write-Host "Warning: Could not start TFTP server via telnet: $_" -ForegroundColor Yellow
    Write-Host "Continuing with upload attempt..." -ForegroundColor Yellow
}

# Step 4: Upload firmware via TFTP
Write-Host "Uploading firmware via TFTP..." -ForegroundColor Yellow
Write-Host "Source: $firmwarePath ($firmwareSize bytes)" -ForegroundColor Gray
Write-Host "Target: $TargetHost" -ForegroundColor Gray

try {
    $uploadStart = Get-Date
    $uploadOutput = & tftp -i $TargetHost put $firmwarePath firmware.bin 2>&1
    $uploadEnd = Get-Date
    $uploadTime = ($uploadEnd - $uploadStart).TotalSeconds
    
    if ($LASTEXITCODE -eq 0) {
        $transferRate = [math]::Round($firmwareSize / $uploadTime, 0)
        Write-Host "TFTP upload successful!" -ForegroundColor Green
        Write-Host "Time: $([math]::Round($uploadTime, 1)) seconds" -ForegroundColor Cyan
        Write-Host "Rate: $transferRate bytes/second" -ForegroundColor Cyan
    } else {
        Write-Host "TFTP upload failed!" -ForegroundColor Red
        Write-Host "Output: $uploadOutput" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "TFTP upload error: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build and TFTP upload complete!" -ForegroundColor Green
Write-Host "Firmware: $firmwareSize bytes uploaded successfully" -ForegroundColor Cyan