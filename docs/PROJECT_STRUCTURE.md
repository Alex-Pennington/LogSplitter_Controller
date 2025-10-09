# Project Structure Summary

## LogSplitter_Controller (Original Complex Project)
```
LogSplitter_Controller/
├── src/                    # Main controller program
├── monitor/               # Monitor program (old location)  
├── shared/                # Shared libraries
├── scripts/               # Build scripts
├── docs/                  # Documentation
└── Many config files...   # Complex multi-program setup
```

## monitor_standalone/ (Clean Monitor Project)
```
monitor_standalone/
├── src/                   # All monitor source code
├── include/               # Header files
├── .vscode/              # VS Code tasks (build/upload)
├── platformio.ini        # Simple PlatformIO config
└── README.md             # Clean documentation
```

## Recommendations

### For the Monitor Project (Weight Sensor Work)
**Use:** `monitor_standalone/` 
- ✅ Clean, simple structure
- ✅ Working VS Code tasks  
- ✅ Proper COM39 configuration
- ✅ All monitor code in one place
- ✅ Fixed weight calibration issues

### For the Controller Project
**Use:** `src/` (main controller code)
- Move this to a separate clean directory when ready
- Keep it simple like the monitor project

## Current Status
- **monitor_standalone/** is ready to use
- Weight calibration is working with improved retry logic
- COM39 configured correctly
- VS Code tasks working
- Can build/upload/test independently

## Quick Commands
```bash
cd monitor_standalone
platformio run --target upload    # Build and upload
platformio device monitor         # Serial monitor
telnet 192.168.1.225 23           # Command interface
```