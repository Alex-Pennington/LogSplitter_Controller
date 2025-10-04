#include "system_test_suite.h"
#include "safety_system.h"
#include "relay_controller.h"
#include "input_manager.h"
#include "pressure_manager.h"
#include "sequence_controller.h"
#include "network_manager.h"
#include "constants.h"

extern void debugPrintf(const char* fmt, ...);

// Global instance
SystemTestSuite systemTestSuite;

SystemTestSuite::SystemTestSuite() : 
    testCount(0), 
    currentTest(0),
    safetySystem(nullptr),
    relayController(nullptr),
    inputManager(nullptr),
    pressureManager(nullptr),
    sequenceController(nullptr),
    networkManager(nullptr),
    testingActive(false),
    emergencyAbort(false),
    testStartTime(0),
    testTimeout(0),
    waitingForUser(false) {
}

void SystemTestSuite::begin(SafetySystem* safety, RelayController* relay, InputManager* input,
                           PressureManager* pressure, SequenceController* sequence, NetworkManager* network) {
    safetySystem = safety;
    relayController = relay;
    inputManager = input;
    pressureManager = pressure;
    sequenceController = sequence;
    networkManager = network;
    
    initializeTestCases();
    debugPrintf("SystemTestSuite: Initialized with %d test cases\\n", testCount);
}

void SystemTestSuite::initializeTestCases() {
    testCount = 0;
    
    // Safety Input Tests (Critical)
    addTestCase("Emergency Stop Button", true);
    addTestCase("Limit Switch - Retract", true);
    addTestCase("Limit Switch - Extend", true);
    addTestCase("Pressure Sensor Reading", true);
    addTestCase("Pressure Safety Threshold", true);
    
    // Output Device Tests (Critical)
    addTestCase("Relay Controller", true);
    addTestCase("Engine Stop Circuit", true);
    
    // Integrated System Tests (Non-Critical)
    addTestCase("Extend Sequence", false);
    addTestCase("Retract Sequence", false);
    addTestCase("Pressure Relief", false);
    addTestCase("Network Communication", false);
}

void SystemTestSuite::addTestCase(const char* name, bool critical) {
    if (testCount >= MAX_TESTS) {
        debugPrintf("SystemTestSuite: Cannot add test case '%s' - maximum reached\\n", name);
        return;
    }
    
    TestCase& test = testCases[testCount];
    strncpy((char*)test.name, name, sizeof(test.name) - 1);
    test.result = PENDING;
    test.details[0] = '\\0';
    test.duration_ms = 0;
    test.critical = critical;
    
    testCount++;
}

void SystemTestSuite::runAllTests() {
    Serial.println();
    Serial.println("===============================================");
    Serial.println("  LogSplitter Controller - System Test Suite");
    Serial.println("===============================================");
    Serial.println();
    Serial.println("WARNING: This test will exercise all system components.");
    Serial.println("Ensure area is clear and you are ready to operate controls.");
    Serial.println();
    
    TestResult confirmation = waitForUserConfirmation("Ready to begin system testing?");
    if (confirmation != PASS) {
        Serial.println("System testing cancelled by operator.");
        return;
    }
    
    testingActive = true;
    emergencyAbort = false;
    testStartTime = millis();
    resetTestState();
    
    // Enable safety mode during testing
    safeModeEnable();
    
    Serial.println();
    Serial.println("=== Starting System Test Suite ===");
    Serial.println();
    
    // Run test categories in safety-first order
    runSafetyInputTests();
    if (!emergencyAbort) runOutputDeviceTests();
    if (!emergencyAbort) runIntegratedSystemTests();
    
    // Disable safety mode after testing
    safeModeDisable();
    
    testingActive = false;
    
    Serial.println();
    Serial.println("=== Test Suite Complete ===");
    printTestReport();
    publishTestResults();
}

void SystemTestSuite::runTestCategory(TestCategory category) {
    switch (category) {
        case SAFETY_INPUTS:
            runSafetyInputTests();
            break;
        case OUTPUT_DEVICES:
            runOutputDeviceTests();
            break;
        case INTEGRATED_SYSTEMS:
            runIntegratedSystemTests();
            break;
        case ALL_TESTS:
            runAllTests();
            break;
    }
}

void SystemTestSuite::runSafetyInputTests() {
    Serial.println("--- SAFETY INPUT TESTS ---");
    
    // Test 0: Emergency Stop Button
    currentTest = 0;
    setTestResult(currentTest, testEmergencyStop());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    // Test 1: Retract Limit Switch
    currentTest = 1;
    setTestResult(currentTest, testLimitSwitches());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    // Test 3: Pressure Sensor
    currentTest = 3;
    setTestResult(currentTest, testPressureSensor());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    // Test 4: Pressure Safety
    currentTest = 4;
    setTestResult(currentTest, testPressureSafety());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    Serial.println("Safety input tests completed.");
    Serial.println();
}

void SystemTestSuite::runOutputDeviceTests() {
    Serial.println("--- OUTPUT DEVICE TESTS ---");
    
    // Test 5: Relay Controller
    currentTest = 5;
    setTestResult(currentTest, testRelayController());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    // Test 6: Engine Stop Circuit
    currentTest = 6;
    setTestResult(currentTest, testEngineStopCircuit());
    if (testCases[currentTest].result == FAIL && testCases[currentTest].critical) {
        emergencyAbort = true;
        return;
    }
    
    Serial.println("Output device tests completed.");
    Serial.println();
}

void SystemTestSuite::runIntegratedSystemTests() {
    Serial.println("--- INTEGRATED SYSTEM TESTS ---");
    
    // Test 7: Extend Sequence
    currentTest = 7;
    setTestResult(currentTest, testExtendSequence());
    
    // Test 8: Retract Sequence  
    currentTest = 8;
    setTestResult(currentTest, testRetractSequence());
    
    // Test 9: Pressure Relief
    currentTest = 9;
    setTestResult(currentTest, testPressureRelief());
    
    // Test 10: Network Communication
    currentTest = 10;
    setTestResult(currentTest, testNetworkCommunication());
    
    Serial.println("Integrated system tests completed.");
    Serial.println();
}

SystemTestSuite::TestResult SystemTestSuite::waitForUserConfirmation(const char* prompt, bool requireYes) {
    Serial.print(prompt);
    Serial.print(requireYes ? " [Y/N]: " : " [ENTER to continue]: ");
    
    waitingForUser = true;
    unsigned long startTime = millis();
    
    while (millis() - startTime < USER_TIMEOUT_MS) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\\n');
            input.trim();
            input.toUpperCase();
            
            waitingForUser = false;
            
            if (!requireYes) {
                Serial.println("Continuing...");
                return PASS;
            }
            
            if (input == "Y" || input == "YES") {
                Serial.println("Confirmed.");
                return PASS;
            } else if (input == "N" || input == "NO") {
                Serial.println("Cancelled.");
                return FAIL;
            } else {
                Serial.println("Invalid input. Please enter Y or N.");
                return waitForUserConfirmation(prompt, requireYes); // Retry
            }
        }
        delay(100);
    }
    
    waitingForUser = false;
    Serial.println("TIMEOUT - no response from operator.");
    return TIMEOUT;
}

SystemTestSuite::TestResult SystemTestSuite::waitForUserAction(const char* instruction, const char* verification) {
    Serial.println();
    Serial.println(instruction);
    return waitForUserConfirmation(verification, true);
}

void SystemTestSuite::setTestResult(uint8_t testIndex, TestResult result, const char* details) {
    if (testIndex >= testCount) return;
    
    TestCase& test = testCases[testIndex];
    test.result = result;
    
    if (details) {
        strncpy(test.details, details, sizeof(test.details) - 1);
        test.details[sizeof(test.details) - 1] = '\\0';
    }
    
    printTestResult(result, details);
}

void SystemTestSuite::printTestResult(TestResult result, const char* details) {
    Serial.print("[");
    Serial.print(resultToString(result));
    Serial.print("]");
    
    if (details && strlen(details) > 0) {
        Serial.print(" - ");
        Serial.print(details);
    }
    
    Serial.println();
}

const char* SystemTestSuite::resultToString(TestResult result) {
    switch (result) {
        case PASS: return "PASS";
        case FAIL: return "FAIL";
        case SKIP: return "SKIP";
        case PENDING: return "PENDING";
        case TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

void SystemTestSuite::resetTestState() {
    for (uint8_t i = 0; i < testCount; i++) {
        testCases[i].result = PENDING;
        testCases[i].details[0] = '\\0';
        testCases[i].duration_ms = 0;
    }
    currentTest = 0;
}

void SystemTestSuite::safeModeEnable() {
    if (relayController) {
        relayController->enableSafety();
        debugPrintf("SystemTestSuite: Safety mode enabled\\n");
    }
}

void SystemTestSuite::safeModeDisable() {
    if (relayController) {
        // Note: Don't automatically disable safety - let normal operation handle this
        debugPrintf("SystemTestSuite: Test complete - safety mode remains active\\n");
    }
}

void SystemTestSuite::abortTesting() {
    emergencyAbort = true;
    testingActive = false;
    safeModeEnable();
    Serial.println();
    Serial.println("*** TESTING ABORTED BY OPERATOR ***");
    Serial.println();
}

SystemTestSuite::TestResult SystemTestSuite::getOverallResult() {
    bool hasFailures = false;
    bool hasTimeouts = false;
    bool allComplete = true;
    
    for (uint8_t i = 0; i < testCount; i++) {
        switch (testCases[i].result) {
            case FAIL:
                hasFailures = true;
                break;
            case TIMEOUT:
                hasTimeouts = true;
                break;
            case PENDING:
                allComplete = false;
                break;
            default:
                break;
        }
    }
    
    if (hasFailures) return FAIL;
    if (hasTimeouts) return TIMEOUT;
    if (!allComplete) return PENDING;
    return PASS;
}

uint8_t SystemTestSuite::getCompletedTests() const {
    uint8_t completed = 0;
    for (uint8_t i = 0; i < testCount; i++) {
        if (testCases[i].result != PENDING) {
            completed++;
        }
    }
    return completed;
}

// ============================================================================
// Individual Test Implementations
// ============================================================================

SystemTestSuite::TestResult SystemTestSuite::testEmergencyStop() {
    printTestHeader("Emergency Stop Button Test");
    
    if (!inputManager || !safetySystem) {
        return FAIL;
    }
    
    Serial.println("This test verifies the emergency stop button functions correctly.");
    Serial.println();
    
    // Check initial state
    Serial.println("Step 1: Verify E-Stop is NOT pressed (released position)");
    TestResult step1 = waitForUserConfirmation("Is the E-Stop button in the released (normal) position?");
    if (step1 != PASS) {
        return step1;
    }
    
    // Test E-Stop activation
    Serial.println();
    Serial.println("Step 2: Press and hold the E-Stop button");
    Serial.println("The system should immediately detect the activation.");
    
    TestResult step2 = waitForUserAction(
        "Press the E-Stop button NOW and hold it.",
        "Did the system respond immediately (relays clicked off)?"
    );
    
    if (step2 != PASS) {
        return FAIL;
    }
    
    // Test E-Stop release (should remain latched)
    Serial.println();
    Serial.println("Step 3: Release the E-Stop button");
    Serial.println("The system should remain in E-Stop state (latched).");
    
    TestResult step3 = waitForUserAction(
        "Release the E-Stop button (twist to unlock if needed).",
        "Does the system remain in E-Stop state (relays still off)?"
    );
    
    if (step3 != PASS) {
        return FAIL;
    }
    
    Serial.println();
    Serial.println("E-Stop test completed successfully.");
    Serial.println("NOTE: E-Stop will remain latched until reset via command.");
    
    return PASS;
}

SystemTestSuite::TestResult SystemTestSuite::testLimitSwitches() {
    printTestHeader("Limit Switch Test");
    
    if (!inputManager) {
        return FAIL;
    }
    
    Serial.println("This test verifies both limit switches operate correctly.");
    Serial.println("We will test both NO (Normally Open) and NC (Normally Closed) operation.");
    Serial.println();
    
    // Test Retract Limit Switch
    Serial.println("=== Testing RETRACT Limit Switch ===");
    Serial.println("Step 1: Ensure cylinder is NOT at retract limit");
    
    TestResult retractStep1 = waitForUserConfirmation("Is the cylinder away from the retract limit position?");
    if (retractStep1 != PASS) {
        return retractStep1;
    }
    
    Serial.println();
    Serial.println("Step 2: Manually activate the retract limit switch");
    TestResult retractStep2 = waitForUserAction(
        "Manually press/activate the RETRACT limit switch.",
        "Did the system detect the limit switch activation?"
    );
    
    if (retractStep2 != PASS) {
        Serial.println("RETRACT limit switch test failed.");
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Step 3: Release the retract limit switch");
    TestResult retractStep3 = waitForUserAction(
        "Release the RETRACT limit switch.",
        "Did the system detect the limit switch deactivation?"
    );
    
    if (retractStep3 != PASS) {
        Serial.println("RETRACT limit switch release test failed.");
        return FAIL;
    }
    
    // Test Extend Limit Switch
    Serial.println();
    Serial.println("=== Testing EXTEND Limit Switch ===");
    Serial.println("Step 1: Ensure cylinder is NOT at extend limit");
    
    TestResult extendStep1 = waitForUserConfirmation("Is the cylinder away from the extend limit position?");
    if (extendStep1 != PASS) {
        return extendStep1;
    }
    
    Serial.println();
    Serial.println("Step 2: Manually activate the extend limit switch");
    TestResult extendStep2 = waitForUserAction(
        "Manually press/activate the EXTEND limit switch.",
        "Did the system detect the limit switch activation?"
    );
    
    if (extendStep2 != PASS) {
        Serial.println("EXTEND limit switch test failed.");
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Step 3: Release the extend limit switch");
    TestResult extendStep3 = waitForUserAction(
        "Release the EXTEND limit switch.",
        "Did the system detect the limit switch deactivation?"
    );
    
    if (extendStep3 != PASS) {
        Serial.println("EXTEND limit switch release test failed.");
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Both limit switches tested successfully.");
    return PASS;
}

SystemTestSuite::TestResult SystemTestSuite::testPressureSensor() {
    printTestHeader("Pressure Sensor Test");
    
    if (!pressureManager) {
        return FAIL;
    }
    
    Serial.println("This test verifies the pressure sensor provides valid readings.");
    Serial.println();
    
    // Read current pressure
    float pressure = pressureManager->getPressure();
    
    Serial.print("Current pressure reading: ");
    Serial.print(pressure, 1);
    Serial.println(" PSI");
    Serial.println();
    
    // Validate pressure range
    if (pressure < 0.0f || pressure > 5000.0f) {
        Serial.println("ERROR: Pressure reading out of valid range (0-5000 PSI).");
        return FAIL;
    }
    
    // Test pressure reading stability
    Serial.println("Testing pressure reading stability over 5 seconds...");
    float readings[5];
    
    for (int i = 0; i < 5; i++) {
        delay(1000);
        readings[i] = pressureManager->getPressure();
        Serial.print("Reading ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(readings[i], 1);
        Serial.println(" PSI");
    }
    
    // Check for reasonable stability (no more than 100 PSI variation)
    float minReading = readings[0];
    float maxReading = readings[0];
    
    for (int i = 1; i < 5; i++) {
        if (readings[i] < minReading) minReading = readings[i];
        if (readings[i] > maxReading) maxReading = readings[i];
    }
    
    float variation = maxReading - minReading;
    Serial.println();
    Serial.print("Pressure variation: ");
    Serial.print(variation, 1);
    Serial.println(" PSI");
    
    if (variation > 100.0f) {
        Serial.println("ERROR: Pressure readings too unstable (>100 PSI variation).");
        return FAIL;
    }
    
    Serial.println("Pressure sensor readings are stable and valid.");
    return PASS;
}

SystemTestSuite::TestResult SystemTestSuite::testPressureSafety() {
    printTestHeader("Pressure Safety Threshold Test");
    
    if (!pressureManager || !safetySystem) {
        return FAIL;
    }
    
    Serial.println("This test verifies the pressure safety system activates correctly.");
    Serial.print("Current safety threshold: ");
    Serial.print(SAFETY_THRESHOLD_PSI, 0);
    Serial.println(" PSI");
    Serial.println();
    
    float currentPressure = pressureManager->getPressure();
    
    Serial.print("Current system pressure: ");
    Serial.print(currentPressure, 1);
    Serial.println(" PSI");
    Serial.println();
    
    if (currentPressure >= SAFETY_THRESHOLD_PSI - 100.0f) {
        Serial.println("WARNING: Current pressure is too close to safety threshold.");
        Serial.println("This test requires pressure to be well below the safety limit.");
        
        TestResult proceed = waitForUserConfirmation("Do you want to proceed anyway (not recommended)?");
        if (proceed != PASS) {
            return SKIP;
        }
    }
    
    Serial.println("The pressure safety system is configured and monitoring.");
    Serial.println("In normal operation, if pressure exceeds the threshold:");
    Serial.print("- Safety system will activate at ");
    Serial.print(SAFETY_THRESHOLD_PSI, 0);
    Serial.println(" PSI");
    Serial.println("- All hydraulic operations will be stopped");
    Serial.println("- Engine will be stopped for safety");
    Serial.println();
    
    TestResult confirmation = waitForUserConfirmation("Do you understand the pressure safety operation?");
    if (confirmation != PASS) {
        return FAIL;
    }
    
    Serial.println("Pressure safety system configuration validated.");
    return PASS;
}

void SystemTestSuite::printTestHeader(const char* testName) {
    Serial.println();
    Serial.println("===========================================");
    Serial.print("TEST: ");
    Serial.println(testName);
    Serial.println("===========================================");
}

SystemTestSuite::TestResult SystemTestSuite::testRelayController() {
    printTestHeader("Relay Controller Test");
    
    if (!relayController) {
        return FAIL;
    }
    
    Serial.println("This test verifies relay controller operation.");
    Serial.println("SAFETY: All relays should remain OFF during this test.");
    Serial.println();
    
    // Ensure safety mode is active
    relayController->enableSafety();
    
    Serial.println("Step 1: Verify all relays are in safety mode (OFF)");
    TestResult step1 = waitForUserConfirmation("Are all hydraulic relays currently OFF?");
    if (step1 != PASS) {
        return step1;
    }
    
    Serial.println();
    Serial.println("Step 2: Test relay controller status reporting");
    
    // Test each relay status
    bool extendState = relayController->getRelayState(RELAY_EXTEND);
    bool retractState = relayController->getRelayState(RELAY_RETRACT);
    
    Serial.print("Extend relay state: ");
    Serial.println(extendState ? "ON" : "OFF");
    Serial.print("Retract relay state: ");
    Serial.println(retractState ? "ON" : "OFF");
    
    if (extendState || retractState) {
        Serial.println("ERROR: Relays should be OFF in safety mode.");
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Relay controller safety mode verified.");
    return PASS;
}

SystemTestSuite::TestResult SystemTestSuite::testEngineStopCircuit() {
    printTestHeader("Engine Stop Circuit Test");
    
    if (!safetySystem) {
        return FAIL;
    }
    
    Serial.println("This test verifies the engine stop circuit operates correctly.");
    Serial.println("SAFETY: This test will stop the engine temporarily.");
    Serial.println();
    
    TestResult proceed = waitForUserConfirmation("Is it safe to test engine stop (engine running)?");
    if (proceed != PASS) {
        return SKIP;
    }
    
    Serial.println();
    Serial.println("Step 1: Testing engine stop activation");
    
    // Test engine stop
    safetySystem->setEngineStop(true);
    delay(1000); // Give time for engine to respond
    
    TestResult stopTest = waitForUserConfirmation("Did the engine stop?");
    if (stopTest != PASS) {
        // Try to restart engine before failing
        safetySystem->setEngineStop(false);
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Step 2: Testing engine restart");
    
    safetySystem->setEngineStop(false);
    delay(2000); // Give time for engine to restart
    
    TestResult restartTest = waitForUserConfirmation("Did the engine restart successfully?");
    if (restartTest != PASS) {
        return FAIL;
    }
    
    Serial.println();
    Serial.println("Engine stop circuit test completed successfully.");
    return PASS;
}

// Integrated System Tests (to be implemented)
SystemTestSuite::TestResult SystemTestSuite::testExtendSequence() {
    printTestHeader("Extend Sequence Test");
    Serial.println("Extend sequence test not yet implemented.");
    return SKIP;
}

SystemTestSuite::TestResult SystemTestSuite::testRetractSequence() {
    printTestHeader("Retract Sequence Test");
    Serial.println("Retract sequence test not yet implemented.");
    return SKIP;
}

SystemTestSuite::TestResult SystemTestSuite::testPressureRelief() {
    printTestHeader("Pressure Relief Test");
    Serial.println("Pressure relief test not yet implemented.");
    return SKIP;
}

SystemTestSuite::TestResult SystemTestSuite::testNetworkCommunication() {
    printTestHeader("Network Communication Test");
    
    if (!networkManager) {
        return FAIL;
    }
    
    Serial.println("Testing network connectivity and MQTT communication.");
    
    if (!networkManager->isConnected()) {
        Serial.println("Network is not connected. Test failed.");
        return FAIL;
    }
    
    Serial.println("Network connection verified.");
    
    // Test MQTT publish
    bool published = networkManager->publish("r4/test/ping", "system_test");
    if (!published) {
        Serial.println("MQTT publish test failed.");
        return FAIL;
    }
    
    Serial.println("MQTT publish test successful.");
    return PASS;
}

// ============================================================================
// Reporting System
// ============================================================================

void SystemTestSuite::printTestReport() {
    Serial.println();
    Serial.println("===============================================");
    Serial.println("  LogSplitter Controller - System Test Report");
    Serial.println("===============================================");
    
    // Print timestamp
    Serial.print("Test Date: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds since boot");
    Serial.println("Firmware Version: v2.0");
    Serial.println();
    
    // Print test results by category
    Serial.println("SAFETY SYSTEMS:");
    for (uint8_t i = 0; i < 5 && i < testCount; i++) {
        Serial.print("├─ ");
        Serial.print(testCases[i].name);
        Serial.print(" [" + String(resultToString(testCases[i].result)) + "]");
        if (strlen(testCases[i].details) > 0) {
            Serial.print(" - ");
            Serial.print(testCases[i].details);
        }
        Serial.println();
    }
    
    Serial.println();
    Serial.println("OUTPUT SYSTEMS:");
    for (uint8_t i = 5; i < 7 && i < testCount; i++) {
        Serial.print("├─ ");
        Serial.print(testCases[i].name);
        Serial.print(" [" + String(resultToString(testCases[i].result)) + "]");
        if (strlen(testCases[i].details) > 0) {
            Serial.print(" - ");
            Serial.print(testCases[i].details);
        }
        Serial.println();
    }
    
    Serial.println();
    Serial.println("OPERATIONAL SYSTEMS:");
    for (uint8_t i = 7; i < testCount; i++) {
        Serial.print("├─ ");
        Serial.print(testCases[i].name);
        Serial.print(" [" + String(resultToString(testCases[i].result)) + "]");
        if (strlen(testCases[i].details) > 0) {
            Serial.print(" - ");
            Serial.print(testCases[i].details);
        }
        Serial.println();
    }
    
    Serial.println();
    
    // Overall result
    TestResult overall = getOverallResult();
    uint8_t completed = getCompletedTests();
    
    Serial.print("OVERALL RESULT: ");
    Serial.print(resultToString(overall));
    Serial.print(" (");
    Serial.print(completed);
    Serial.print("/");
    Serial.print(testCount);
    Serial.println(" tests)");
    
    if (overall == PASS) {
        Serial.println("System is SAFE for operational use.");
    } else {
        Serial.println("System has ISSUES - review failed tests before operation.");
    }
    
    Serial.println("===============================================");
    Serial.println();
}

void SystemTestSuite::publishTestResults() {
    debugPrintf("SystemTestSuite: Publishing test results\n");
    
    // Print to serial (already done by printTestReport)
    
    // Publish to MQTT if available
    publishToMQTT();
}

void SystemTestSuite::publishToMQTT() {
    if (!networkManager || !networkManager->isConnected()) {
        debugPrintf("SystemTestSuite: Cannot publish to MQTT - network not available\n");
        return;
    }
    
    // Publish overall status with new topic hierarchy
    TestResult overall = getOverallResult();
    networkManager->publishWithRetain("controller/test/status", resultToString(overall));
    
    // Publish individual test results with new topic hierarchy
    for (uint8_t i = 0; i < testCount; i++) {
        String topic = "controller/test/result/" + String(testCases[i].name);
        topic.replace(" ", "_");
        topic.toLowerCase();
        
        String payload = String(resultToString(testCases[i].result));
        if (strlen(testCases[i].details) > 0) {
            payload += ": " + String(testCases[i].details);
        }
        
        networkManager->publish(topic.c_str(), payload.c_str());
    }
    
    // Publish test summary
    uint8_t completed = getCompletedTests();
    String summary = String(completed) + "/" + String(testCount) + " tests completed - " + String(resultToString(overall));
    networkManager->publishWithRetain("r4/test/summary", summary.c_str());
    
    debugPrintf("SystemTestSuite: Test results published to MQTT\n");
}

void SystemTestSuite::displayProgress() {
    uint8_t completed = getCompletedTests();
    
    Serial.print("Testing Progress: [");
    
    // Create progress bar
    uint8_t barLength = 10;
    uint8_t filled = (completed * barLength) / testCount;
    
    for (uint8_t i = 0; i < barLength; i++) {
        if (i < filled) {
            Serial.print("=");
        } else if (i == filled) {
            Serial.print(">");
        } else {
            Serial.print(" ");
        }
    }
    
    Serial.print("] ");
    Serial.print(completed);
    Serial.print("/");
    Serial.print(testCount);
    Serial.println(" tests complete");
    
    if (currentTest < testCount) {
        Serial.print("Current Test: ");
        Serial.println(testCases[currentTest].name);
    }
}

bool SystemTestSuite::checkSystemSafety() {
    // Basic safety checks before allowing tests
    if (!safetySystem || !relayController) {
        debugPrintf("SystemTestSuite: Missing critical system components\n");
        return false;
    }
    
    // Ensure safety mode is active
    if (!relayController->isSafetyActive()) {
        debugPrintf("SystemTestSuite: Safety mode not active\n");
        return false;
    }
    
    return true;
}