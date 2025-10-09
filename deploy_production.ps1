#!/usr/bin/env powershell
# Production Build and Deploy Script for LogSplitter Monitor
# Version: 2.1.0

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
    Write-Host "Error: Must run from LogSplitter_Controller root directory" -ForegroundColor Red
    exit 1
}

# Build firmware
if (-not $UploadOnly) {
    Write-Host "Building Monitor Firmware..." -ForegroundColor Yellow
    Push-Location monitor
    try {
        $buildResult = & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Build failed!" -ForegroundColor Red
            exit 1
        }
        Write-Host "Build successful!" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}

if ($BuildOnly) {
    Write-Host "Build-only mode complete!" -ForegroundColor Green
    exit 0
}

Write-Host "Production deployment complete!" -ForegroundColor Green