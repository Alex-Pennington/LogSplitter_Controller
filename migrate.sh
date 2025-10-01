#!/bin/bash

# LogSplitter Repository Migration Script
# This script automates the migration from separate projects to unified structure

set -e  # Exit on any error

echo "=========================================="
echo "LogSplitter Repository Migration Script"
echo "=========================================="

# Configuration
CONTROLLER_SRC="../LogSplitter_Controller"
MONITOR_SRC="../LogSplitter_Monitor" 
UNIFIED_ROOT="."

echo "Checking prerequisites..."

# Check if we're in the right directory
if [ ! -f "README.md" ] || [ ! -d "shared" ]; then
    echo "ERROR: Run this script from the unified LogSplitter root directory"
    echo "Expected structure: README.md, shared/, controller/, monitor/, docs/"
    exit 1
fi

# Check if source directories exist
if [ ! -d "$CONTROLLER_SRC" ]; then
    echo "ERROR: Controller source directory not found: $CONTROLLER_SRC"
    echo "Please ensure LogSplitter_Controller is in the parent directory"
    exit 1
fi

if [ ! -d "$MONITOR_SRC" ]; then
    echo "ERROR: Monitor source directory not found: $MONITOR_SRC"
    echo "Please ensure LogSplitter_Monitor is in the parent directory"
    exit 1
fi

echo "✓ Prerequisites check passed"

# Step 1: Create directory structure
echo ""
echo "Step 1: Creating unified directory structure..."

mkdir -p controller/{src,include,lib,test}
mkdir -p monitor/{src,include,lib,test}
mkdir -p shared/{logger,network,constants}
mkdir -p docs

echo "✓ Directory structure created"

# Step 2: Copy Controller files
echo ""
echo "Step 2: Migrating Controller files..."

# Copy main project files
cp "$CONTROLLER_SRC/platformio.ini" controller/
cp "$CONTROLLER_SRC/README_REFACTORED.md" controller/README.md
cp "$CONTROLLER_SRC/COMMANDS.md" controller/
cp "$CONTROLLER_SRC/PINS.md" controller/
cp "$CONTROLLER_SRC/COMPREHENSIVE_REVIEW.md" controller/
cp "$CONTROLLER_SRC/SYSTEM_TEST_SUITE.md" controller/
cp "$CONTROLLER_SRC/ARDUINO_SECRETS_SETUP.md" controller/

# Copy source directories
cp -r "$CONTROLLER_SRC/src/"* controller/src/ 2>/dev/null || true
cp -r "$CONTROLLER_SRC/include/"* controller/include/ 2>/dev/null || true
cp -r "$CONTROLLER_SRC/lib/"* controller/lib/ 2>/dev/null || true
cp -r "$CONTROLLER_SRC/test/"* controller/test/ 2>/dev/null || true

# Copy secrets template
if [ -f "$CONTROLLER_SRC/include/arduino_secrets.h.template" ]; then
    cp "$CONTROLLER_SRC/include/arduino_secrets.h.template" controller/include/
fi

echo "✓ Controller files migrated"

# Step 3: Copy Monitor files
echo ""
echo "Step 3: Migrating Monitor files..."

# Copy main project files
cp "$MONITOR_SRC/platformio.ini" monitor/
cp "$MONITOR_SRC/README.md" monitor/
if [ -f "$MONITOR_SRC/COMMANDS.md" ]; then
    cp "$MONITOR_SRC/COMMANDS.md" monitor/
fi

# Copy source directories
cp -r "$MONITOR_SRC/src/"* monitor/src/ 2>/dev/null || true
cp -r "$MONITOR_SRC/include/"* monitor/include/ 2>/dev/null || true
cp -r "$MONITOR_SRC/lib/"* monitor/lib/ 2>/dev/null || true
cp -r "$MONITOR_SRC/test/"* monitor/test/ 2>/dev/null || true

# Copy secrets template
if [ -f "$MONITOR_SRC/include/arduino_secrets.h.template" ]; then
    cp "$MONITOR_SRC/include/arduino_secrets.h.template" monitor/include/
fi

echo "✓ Monitor files migrated"

# Step 4: Remove duplicate logger files and update includes
echo ""
echo "Step 4: Removing duplicate logger files..."

# Remove logger files from controller
rm -f controller/include/logger.h
rm -f controller/src/logger.cpp

# Remove logger files from monitor  
rm -f monitor/include/logger.h
rm -f monitor/src/logger.cpp

echo "✓ Duplicate logger files removed"

# Step 5: Update PlatformIO configurations
echo ""
echo "Step 5: Updating PlatformIO configurations..."

# Update controller platformio.ini
if ! grep -q "lib_extra_dirs.*shared" controller/platformio.ini; then
    sed -i '/^\[env:uno_r4_wifi\]/a lib_extra_dirs = ../shared' controller/platformio.ini
fi

# Update monitor platformio.ini
if ! grep -q "lib_extra_dirs.*shared" monitor/platformio.ini; then
    sed -i '/^\[env:uno_r4_wifi\]/a lib_extra_dirs = ../shared' monitor/platformio.ini
fi

echo "✓ PlatformIO configurations updated"

# Step 6: Update include statements
echo ""
echo "Step 6: Updating include statements..."

# Update controller includes
find controller/src -name "*.cpp" -exec sed -i 's/#include "logger\.h"/#include "shared\/logger\/logger.h"/g' {} \;
find controller/include -name "*.h" -exec sed -i 's/#include "logger\.h"/#include "shared\/logger\/logger.h"/g' {} \;

# Update monitor includes
find monitor/src -name "*.cpp" -exec sed -i 's/#include "logger\.h"/#include "shared\/logger\/logger.h"/g' {} \;
find monitor/include -name "*.h" -exec sed -i 's/#include "logger\.h"/#include "shared\/logger\/logger.h"/g' {} \;

echo "✓ Include statements updated"

# Step 7: Create .gitignore
echo ""
echo "Step 7: Creating unified .gitignore..."

cat > .gitignore << EOF
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

# Temporary files
*.tmp
*.bak
EOF

echo "✓ .gitignore created"

# Step 8: Test builds
echo ""
echo "Step 8: Testing builds..."

echo "Testing controller build..."
cd controller
if pio run > /dev/null 2>&1; then
    echo "✓ Controller build successful"
else
    echo "⚠ Controller build failed - check configuration manually"
fi
cd ..

echo "Testing monitor build..."
cd monitor  
if pio run > /dev/null 2>&1; then
    echo "✓ Monitor build successful"
else
    echo "⚠ Monitor build failed - check configuration manually"
fi
cd ..

# Step 9: Display migration summary
echo ""
echo "=========================================="
echo "Migration Summary"
echo "=========================================="
echo ""
echo "Unified repository structure created:"
echo "├── README.md              # Main project overview"
echo "├── Makefile              # Build automation"
echo "├── MIGRATION_GUIDE.md    # This migration process"
echo "├── controller/           # Controller project"
echo "│   ├── platformio.ini"
echo "│   ├── src/, include/"
echo "│   └── README.md"
echo "├── monitor/              # Monitor project"
echo "│   ├── platformio.ini"
echo "│   ├── src/, include/"
echo "│   └── README.md"
echo "├── shared/               # Common components"
echo "│   └── logger/           # Unified logging system"
echo "└── docs/                 # Unified documentation"
echo "    ├── DEPLOYMENT_GUIDE.md"
echo "    └── LOGGING_SYSTEM.md"
echo ""

echo "Next Steps:"
echo "1. Configure WiFi credentials:"
echo "   - Copy controller/include/arduino_secrets.h.template to arduino_secrets.h"
echo "   - Copy monitor/include/arduino_secrets.h.template to arduino_secrets.h"
echo "   - Edit both files with your network settings"
echo ""
echo "2. Build both projects:"
echo "   make build-all"
echo ""
echo "3. Upload firmware:"
echo "   make upload-controller  # Connect controller Arduino"
echo "   make upload-monitor     # Connect monitor Arduino"
echo ""
echo "4. Initialize git repository:"
echo "   git add ."
echo "   git commit -m 'Unified LogSplitter repository structure'"
echo ""

echo "Migration completed successfully!"
echo "=========================================="