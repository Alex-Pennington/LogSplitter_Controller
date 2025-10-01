# LogSplitter Unified Build System

.PHONY: help build-all build-controller build-monitor clean-all clean-controller clean-monitor test upload-controller upload-monitor monitor-controller monitor-monitor

# Default target
help:
	@echo "LogSplitter Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  build-all         - Build both controller and monitor"
	@echo "  build-controller  - Build controller only"
	@echo "  build-monitor     - Build monitor only"
	@echo "  clean-all         - Clean both projects"
	@echo "  clean-controller  - Clean controller build files"
	@echo "  clean-monitor     - Clean monitor build files"
	@echo "  upload-controller - Upload firmware to controller"
	@echo "  upload-monitor    - Upload firmware to monitor"
	@echo "  monitor-controller- Monitor controller serial output"
	@echo "  monitor-monitor   - Monitor monitor serial output"
	@echo "  test              - Run tests for both projects"
	@echo ""
	@echo "Usage examples:"
	@echo "  make build-all"
	@echo "  make upload-controller"
	@echo "  make monitor-monitor"

# Build targets
build-all: build-controller build-monitor

build-controller:
	@echo "Building controller..."
	cd controller && pio run

build-monitor:
	@echo "Building monitor..."
	cd monitor && pio run

# Clean targets
clean-all: clean-controller clean-monitor

clean-controller:
	@echo "Cleaning controller..."
	cd controller && pio run --target clean

clean-monitor:
	@echo "Cleaning monitor..."
	cd monitor && pio run --target clean

# Upload targets
upload-controller:
	@echo "Uploading controller firmware..."
	cd controller && pio run --target upload

upload-monitor:
	@echo "Uploading monitor firmware..."
	cd monitor && pio run --target upload

# Monitor targets
monitor-controller:
	@echo "Monitoring controller serial output..."
	cd controller && pio device monitor

monitor-monitor:
	@echo "Monitoring monitor serial output..."
	cd monitor && pio device monitor

# Test targets
test:
	@echo "Running controller tests..."
	cd controller && pio test
	@echo "Running monitor tests..."
	cd monitor && pio test

# Development helpers
check-deps:
	@echo "Checking PlatformIO installation..."
	@pio --version || (echo "PlatformIO not found. Install with: pip install platformio" && exit 1)

setup: check-deps
	@echo "Setting up LogSplitter development environment..."
	@echo "Checking arduino_secrets.h files..."
	@test -f controller/include/arduino_secrets.h || (echo "Copy controller/include/arduino_secrets.h.template to controller/include/arduino_secrets.h and configure" && exit 1)
	@test -f monitor/include/arduino_secrets.h || (echo "Copy monitor/include/arduino_secrets.h.template to monitor/include/arduino_secrets.h and configure" && exit 1)
	@echo "Environment setup complete!"

# Release management
version:
	@echo "LogSplitter System Version Information:"
	@echo "Repository: $(shell git describe --always --dirty)"
	@echo "Controller: $(shell cd controller && pio run | grep "Flash:" | head -1)"
	@echo "Monitor: $(shell cd monitor && pio run | grep "Flash:" | head -1)"

# Documentation
docs:
	@echo "Opening documentation..."
	@echo "Main README: README.md"
	@echo "Deployment Guide: docs/DEPLOYMENT_GUIDE.md"
	@echo "Logging System: docs/LOGGING_SYSTEM.md"
	@echo "Migration Guide: MIGRATION_GUIDE.md"