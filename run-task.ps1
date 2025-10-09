# Quick Task Runner for Monitor Development
# Provides easy access to common development tasks

param(
    [Parameter(Position=0)]
    [ValidateSet("build", "upload-usb", "upload-tftp", "build-upload", "status", "help")]
    [string]$Task = "help",
    
    [string]$TargetHost = "192.168.1.225"
)

function Show-Help {
    Write-Host "Monitor Development Task Runner" -ForegroundColor Green
    Write-Host "===============================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage: .\run-task.ps1 <task> [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Available Tasks:" -ForegroundColor Cyan
    Write-Host "  build        - Build monitor firmware only" -ForegroundColor White
    Write-Host "  upload-usb   - Upload via USB (requires build first)" -ForegroundColor White
    Write-Host "  upload-tftp  - Upload via TFTP (requires build first)" -ForegroundColor White
    Write-Host "  build-upload - Build and upload via TFTP (recommended)" -ForegroundColor White
    Write-Host "  status       - Check device and TFTP status" -ForegroundColor White
    Write-Host "  help         - Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Cyan
    Write-Host "  -TargetHost  - Device IP address (default: 192.168.1.225)" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\run-task.ps1 build-upload" -ForegroundColor Gray
    Write-Host "  .\run-task.ps1 status" -ForegroundColor Gray
    Write-Host "  .\run-task.ps1 upload-tftp -TargetHost 192.168.1.100" -ForegroundColor Gray
}

function Invoke-Build {
    Write-Host "Building monitor firmware..." -ForegroundColor Yellow
    Push-Location "monitor"
    try {
        & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run
        return $LASTEXITCODE -eq 0
    } finally {
        Pop-Location
    }
}

function Invoke-USBUpload {
    Write-Host "Uploading via USB..." -ForegroundColor Yellow
    Push-Location "monitor"
    try {
        & "C:\Users\USER\.platformio\penv\Scripts\platformio.exe" run --target upload
        return $LASTEXITCODE -eq 0
    } finally {
        Pop-Location
    }
}

function Invoke-TFTPUpload {
    $firmwarePath = "monitor\.pio\build\uno_r4_wifi\firmware.bin"
    if (-not (Test-Path $firmwarePath)) {
        Write-Host "Firmware not found. Build first!" -ForegroundColor Red
        return $false
    }
    
    Write-Host "Uploading via TFTP to $TargetHost..." -ForegroundColor Yellow
    $result = & tftp -i $TargetHost put $firmwarePath firmware.bin 2>&1
    return $LASTEXITCODE -eq 0
}

function Get-DeviceStatus {
    Write-Host "Checking device status..." -ForegroundColor Yellow
    try {
        $result = .\test_single_command.ps1 -TargetHost $TargetHost -Command "status"
        if ($result) {
            Write-Host "Device is online and responsive" -ForegroundColor Green
            
            # Check TFTP status
            $tftpResult = .\test_single_command.ps1 -TargetHost $TargetHost -Command "tftp status"
            Write-Host "TFTP status checked" -ForegroundColor Cyan
        } else {
            Write-Host "Device is not responding" -ForegroundColor Red
        }
    } catch {
        Write-Host "Error checking device: $_" -ForegroundColor Red
    }
}

# Main task execution
switch ($Task) {
    "build" {
        $success = Invoke-Build
        if ($success) {
            Write-Host "Build completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "Build failed!" -ForegroundColor Red
            exit 1
        }
    }
    
    "upload-usb" {
        $success = Invoke-USBUpload
        if ($success) {
            Write-Host "USB upload completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "USB upload failed!" -ForegroundColor Red
            exit 1
        }
    }
    
    "upload-tftp" {
        $success = Invoke-TFTPUpload
        if ($success) {
            Write-Host "TFTP upload completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "TFTP upload failed!" -ForegroundColor Red
            exit 1
        }
    }
    
    "build-upload" {
        Write-Host "Starting build and upload process..." -ForegroundColor Cyan
        
        $buildSuccess = Invoke-Build
        if (-not $buildSuccess) {
            Write-Host "Build failed, aborting upload!" -ForegroundColor Red
            exit 1
        }
        
        $uploadSuccess = Invoke-TFTPUpload
        if ($uploadSuccess) {
            Write-Host "Build and TFTP upload completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "Build succeeded but TFTP upload failed!" -ForegroundColor Red
            exit 1
        }
    }
    
    "status" {
        Get-DeviceStatus
    }
    
    "help" {
        Show-Help
    }
    
    default {
        Write-Host "Unknown task: $Task" -ForegroundColor Red
        Show-Help
        exit 1
    }
}