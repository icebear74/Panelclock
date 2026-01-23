#ifndef FRAGMENTATIONMONITOR_HPP
#define FRAGMENTATIONMONITOR_HPP

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Enable/disable fragmentation monitoring
// Set to 0 to disable completely (no code compiled)
#ifndef ENABLE_FRAG_MONITOR
#define ENABLE_FRAG_MONITOR 1
#endif

// Maximum number of operations to track in FIFO buffer (stored in PSRAM!)
#define FRAG_MONITOR_BUFFER_SIZE 100

// Fragmentation detection thresholds
#define FRAG_THRESHOLD_PERCENT 50
#define FRAG_MIN_FREE_BYTES 10000
#define FRAG_PERSIST_TIME_MS 5000

// Degradation detection - detect NEW fragmentation over time
#define FRAG_DEGRADATION_THRESHOLD_PERCENT 20  // Alert if largest_block degrades by 20%
#define FRAG_BASELINE_UPDATE_INTERVAL_MS 60000  // Update baseline every 60s when stable

// Severity thresholds for adaptive response
#define FRAG_CRITICAL_THRESHOLD_BYTES 15360   // <15KB = critical (immediate dump)
#define FRAG_SEVERE_THRESHOLD_BYTES 20480     // <20KB = severe (bypass cooldown)
#define FRAG_WARNING_THRESHOLD_BYTES 25600    // <25KB = warning (increase frequency)
#define FRAG_SEVERE_DEGRADATION_PERCENT 50    // >50% degradation = severe (bypass cooldown)

// Filesystem protection
#define FRAG_MIN_FS_FREE_BYTES 51200  // 50KB minimum free space (don't write if below this)
#define FRAG_MAX_LOG_FILES 10          // Maximum number of log files to keep
#define FRAG_DUMP_COOLDOWN_MS 300000   // 5 minutes cooldown between dumps (normal)
#define FRAG_SEVERE_COOLDOWN_MS 30000  // 30 seconds cooldown for severe cases

#if ENABLE_FRAG_MONITOR

// Operation log entry - IMPORTANT: Stored in PSRAM to avoid heap fragmentation!
struct MemoryOperation {
    uint32_t timestamp;      // millis() when operation occurred
    char module[16];         // Module name (extracted from __FILE__)
    char operation[32];      // Operation description
    int line;                // Line number in source file
    uint32_t heapFree;       // Free heap at time of operation
    uint32_t largestBlock;   // Largest free block at time of operation
};

/**
 * @brief FragmentationMonitor - Standalone module for heap fragmentation monitoring
 * 
 * This module runs independently with its own periodicTick() callback.
 * Modules only need to call LOG_MEM_OP() at critical points.
 * When fragmentation persists for FRAG_PERSIST_TIME_MS, dumps log to filesystem.
 * 
 * Detection strategy: Tracks largest_free_block baseline and detects degradation over time
 */
class FragmentationMonitor {
public:
    FragmentationMonitor();
    ~FragmentationMonitor();
    
    /**
     * @brief Initialize the monitor (create buffer, set baseline)
     */
    void begin();
    
    /**
     * @brief Clean up mem_debug directory (called AFTER LittleFS init)
     */
    void cleanupDirectoryOnStartup();
    
    /**
     * @brief Periodic check for fragmentation (called every 100ms from Application)
     */
    void periodicTick();
    
    /**
     * @brief Log a memory operation to the FIFO buffer
     * @param file Source file (use __FILE__)
     * @param line Line number (use __LINE__)
     * @param operation Operation description
     * @param forceLog Force logging even if not fragmented (for startup logging)
     */
    static void logOperation(const char* file, int line, const char* operation, bool forceLog = false);
    
    /**
     * @brief Check if heap is currently fragmented (NEW fragmentation detected)
     * @return true if fragmented, false otherwise
     */
    static bool isFragmented();
    
    /**
     * @brief Get current heap statistics
     */
    static void getHeapStats(uint32_t& free, uint32_t& largestBlock, uint32_t& freeBlocks);
    
    /**
     * @brief Update baseline (called periodically when no fragmentation detected)
     */
    void updateBaseline();

private:
    static MemoryOperation* operationBuffer;  // PSRAM buffer for operations
    static int bufferIndex;                   // Current write position in circular buffer
    static int operationCount;                // Total operations logged (for wraparound tracking)
    static SemaphoreHandle_t bufferMutex;     // Protect buffer access
    
    // Baseline tracking for detecting NEW fragmentation
    static uint32_t baselineLargestBlock;     // Baseline largest contiguous block
    static uint32_t baselineFreeBytes;        // Baseline total free bytes
    static unsigned long baselineUpdateTime;  // Last time baseline was updated
    
    unsigned long fragmentedSince;            // millis() when fragmentation started (0 if not fragmented)
    bool lastFragmentedState;                 // Previous fragmentation state
    unsigned long lastBaselineUpdate;         // Last baseline update time
    unsigned long lastDumpTime;               // Last time we dumped a log file (for cooldown)
    uint32_t fragmentedAtLargestBlock;        // largestBlock value when fragmentation was first detected
    uint32_t lastDumpedLargestBlock;          // largestBlock value when we last dumped (to detect worsening)
    
    /**
     * @brief Dump current buffer and heap state to filesystem
     */
    void dumpToFile();
    
    /**
     * @brief Check if filesystem has enough free space for logging
     * @return true if enough space available, false otherwise
     */
    bool hasEnoughFsSpace();
    
    /**
     * @brief Delete oldest log files to free up space
     * @param targetFreeSpace Target free space in bytes to achieve
     */
    void cleanupOldLogs(size_t targetFreeSpace);
    
    /**
     * @brief Extract short filename from full path
     */
    static const char* getShortFilename(const char* file);
};

// Global instance (defined in .cpp)
extern FragmentationMonitor* g_FragMonitor;

// Convenience macros for logging operations
#define LOG_MEM_OP(operation) \
    do { if (g_FragMonitor) FragmentationMonitor::logOperation(__FILE__, __LINE__, operation); } while(0)

#define LOG_MEM_OP_FORCE(operation) \
    do { if (g_FragMonitor) FragmentationMonitor::logOperation(__FILE__, __LINE__, operation, true); } while(0)

#else
// If monitoring disabled, macros do nothing
#define LOG_MEM_OP(operation) do { } while(0)
#define LOG_MEM_OP_FORCE(operation) do { } while(0)
#endif // ENABLE_FRAG_MONITOR

#endif // FRAGMENTATIONMONITOR_HPP
