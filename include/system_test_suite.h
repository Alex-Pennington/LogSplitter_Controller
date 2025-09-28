#ifndef SYSTEM_TEST_SUITE_H
#define SYSTEM_TEST_SUITE_H

#include <Arduino.h>

// Forward declarations
class SafetySystem;
class RelayController;
class InputManager;
class PressureManager;
class SequenceController;
class NetworkManager;

class SystemTestSuite {
public:
    enum TestResult {
        PASS,
        FAIL, 
        SKIP,
        PENDING,
        TIMEOUT
    };
    
    enum TestCategory {
        SAFETY_INPUTS,
        OUTPUT_DEVICES,
        INTEGRATED_SYSTEMS,
        ALL_TESTS
    };
    
    struct TestCase {
        const char* name;
        TestResult result;
        char details[128];
        unsigned long duration_ms;
        bool critical;  // If true, failure stops all testing
    };
    
private:
    // Test case storage
    static const uint8_t MAX_TESTS = 15;
    TestCase testCases[MAX_TESTS];
    uint8_t testCount;
    uint8_t currentTest;
    
    // System component references
    SafetySystem* safetySystem;
    RelayController* relayController;
    InputManager* inputManager;
    PressureManager* pressureManager;
    SequenceController* sequenceController;
    NetworkManager* networkManager;
    
    // Test state
    bool testingActive;
    bool emergencyAbort;
    unsigned long testStartTime;
    unsigned long testTimeout;
    
    // User interaction
    static const uint16_t USER_TIMEOUT_MS = 30000; // 30 second timeout
    bool waitingForUser;
    
public:
    SystemTestSuite();
    
    // Initialization
    void begin(SafetySystem* safety, RelayController* relay, InputManager* input,
               PressureManager* pressure, SequenceController* sequence, NetworkManager* network);
    
    // Main test execution
    void runAllTests();
    void runTestCategory(TestCategory category);
    void abortTesting();
    
    // Individual test categories
    void runSafetyInputTests();
    void runOutputDeviceTests();
    void runIntegratedSystemTests();
    
    // Individual test methods
    TestResult testEmergencyStop();
    TestResult testLimitSwitches();
    TestResult testPressureSensor();
    TestResult testPressureSafety();
    TestResult testRelayController();
    TestResult testEngineStopCircuit();
    TestResult testExtendSequence();
    TestResult testRetractSequence();
    TestResult testPressureRelief();
    TestResult testNetworkCommunication();
    
    // User interaction
    TestResult waitForUserConfirmation(const char* prompt, bool requireYes = true);
    TestResult waitForUserAction(const char* instruction, const char* verification);
    void displayProgress();
    
    // Result management
    void addTestCase(const char* name, bool critical = false);
    void setTestResult(uint8_t testIndex, TestResult result, const char* details = nullptr);
    TestResult getOverallResult();
    
    // Reporting
    void publishTestResults();
    void printTestReport();
    void publishToMQTT();
    
    // Status
    bool isTestingActive() const { return testingActive; }
    uint8_t getCompletedTests() const;
    uint8_t getTotalTests() const { return testCount; }
    const char* resultToString(TestResult result);
    
private:
    // Helper functions
    void initializeTestCases();
    void resetTestState();
    TestResult readUserInput(unsigned long timeoutMs);
    void printTestHeader(const char* testName);
    void printTestResult(TestResult result, const char* details = nullptr);
    void safeModeEnable();
    void safeModeDisable();
    bool checkSystemSafety();
};

// Global instance declaration
extern SystemTestSuite systemTestSuite;

#endif // SYSTEM_TEST_SUITE_H