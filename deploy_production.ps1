#!/usr/bin/env powershell
# Production Build and Deploy Script for LogSplitter Monitor
# Version: 2.1.0
# Features: TFTP Firmware Update System

param(
    [string]$TargetHost = "192.168.1.225",
    [switch]$BuildOnly = $false,
    [switch]$UploadOnly = $false,
    [switch]$SkipTests = $false
)

Write-Host "=====================================" -ForegroundColor Green
Write-Host "LogSplitter Monitor Production Deploy" -ForegroundColor Green
Write-Host "Version 2.1.0 - TFTP Firmware Update" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green
Write-Host ""

# Check if we're in the correct directory
if (-not (Test-Path "monitor\platformio.ini")) {
    Write-Host "❌ Error: Must run from LogSplitter_Controller root directory" -ForegroundColor Red
    exit 1
}

# Build firmware
if (-not $UploadOnly) {
    Write-Host "🔧 Building Monitor Firmware..." -ForegroundColor Yellow
    Push-Location monitor
    try {
        $buildResult = & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run
        if ($LASTEXITCODE -ne 0) {
            Write-Host "❌ Build failed!" -ForegroundColor Red
            exit 1
        }
        Write-Host "✅ Build successful!" -ForegroundColor Green
    } finally {
        Pop-Location
    }
    
    # Show build info
    $firmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
    if (Test-Path $firmwarePath) {
        $size = (Get-Item $firmwarePath).Length
        Write-Host "📦 Firmware size: $size bytes" -ForegroundColor Cyan
        
        # Check ARM vector table
        $bytes = [System.IO.File]::ReadAllBytes($firmwarePath)
        $sp = [BitConverter]::ToUInt32($bytes, 0)
        $rv = [BitConverter]::ToUInt32($bytes, 4)
        Write-Host "🎯 Stack Pointer: 0x$($sp.ToString('X8'))" -ForegroundColor Cyan
        Write-Host "🎯 Reset Vector: 0x$($rv.ToString('X8'))" -ForegroundColor Cyan
    }
}

if ($BuildOnly) {
    Write-Host "✅ Build-only mode complete!" -ForegroundColor Green
    exit 0
}

# Upload firmware via USB
if (-not $UploadOnly) {
    Write-Host ""
    Write-Host "📤 Uploading firmware via USB..." -ForegroundColor Yellow
    Push-Location monitor
    try {
        $uploadResult = & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run --target upload
        if ($LASTEXITCODE -ne 0) {
            Write-Host "❌ Upload failed!" -ForegroundColor Red
            exit 1
        }
        Write-Host "✅ Upload successful!" -ForegroundColor Green
    } finally {
        Pop-Location
    }
    
    # Wait for device restart
    Write-Host "⏳ Waiting for device restart..." -ForegroundColor Yellow
    Start-Sleep -Seconds 15
}

# Test device connectivity
if (-not $SkipTests) {
    Write-Host ""
    Write-Host "🔍 Testing device connectivity..." -ForegroundColor Yellow
    
    $testResult = .\test_single_command.ps1 -TargetHost $TargetHost -Command "status"
    if (-not $testResult) {
        Write-Host "❌ Device connectivity test failed!" -ForegroundColor Red
        Write-Host "   Device may still be restarting. Try manual connection:" -ForegroundColor Yellow
        Write-Host "   telnet $TargetHost 23" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "✅ Device connectivity verified!" -ForegroundColor Green
    
    # Test TFTP functionality
    Write-Host "🔍 Testing TFTP server..." -ForegroundColor Yellow
    $tftpStart = .\test_single_command.ps1 -TargetHost $TargetHost -Command "tftp start"
    if ($tftpStart) {
        $tftpStatus = .\test_single_command.ps1 -TargetHost $TargetHost -Command "tftp status"
        Write-Host "✅ TFTP server operational!" -ForegroundColor Green
        
        # Stop TFTP for normal operation
        .\test_single_command.ps1 -TargetHost $TargetHost -Command "tftp stop" | Out-Null
    }
}

Write-Host ""
Write-Host "🎉 Production deployment complete!" -ForegroundColor Green
Write-Host ""
Write-Host "📋 Next Steps:" -ForegroundColor Cyan
Write-Host "   1. Connect: telnet $TargetHost 23" -ForegroundColor White
Write-Host "   2. Start TFTP: tftp start" -ForegroundColor White
Write-Host "   3. Upload firmware: tftp -i $TargetHost put firmware.bin firmware.bin" -ForegroundColor White
Write-Host "   4. Check status: tftp status" -ForegroundColor White
Write-Host ""
Write-Host "📄 Documentation: TFTP_FIRMWARE_UPDATE.md" -ForegroundColor Cyan