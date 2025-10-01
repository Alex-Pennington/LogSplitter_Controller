# LogSplitter Repository Migration Guide

## Overview

This guide provides step-by-step instructions for migrating from the current separate project structure to the unified LogSplitter repository architecture.

## Current State

**Before Migration:**
```
LogSplitter_Controller/     # Standalone controller project
├── src/, include/, platformio.ini, etc.

LogSplitter_Monitor/        # Standalone monitor project  
├── src/, include/, platformio.ini, etc.
```

**After Migration:**
```
LogSplitter/                # Unified repository
├── README.md              # Main project overview
├── controller/            # Controller project
├── monitor/               # Monitor project
├── shared/                # Common components
└── docs/                  # Unified documentation
```

## Migration Steps

### Step 1: Backup Current Projects

```bash
# Create backup copies
cp -r LogSplitter_Controller LogSplitter_Controller_backup
cp -r LogSplitter_Monitor LogSplitter_Monitor_backup
```

### Step 2: Create Unified Repository Structure

You already have the base structure created in the Controller directory. Now we need to move the current files to their new locations.

#### 2a. Move Controller Files

```bash
# Navigate to the Controller directory (where you currently are)
cd LogSplitter_Controller

# Create controller subdirectory structure
mkdir -p controller/src
mkdir -p controller/include  
mkdir -p controller/lib
mkdir -p controller/test

# Move controller files to subdirectory
cp platformio.ini controller/
cp -r src/* controller/src/
cp -r include/* controller/include/
cp -r lib/* controller/lib/
cp -r test/* controller/test/
cp README_REFACTORED.md controller/README.md
cp COMMANDS.md controller/
cp PINS.md controller/
cp COMPREHENSIVE_REVIEW.md controller/
cp SYSTEM_TEST_SUITE.md controller/
cp ARDUINO_SECRETS_SETUP.md controller/

# Update controller to use shared logger
# (We'll do this in the next step)
```

#### 2b. Copy Monitor Files

```bash
# Copy monitor files from the other project
# Note: You'll need to manually copy these from the LogSplitter_Monitor directory

mkdir -p monitor/src
mkdir -p monitor/include
mkdir -p monitor/lib  
mkdir -p monitor/test

# These files would need to be copied:
# - LogSplitter_Monitor/platformio.ini -> monitor/platformio.ini
# - LogSplitter_Monitor/src/* -> monitor/src/
# - LogSplitter_Monitor/include/* -> monitor/include/
# - LogSplitter_Monitor/lib/* -> monitor/lib/
# - LogSplitter_Monitor/test/* -> monitor/test/
# - LogSplitter_Monitor/README.md -> monitor/README.md
```

### Step 3: Update Include Paths

Both projects need to be updated to use the shared logger component.

#### 3a. Update Controller Include Path

**controller/platformio.ini:**
```ini
[env:uno_r4_wifi]
platform = renesas-ra
board = uno_r4_wifi
framework = arduino
lib_extra_dirs = ../shared
monitor_speed = 115200
# ... rest of configuration
```

#### 3b. Update Monitor Include Path  

**monitor/platformio.ini:**
```ini
[env:uno_r4_wifi]
platform = renesas-ra
board = uno_r4_wifi
framework = arduino
lib_extra_dirs = ../shared
monitor_speed = 115200
# ... rest of configuration
```

### Step 4: Update Source Code

#### 4a. Update Include Statements

**In both controller/src/\*.cpp and monitor/src/\*.cpp:**

Change:
```cpp
#include "logger.h"
```

To:
```cpp
#include "shared/logger/logger.h"
```

#### 4b. Remove Duplicate Logger Files

Since we now have shared logger components, remove the individual copies:

```bash
# Remove from controller
rm controller/include/logger.h
rm controller/src/logger.cpp

# Remove from monitor (when copying)
# Don't copy logger.h and logger.cpp from LogSplitter_Monitor
```

### Step 5: Create Shared Library Structure

The shared directory should be set up as a proper PlatformIO library:

**shared/logger/library.json:**
```json
{
    "name": "LogSplitter-Logger",
    "version": "1.0.0",
    "description": "Unified logging system for LogSplitter projects",
    "keywords": ["logging", "syslog", "arduino"],
    "repository": {
        "type": "git",
        "url": "."
    },
    "authors": [
        {
            "name": "LogSplitter Team"
        }
    ],
    "license": "MIT",
    "homepage": ".",
    "frameworks": ["arduino"],
    "platforms": ["renesas-ra"]
}
```

### Step 6: Update Git Configuration

#### 6a. Update .gitignore

**Root .gitignore:**
```gitignore
# PlatformIO
.pio/
.vscode/

# Build artifacts
controller/.pio/
monitor/.pio/

# IDE files
*.workspace
.vscode/

# Secrets
controller/include/arduino_secrets.h
monitor/include/arduino_secrets.h

# OS files
.DS_Store
Thumbs.db

# Logs
*.log
```

#### 6b. Initialize Git (if needed)

```bash
# If starting fresh git repository
git init
git add .
git commit -m "Initial unified LogSplitter repository structure"

# If migrating existing repository
git add .
git commit -m "Migrate to unified repository structure"
```

### Step 7: Test Build Process

#### 7a. Test Controller Build

```bash
cd controller/
pio run
```

Expected success output:
```
[SUCCESS] Took X.XX seconds
```

#### 7b. Test Monitor Build

```bash
cd ../monitor/
pio run
```

Expected success output:
```
[SUCCESS] Took X.XX seconds
```

### Step 8: Verify Shared Components

Both projects should now use the same logger implementation:

```bash
# Check that both projects reference shared logger
grep -r "shared/logger" controller/src/
grep -r "shared/logger" monitor/src/
```

### Step 9: Update Development Workflow

#### 9a. Building Individual Components

```bash
# Build controller only
cd controller && pio run

# Build monitor only  
cd monitor && pio run

# Build both from root
make build-all  # (if you create a Makefile)
```

#### 9b. Uploading Firmware

```bash
# Upload to controller (connect controller Arduino)
cd controller && pio run --target upload

# Upload to monitor (connect monitor Arduino)  
cd monitor && pio run --target upload
```

#### 9c. Monitoring Serial Output

```bash
# Monitor controller
cd controller && pio device monitor

# Monitor monitor unit
cd monitor && pio device monitor
```

## Verification Checklist

- [ ] **Repository Structure**: Unified directory layout created
- [ ] **Controller Build**: `cd controller && pio run` succeeds
- [ ] **Monitor Build**: `cd monitor && pio run` succeeds  
- [ ] **Shared Logger**: Both projects use `shared/logger/logger.h`
- [ ] **Documentation**: Updated README.md and docs/ structure
- [ ] **Git History**: Proper commit history maintained
- [ ] **Secrets**: arduino_secrets.h files properly configured
- [ ] **Functionality**: Both units operate correctly after migration

## Troubleshooting

### Build Errors

**Include not found errors:**
```bash
# Check lib_extra_dirs in platformio.ini
lib_extra_dirs = ../shared
```

**Logger not found:**
```bash
# Verify shared logger exists
ls -la shared/logger/
# Should show logger.h and logger.cpp
```

### Runtime Issues

**Logger not initializing:**
```cpp
// Ensure NetworkManager is initialized first
if (networkManager.begin()) {
    Logger::begin(&networkManager);  // Pass NetworkManager instance
}
```

**Different log formats:**
- Both units should now produce identical syslog message formats
- Verify shared logger is being used in both projects

### Git Issues

**Large repository size:**
```bash
# Remove .pio directories from git
echo ".pio/" >> .gitignore
git rm -r --cached controller/.pio monitor/.pio
git commit -m "Remove build artifacts from git"
```

## Benefits After Migration

### Development Benefits
- **Single Repository**: Easier version control and release management
- **Shared Components**: Logger changes benefit both projects immediately
- **Unified Documentation**: Single source of truth for system architecture
- **Consistent Build Process**: Standardized development workflow

### Operational Benefits  
- **Synchronized Releases**: Deploy both components with matching versions
- **Unified Monitoring**: Consistent log formats and command interfaces
- **Easier Maintenance**: Single repository to update and maintain
- **Better Collaboration**: Centralized development and issue tracking

### Future Enhancements
- **Shared Network Library**: Extract common networking code
- **Common Constants**: Unified configuration management
- **Integrated Testing**: Cross-component system tests
- **Documentation**: Comprehensive system documentation

## Next Steps

After completing the migration:

1. **Test Complete System**: Deploy both units and verify operation
2. **Update CI/CD**: Configure automated builds for both components
3. **Documentation Review**: Ensure all documentation reflects new structure
4. **Team Training**: Update development procedures for unified repository

---

This migration provides the foundation for a more maintainable, scalable, and professional LogSplitter development process.