# Controller-Focused Makefile for LogSplitter Controller System

# Project configuration
PROJECT_NAME = LogSplitter_Controller
PLATFORMIO = C:/Users/rayve/.platformio/penv/Scripts/platformio.exe
WORKSPACE = c:\\Users\\rayve\\OneDrive\\Documents\\PlatformIO\\Projects\\LogSplitter_Controller

# Default environment (can be overridden)
ENV ?= uno_r4_wifi

.PHONY: help build upload monitor deploy clean check test debug deps secrets backup restore info validate

# Default target
help:
	@echo "LogSplitter Controller Development System"
	@echo ""
	@echo "Controller Build Commands:"
	@echo "  build       - Compile controller program"
	@echo "  upload      - Upload controller firmware to Arduino Uno R4 WiFi"
	@echo "  monitor     - Start serial monitor for controller debugging"
	@echo "  deploy      - Complete build, upload, and monitor workflow"
	@echo ""
	@echo "Development Tools:"
	@echo "  clean       - Clean controller build artifacts"
	@echo "  check       - Run static code analysis on controller"
	@echo "  test        - Run controller system tests"
	@echo "  debug       - Build with debug symbols and start monitor"
	@echo ""
	@echo "Configuration Management:"
	@echo "  backup      - Backup controller configuration files"
	@echo "  restore     - List available configuration backups"
	@echo "  secrets     - Setup arduino_secrets.h from template"
	@echo "  deps        - Install/update controller dependencies"
	@echo ""
	@echo "System Information:"
	@echo "  info        - Show controller project information"
	@echo "  validate    - Validate controller workspace setup"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Quick Start Examples:"
	@echo "  make deploy         # Complete controller deployment"
	@echo "  make debug          # Debug controller with monitor"
	@echo "  make check && make build # Analyze and build controller"

# Build controller program
build:
	@echo "[Controller] Building controller program..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" run --environment $(ENV)
	@echo "✓ Controller build completed successfully"

# Upload controller firmware
upload:
	@echo "[Controller] Uploading controller firmware to Arduino Uno R4 WiFi..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" run --environment $(ENV) --target upload
	@echo "✓ Controller firmware uploaded successfully"

# Start controller serial monitor
monitor:
	@echo "[Controller] Starting controller serial monitor..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" device monitor

# Complete controller deployment workflow
deploy: build upload
	@echo "✓ Controller deployment completed - starting monitor..."
	@$(MAKE) monitor

# Clean controller build artifacts
clean:
	@echo "[Controller] Cleaning controller build artifacts..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" run --target clean
	@echo "✓ Controller build artifacts cleaned"

# Run static code analysis on controller
check:
	@echo "[Controller] Running static code analysis on controller..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" check --verbose
	@echo "✓ Controller code analysis completed"

# Run controller system tests
test:
	@echo "[Controller] Running controller system tests..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" test
	@echo "✓ Controller tests completed"

# Debug build with monitor
debug:
	@echo "[Controller] Building controller with debug symbols..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" run --environment $(ENV) --target upload
	@echo "✓ Debug build uploaded - starting monitor..."
	@$(MAKE) monitor

# Install/update controller dependencies
deps:
	@echo "[Controller] Installing/updating controller dependencies..."
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" lib install
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" platform update
	@echo "✓ Controller dependencies updated"

# Setup arduino_secrets.h from template
secrets:
	@echo "[Controller] Setting up arduino_secrets.h..."
	@if not exist "include\\arduino_secrets.h" ( \
		if exist "include\\arduino_secrets.h.template" ( \
			copy "include\\arduino_secrets.h.template" "include\\arduino_secrets.h" && \
			echo "✓ arduino_secrets.h created from template - please configure your WiFi and server settings" \
		) else ( \
			echo "✗ arduino_secrets.h.template not found" \
		) \
	) else ( \
		echo "⚠ arduino_secrets.h already exists" \
	)

# Create controller configuration backup
backup:
	@echo "[Controller] Creating controller configuration backup..."
	@powershell -Command "if (!(Test-Path 'backups')) { New-Item -ItemType Directory -Name 'backups' }"
	@powershell -Command "$$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'; Copy-Item 'include\\arduino_secrets.h' \"backups\\controller_secrets_$$timestamp.h.bak\" -ErrorAction SilentlyContinue"
	@powershell -Command "$$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'; Copy-Item 'platformio.ini' \"backups\\controller_platformio_$$timestamp.ini.bak\" -ErrorAction SilentlyContinue"
	@echo "✓ Controller configuration backup created"

# List available backups
restore:
	@echo "[Controller] Available controller configuration backups:"
	@powershell -Command "if (Test-Path 'backups') { Get-ChildItem 'backups\\controller_*.bak' | Sort-Object LastWriteTime -Descending | ForEach-Object { Write-Host \"  $$($_.Name) - $$($_.LastWriteTime)\" } } else { Write-Host '  No controller backups found' }"

# Show controller project information
info:
	@echo "[Controller] Controller Project Information"
	@echo "Project: $(PROJECT_NAME)"
	@echo "Workspace: $(WORKSPACE)"
	@echo "PlatformIO: $(PLATFORMIO)"
	@echo "Target Environment: $(ENV)"
	@echo "Focus: Arduino Uno R4 WiFi Controller Only"
	@echo ""
	@cd "$(WORKSPACE)" && "$(PLATFORMIO)" system info

# Validate controller workspace setup
validate:
	@echo "[Controller] Validating controller workspace setup..."
	@if not exist "$(WORKSPACE)" (echo "✗ Controller workspace directory not found" && exit 1)
	@if not exist "$(PLATFORMIO)" (echo "✗ PlatformIO executable not found" && exit 1)
	@if not exist "$(WORKSPACE)\\platformio.ini" (echo "✗ Controller platformio.ini not found" && exit 1)
	@if not exist "$(WORKSPACE)\\src" (echo "✗ Controller src directory not found" && exit 1)
	@if not exist "$(WORKSPACE)\\include" (echo "✗ Controller include directory not found" && exit 1)
	@echo "✓ Controller workspace validation passed"