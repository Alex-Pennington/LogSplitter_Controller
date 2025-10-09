# Project Reorganization Script
# This script reorganizes the LogSplitter_Controller project into monitor, controller, and shared folders

Write-Host "🔄 LogSplitter_Controller Project Reorganization" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green

# Define paths
$rootPath = Get-Location
$controllerPath = Join-Path $rootPath "controller"
$monitorPath = Join-Path $rootPath "monitor" 
$sharedPath = Join-Path $rootPath "shared"
$oldSrcPath = Join-Path $rootPath "src"
$oldIncludePath = Join-Path $rootPath "include"

Write-Host "📁 Creating directory structure..." -ForegroundColor Yellow

# Create controller directories (already exist but ensure they're there)
New-Item -ItemType Directory -Path "$controllerPath\src" -Force | Out-Null
New-Item -ItemType Directory -Path "$controllerPath\include" -Force | Out-Null
New-Item -ItemType Directory -Path "$controllerPath\lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$controllerPath\test" -Force | Out-Null

# Create shared directories
New-Item -ItemType Directory -Path "$sharedPath\include" -Force | Out-Null
New-Item -ItemType Directory -Path "$sharedPath\src" -Force | Out-Null

Write-Host "✅ Directory structure created" -ForegroundColor Green

# Move controller code from old src to controller/src
Write-Host "📦 Moving controller source files..." -ForegroundColor Yellow
if (Test-Path $oldSrcPath) {
    Get-ChildItem -Path $oldSrcPath -File | ForEach-Object {
        $destination = Join-Path "$controllerPath\src" $_.Name
        Write-Host "  Moving $($_.Name) to controller/src/" -ForegroundColor Gray
        Move-Item $_.FullName $destination -Force
    }
}

# Move controller headers from old include to controller/include (except shared ones)
Write-Host "📦 Moving controller header files..." -ForegroundColor Yellow
if (Test-Path $oldIncludePath) {
    # Files that should go to shared (common between monitor and controller)
    $sharedHeaders = @(
        "arduino_secrets.h",
        "constants.h", 
        "logger.h"
    )
    
    Get-ChildItem -Path $oldIncludePath -File | ForEach-Object {
        if ($sharedHeaders -contains $_.Name) {
            $destination = Join-Path "$sharedPath\include" $_.Name
            Write-Host "  Moving $($_.Name) to shared/include/" -ForegroundColor Cyan
            Move-Item $_.FullName $destination -Force
        } else {
            $destination = Join-Path "$controllerPath\include" $_.Name
            Write-Host "  Moving $($_.Name) to controller/include/" -ForegroundColor Gray
            Move-Item $_.FullName $destination -Force
        }
    }
}

# Copy platformio.ini to controller and monitor directories
Write-Host "📦 Setting up build configurations..." -ForegroundColor Yellow
if (Test-Path "platformio.ini") {
    Copy-Item "platformio.ini" "$controllerPath\platformio.ini" -Force
    Write-Host "  Copied platformio.ini to controller/" -ForegroundColor Gray
}

# Move existing lib and test directories to controller
Write-Host "📦 Moving library and test directories..." -ForegroundColor Yellow
if (Test-Path "lib") {
    if (Test-Path "$controllerPath\lib") {
        Remove-Item "$controllerPath\lib" -Recurse -Force
    }
    Move-Item "lib" "$controllerPath\lib" -Force
    Write-Host "  Moved lib/ to controller/lib/" -ForegroundColor Gray
}

if (Test-Path "test") {
    if (Test-Path "$controllerPath\test") {
        Remove-Item "$controllerPath\test" -Recurse -Force  
    }
    Move-Item "test" "$controllerPath\test" -Force
    Write-Host "  Moved test/ to controller/test/" -ForegroundColor Gray
}

# Update controller platformio.ini to reference shared folder
Write-Host "🔧 Updating controller platformio.ini..." -ForegroundColor Yellow
$controllerPlatformioIni = Join-Path $controllerPath "platformio.ini"
if (Test-Path $controllerPlatformioIni) {
    $content = Get-Content $controllerPlatformioIni
    # Add shared include path
    $newContent = $content | ForEach-Object {
        if ($_ -match "^build_flags") {
            $_ + " -I../shared/include"
        } else {
            $_
        }
    }
    
    # If no build_flags found, add it
    if ($content -notmatch "^build_flags") {
        $newContent += ""
        $newContent += "build_flags = -I../shared/include"
    }
    
    $newContent | Set-Content $controllerPlatformioIni
    Write-Host "  Updated controller platformio.ini with shared include path" -ForegroundColor Gray
}

# Update monitor platformio.ini to reference shared folder  
Write-Host "🔧 Updating monitor platformio.ini..." -ForegroundColor Yellow
$monitorPlatformioIni = Join-Path $monitorPath "platformio.ini"
if (Test-Path $monitorPlatformioIni) {
    $content = Get-Content $monitorPlatformioIni
    # Add shared include path
    $newContent = $content | ForEach-Object {
        if ($_ -match "^build_flags") {
            $_ + " -I../shared/include"
        } else {
            $_
        }
    }
    
    # If no build_flags found, add it
    if ($content -notmatch "^build_flags") {
        $newContent += ""
        $newContent += "build_flags = -I../shared/include"
    }
    
    $newContent | Set-Content $monitorPlatformioIni
    Write-Host "  Updated monitor platformio.ini with shared include path" -ForegroundColor Gray
}

# Move shared source files
Write-Host "📦 Moving shared source files..." -ForegroundColor Yellow
$sharedSourceFiles = @(
    "logger.cpp"
)

# Check if any shared source files exist in monitor and copy to shared
foreach ($file in $sharedSourceFiles) {
    $monitorFile = Join-Path "$monitorPath\src" $file
    $sharedFile = Join-Path "$sharedPath\src" $file
    
    if (Test-Path $monitorFile) {
        Copy-Item $monitorFile $sharedFile -Force
        Write-Host "  Copied $file from monitor/src to shared/src/" -ForegroundColor Cyan
    }
}

# Remove empty old directories
Write-Host "🧹 Cleaning up empty directories..." -ForegroundColor Yellow
if (Test-Path $oldSrcPath) {
    $srcItems = Get-ChildItem $oldSrcPath
    if ($srcItems.Count -eq 0) {
        Remove-Item $oldSrcPath -Force
        Write-Host "  Removed empty src/ directory" -ForegroundColor Gray
    }
}

if (Test-Path $oldIncludePath) {
    $includeItems = Get-ChildItem $oldIncludePath
    if ($includeItems.Count -eq 0) {
        Remove-Item $oldIncludePath -Force  
        Write-Host "  Removed empty include/ directory" -ForegroundColor Gray
    }
}

# Create README files for each directory
Write-Host "📝 Creating README files..." -ForegroundColor Yellow

$controllerReadme = @'
# Controller

This directory contains the main LogSplitter hydraulic controller code.

## Structure
- src/ - Controller source files (.cpp)
- include/ - Controller headers (.h) 
- lib/ - Controller-specific libraries
- test/ - Controller unit tests
- platformio.ini - PlatformIO build configuration

## Building
```
cd controller
pio run
```

## Uploading  
```
cd controller
pio run --target upload
```

## Features
- Hydraulic pressure control
- Safety systems and E-stop handling
- Relay control for splitter operation
- Sequence control for automated operations
- Network connectivity (WiFi/MQTT)
- Telnet command interface
- Over-the-air (OTA) firmware updates
'@

$monitorReadme = @'
# Monitor

This directory contains the LogSplitter monitoring system code.

## Structure
- src/ - Monitor source files (.cpp)
- include/ - Monitor headers (.h)
- platformio.ini - PlatformIO build configuration

## Building
```
cd monitor  
pio run
```

## Uploading
```
cd monitor
pio run --target upload
```

## Features
- Multi-sensor monitoring (temperature, weight, power, ADC)
- I2C multiplexer support for multiple sensors
- LCD display with heartbeat animation
- Network connectivity and logging
- Telnet command interface
- Real-time sensor data collection
'@

$sharedReadme = @'
# Shared

This directory contains code shared between the controller and monitor.

## Structure
- include/ - Shared header files (.h)
- src/ - Shared source files (.cpp)

## Shared Components
- arduino_secrets.h - WiFi credentials and network configuration
- constants.h - Common constants and definitions
- logger.h - Logging system interface

## Usage
Both controller and monitor projects reference this directory via:
```
build_flags = -I../shared/include
```

The shared directory allows common code to be maintained in one place while being used by both projects.
'@

Set-Content "$controllerPath\README.md" $controllerReadme
Set-Content "$monitorPath\README.md" $monitorReadme  
Set-Content "$sharedPath\README.md" $sharedReadme

Write-Host "  Created README.md files for all directories" -ForegroundColor Gray

# Create a new workspace file
Write-Host "🏢 Creating VS Code workspace configuration..." -ForegroundColor Yellow
$workspaceConfig = @{
    folders = @(
        @{ name = "Controller"; path = "./controller" }
        @{ name = "Monitor"; path = "./monitor" }
        @{ name = "Shared"; path = "./shared" }
        @{ name = "Documentation"; path = "./docs" }
        @{ name = "Root"; path = "./" }
    )
    settings = @{
        "C_Cpp.default.includePath" = @(
            "`${workspaceFolder}/shared/include",
            "`${workspaceFolder}/controller/include", 
            "`${workspaceFolder}/monitor/include"
        )
        "files.associations" = @{
            "*.h" = "cpp"
            "*.cpp" = "cpp"
            "*.ino" = "cpp"
        }
    }
    extensions = @{
        recommendations = @(
            "platformio.platformio-ide",
            "ms-vscode.cpptools",
            "ms-vscode.cpptools-extension-pack"
        )
    }
}

$workspaceJson = $workspaceConfig | ConvertTo-Json -Depth 10
Set-Content "LogSplitter_Reorganized.code-workspace" $workspaceJson
Write-Host "  Created LogSplitter_Reorganized.code-workspace" -ForegroundColor Gray

Write-Host ""
Write-Host "🎉 Project reorganization complete!" -ForegroundColor Green
Write-Host "📊 New project structure:" -ForegroundColor Cyan
Write-Host "  📁 controller/     - Main hydraulic controller" -ForegroundColor White
Write-Host "  📁 monitor/        - Sensor monitoring system" -ForegroundColor White  
Write-Host "  📁 shared/         - Common code and configuration" -ForegroundColor White
Write-Host ""
Write-Host "🔨 Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open LogSplitter_Reorganized.code-workspace in VS Code" -ForegroundColor White
Write-Host "  2. Test builds:" -ForegroundColor White  
Write-Host "     cd controller && pio run" -ForegroundColor Gray
Write-Host "     cd monitor && pio run" -ForegroundColor Gray
Write-Host "  3. Update any hardcoded paths in source files if needed" -ForegroundColor White
Write-Host ""
Write-Host "✨ Project is now organized and ready for development!" -ForegroundColor Green