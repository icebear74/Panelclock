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

#if ENABLE_FRAG_MONITOR

// Maximum number of operations to track in FIFO buffer (stored in PSRAM!)
#define FRAG_MONITOR_BUFFER_SIZE 100

// Fragmentation detection thresholds
#define FRAG_THRESHOLD_PERCENT 50
#define FRAG_MIN_FREE_BYTES 10000
#define FRAG_PERSIST_TIME_MS 5000

// Degradation detection - detect NEW fragmentation over time
#define FRAG_DEGRADATION_THRESHOLD_PERCENT 20  // Alert if largest_block degrades by 20%
#define FRAG_BASELINE_UPDATE_INTERVAL_MS 60000  // Update baseline every 60s when stable

// Filesystem protection
#define FRAG_MIN_FS_FREE_BYTES 51200  // 50KB minimum free space (don't write if below this)
#define FRAG_MAX_LOG_FILES 10          // Maximum number of log files to keep

// Operation log entry - IMPORTANT: Stored in PSRAM to avoid heap fragmentation!
struct MemoryOperation {
    uint32_t timestamp;      // millis() when operation occurred
    char module[16];         // Module name (extracted from __FILE__)
    char operation[32];      // Operation description
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
     * @brief Initialize the monitor (create directory, reset state)
     */
    void begin();
    
    /**
     * @brief Periodic check for fragmentation (called every 100ms from Application)
     */
    void periodicTick();
    
    /**
     * @brief Log a memory operation to the FIFO buffer
     * @param file Source file (use __FILE__)
     * @param operation Operation description
     */
    static void logOperation(const char* file, const char* operation);
    
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

// Convenience macro for logging operations
#define LOG_MEM_OP(operation) \
    do { if (g_FragMonitor) FragmentationMonitor::logOperation(__FILE__, operation); } while(0)

#else
// If monitoring disabled, macro does nothing
#define LOG_MEM_OP(operation) do { } while(0)
#endif // ENABLE_FRAG_MONITOR

#endif // FRAGMENTATIONMONITOR_HPP
