#include "FragmentationMonitor.hpp"
#include "MultiLogger.hpp"
#include "GeneralTimeConverter.hpp"
#include <time.h>

// External time converter from Application
extern GeneralTimeConverter* timeConverter;

#if ENABLE_FRAG_MONITOR

// Static members - ALL ALLOCATED IN PSRAM to avoid heap fragmentation!
MemoryOperation* FragmentationMonitor::operationBuffer = nullptr;
int FragmentationMonitor::bufferIndex = 0;
int FragmentationMonitor::operationCount = 0;
SemaphoreHandle_t FragmentationMonitor::bufferMutex = nullptr;

// Baseline tracking for detecting NEW fragmentation
uint32_t FragmentationMonitor::baselineLargestBlock = 0;
uint32_t FragmentationMonitor::baselineFreeBytes = 0;
unsigned long FragmentationMonitor::baselineUpdateTime = 0;

// Global instance
FragmentationMonitor* g_FragMonitor = nullptr;

FragmentationMonitor::FragmentationMonitor() 
    : fragmentedSince(0), lastFragmentedState(false), lastBaselineUpdate(0), lastDumpTime(0),
      fragmentedAtLargestBlock(0), lastDumpedLargestBlock(0), activeLoggingStartTime(0), activeLoggingMode(false), lastActiveLogTime(0) {
}

FragmentationMonitor::~FragmentationMonitor() {
    if (operationBuffer) {
        free(operationBuffer);  // Free PSRAM allocation
        operationBuffer = nullptr;
    }
    if (bufferMutex) {
        vSemaphoreDelete(bufferMutex);
        bufferMutex = nullptr;
    }
}

void FragmentationMonitor::begin() {
    // NOTE: Logging not yet available during early init!
    // Just allocate buffer and create mutex - directory cleanup comes later
    
    // Allocate operation buffer in PSRAM (NOT heap!)
    if (!operationBuffer) {
        operationBuffer = (MemoryOperation*)ps_malloc(sizeof(MemoryOperation) * FRAG_MONITOR_BUFFER_SIZE);
        if (!operationBuffer) {
            return;  // Silent fail - logging not ready yet
        }
        memset(operationBuffer, 0, sizeof(MemoryOperation) * FRAG_MONITOR_BUFFER_SIZE);
    }
    
    // Create mutex for buffer protection
    if (!bufferMutex) {
        bufferMutex = xSemaphoreCreateMutex();
        if (!bufferMutex) {
            return;  // Silent fail - logging not ready yet
        }
    }
    
    // Log startup heap immediately (before any allocations!)
    uint32_t free, largestBlock, freeBlocks;
    getHeapStats(free, largestBlock, freeBlocks);
    
    // Initialize baseline with current values
    baselineLargestBlock = largestBlock;
    baselineFreeBytes = free;
    baselineUpdateTime = millis();
    lastBaselineUpdate = millis();
}

void FragmentationMonitor::cleanupDirectoryOnStartup() {
    // This is called AFTER LittleFS is initialized but BEFORE any modules run
    Log.println("[FragMon] Cleaning up /mem_debug directory...");
    
    // Remove all files in /mem_debug directory if it exists
    // Use the same robust approach as SofaScoreLiveModule's json_debug cleanup
    if (LittleFS.exists("/mem_debug")) {
        Log.println("[FragMon] Removing old files from /mem_debug...");
        File dir = LittleFS.open("/mem_debug", "r");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            int deletedCount = 0;
            while (file) {
                char filename[128];
                snprintf(filename, sizeof(filename), "/mem_debug/%s", file.name());
                file.close();  // IMPORTANT: Close file before deleting!
                if (LittleFS.remove(filename)) {
                    deletedCount++;
                    Log.printf("[FragMon] Deleted: %s\n", filename);
                } else {
                    Log.printf("[FragMon] WARNING: Failed to delete %s\n", filename);
                }
                file = dir.openNextFile();
            }
            dir.close();
            Log.printf("[FragMon] Deleted %d debug files from /mem_debug\n", deletedCount);
            
            // Try to remove the directory itself (should be empty now)
            if (LittleFS.rmdir("/mem_debug")) {
                Log.println("[FragMon] Removed /mem_debug directory");
            } else {
                Log.println("[FragMon] INFO: /mem_debug directory still exists (will be reused)");
            }
        }
    }
    
    // Create fresh directory (or ensure it exists)
    if (!LittleFS.exists("/mem_debug")) {
        if (LittleFS.mkdir("/mem_debug")) {
            Log.println("[FragMon] Created fresh /mem_debug directory");
        } else {
            Log.println("[FragMon] ERROR: Failed to create /mem_debug directory");
        }
    }
    
    // Log initialization complete
    Log.printf("[FragMon] Initialized - Baseline: largestBlock=%u, freeBytes=%u\n",
               baselineLargestBlock, baselineFreeBytes);
}

void FragmentationMonitor::periodicTick() {
    // Get current heap stats
    uint32_t free, largestBlock, freeBlocks;
    getHeapStats(free, largestBlock, freeBlocks);
    
    // Check for CRITICAL thresholds - these bypass all cooldowns and dump immediately
    if (largestBlock < FRAG_CRITICAL_THRESHOLD_BYTES) {
        unsigned long timeSinceLastDump = millis() - lastDumpTime;
        if (timeSinceLastDump >= FRAG_SEVERE_COOLDOWN_MS || lastDumpTime == 0) {
            Log.printf("[FragMon] CRITICAL: largestBlock=%u < %u bytes - IMMEDIATE DUMP!\n",
                       largestBlock, FRAG_CRITICAL_THRESHOLD_BYTES);
            dumpToFile();
            lastDumpTime = millis();
            lastDumpedLargestBlock = largestBlock;
            return;  // Skip normal processing
        }
    }
    
    // Check current fragmentation state
    bool currentlyFragmented = isFragmented();
    
    // Update baseline periodically when not fragmented
    if (!currentlyFragmented && (millis() - lastBaselineUpdate) >= FRAG_BASELINE_UPDATE_INTERVAL_MS) {
        updateBaseline();
        lastBaselineUpdate = millis();
    }
    
    // Check if active logging period has ended (using start time is simpler and overflow-safe)
    if (activeLoggingMode) {
        unsigned long now = millis();
        unsigned long elapsed = now - activeLoggingStartTime;
        // Unsigned arithmetic automatically handles millis() overflow correctly
        if (elapsed >= FRAG_ACTIVE_LOGGING_DURATION_MS) {
            Log.printf("[FragMon] Active logging period ended (30 seconds elapsed)\n");
            activeLoggingMode = false;
            activeLoggingStartTime = 0;
            lastActiveLogTime = 0;
        }
    }
    
    // Log operation automatically during active logging mode (throttled to once per second)
    if (activeLoggingMode) {
        unsigned long now = millis();
        // Use unsigned arithmetic to handle millis() overflow
        if ((unsigned long)(now - lastActiveLogTime) >= 1000 || lastActiveLogTime == 0) {
            logOperation("FragMon", 0, "PeriodicCheck", true);
            lastActiveLogTime = now;
        }
    }
    
    // State change detection
    if (currentlyFragmented && !lastFragmentedState) {
        // Just became fragmented
        fragmentedSince = millis();
        fragmentedAtLargestBlock = largestBlock;
        
        // Start active logging period for 30 seconds
        activeLoggingMode = true;
        activeLoggingStartTime = millis();
        
        // Calculate degradation from baseline
        int32_t degradation = baselineLargestBlock - largestBlock;
        float degradationPercent = (baselineLargestBlock > 0) ? 
                                   (degradation * 100.0f / baselineLargestBlock) : 0.0f;
        
        Log.println("[FragMon] WARNING: Heap fragmentation detected!");
        Log.printf("[FragMon] Current: free=%u, largestBlock=%u (%.1f%%), blocks=%u\n",
                   free, largestBlock, (largestBlock * 100.0f / free), freeBlocks);
        Log.printf("[FragMon] Baseline: largestBlock=%u (degraded by %d bytes, %.1f%%)\n",
                   baselineLargestBlock, degradation, degradationPercent);
        Log.printf("[FragMon] Starting active logging for next %d seconds\n", FRAG_ACTIVE_LOGGING_DURATION_MS / 1000);
        
    } else if (!currentlyFragmented && lastFragmentedState) {
        // Fragmentation resolved
        unsigned long duration = millis() - fragmentedSince;
        Log.printf("[FragMon] Fragmentation resolved after %lu ms\n", duration);
        
        // Stop active logging if still running
        if (activeLoggingMode) {
            Log.printf("[FragMon] Stopping active logging (fragmentation resolved)\n");
            activeLoggingMode = false;
            activeLoggingStartTime = 0;
            lastActiveLogTime = 0;
        }
        
        fragmentedSince = 0;
        fragmentedAtLargestBlock = 0;
        
        // Update baseline immediately after recovery
        updateBaseline();
        lastBaselineUpdate = millis();
        
    } else if (currentlyFragmented && fragmentedSince > 0) {
        // Check if fragmentation persists AND is worsening
        unsigned long duration = millis() - fragmentedSince;
        
        if (duration >= FRAG_PERSIST_TIME_MS) {
            // Only dump if fragmentation has WORSENED since first detected
            uint32_t compareBlock = (lastDumpedLargestBlock > 0) ? lastDumpedLargestBlock : fragmentedAtLargestBlock;
            
            // Check severity of worsening
            bool hasWorsened = false;
            bool isSevereWorsening = false;
            float worseningPercent = 0.0f;
            
            if (compareBlock > 0 && largestBlock < compareBlock) {
                int32_t furtherDegradation = compareBlock - largestBlock;
                worseningPercent = (furtherDegradation * 100.0f / compareBlock);
                hasWorsened = (worseningPercent >= 5.0f);  // 5% threshold for worsening
                isSevereWorsening = (worseningPercent >= FRAG_SEVERE_DEGRADATION_PERCENT);  // >50% = severe
                
                if (hasWorsened) {
                    Log.printf("[FragMon] Fragmentation WORSENED: %u -> %u bytes (%.1f%% worse)\n",
                               compareBlock, largestBlock, worseningPercent);
                }
            }
            
            // Determine appropriate cooldown based on severity
            unsigned long cooldownRequired = isSevereWorsening ? FRAG_SEVERE_COOLDOWN_MS : FRAG_DUMP_COOLDOWN_MS;
            bool isSevereThreshold = (largestBlock < FRAG_SEVERE_THRESHOLD_BYTES);
            if (isSevereThreshold) {
                cooldownRequired = FRAG_SEVERE_COOLDOWN_MS;  // Use shorter cooldown for severe threshold
            }
            
            unsigned long timeSinceLastDump = millis() - lastDumpTime;
            bool cooldownExpired = (timeSinceLastDump >= cooldownRequired || lastDumpTime == 0);
            
            // Dump if worsening AND (cooldown expired OR severe case bypassing cooldown)
            if (hasWorsened && (cooldownExpired || (isSevereWorsening && timeSinceLastDump >= FRAG_SEVERE_COOLDOWN_MS))) {
                const char* severity = isSevereWorsening ? "SEVERE" : (isSevereThreshold ? "SEVERE" : "NORMAL");
                Log.printf("[FragMon] ALERT [%s]: Fragmentation worsened %.1f%% and persisted for %lu ms - dumping log!\n",
                           severity, worseningPercent, duration);
                dumpToFile();
                lastDumpTime = millis();
                lastDumpedLargestBlock = largestBlock;
            } else if (hasWorsened && !cooldownExpired) {
                Log.printf("[FragMon] Fragmentation worsening but in cooldown period (%lu ms remaining)\n",
                           FRAG_DUMP_COOLDOWN_MS - timeSinceLastDump);
            }
            // If not worsening, don't log anything (prevents spam when fragmentation is stable)
        }
    }
    
    lastFragmentedState = currentlyFragmented;
}

void FragmentationMonitor::logOperation(const char* file, int line, const char* operation, bool forceLog) {
    if (!operationBuffer || !bufferMutex) return;
    
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Get current heap stats
        uint32_t free, largestBlock, freeBlocks;
        getHeapStats(free, largestBlock, freeBlocks);
        
        // Write to circular buffer (always log to buffer for history)
        MemoryOperation& op = operationBuffer[bufferIndex];
        op.timestamp = millis();
        
        // Extract short filename (avoid heap allocation!)
        const char* shortFile = getShortFilename(file);
        strncpy(op.module, shortFile, sizeof(op.module) - 1);
        op.module[sizeof(op.module) - 1] = '\0';
        
        // Copy operation name (avoid heap allocation!)
        strncpy(op.operation, operation, sizeof(op.operation) - 1);
        op.operation[sizeof(op.operation) - 1] = '\0';
        
        op.line = line;  // Store line number
        op.heapFree = free;
        op.largestBlock = largestBlock;
        
        // Advance circular buffer index
        bufferIndex = (bufferIndex + 1) % FRAG_MONITOR_BUFFER_SIZE;
        operationCount++;
        
        xSemaphoreGive(bufferMutex);
    }
}

bool FragmentationMonitor::isFragmented() {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    
    // Not enough free memory to care about fragmentation
    if (info.total_free_bytes < FRAG_MIN_FREE_BYTES) {
        return false;
    }
    
    // If we don't have a baseline yet, we're not fragmented
    if (baselineLargestBlock == 0) {
        return false;
    }
    
    // NEW FRAGMENTATION DETECTION: Check if largest_free_block has degraded significantly
    // from baseline (indicates NEW fragmentation occurring over time)
    int32_t degradation = baselineLargestBlock - info.largest_free_block;
    float degradationPercent = (baselineLargestBlock > 0) ? 
                               (degradation * 100.0f / baselineLargestBlock) : 0.0f;
    
    // Alert if largest block has degraded by FRAG_DEGRADATION_THRESHOLD_PERCENT or more
    return (degradationPercent >= FRAG_DEGRADATION_THRESHOLD_PERCENT);
}

void FragmentationMonitor::getHeapStats(uint32_t& free, uint32_t& largestBlock, uint32_t& freeBlocks) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    
    free = info.total_free_bytes;
    largestBlock = info.largest_free_block;
    freeBlocks = info.free_blocks;
}

void FragmentationMonitor::updateBaseline() {
    uint32_t free, largestBlock, freeBlocks;
    getHeapStats(free, largestBlock, freeBlocks);
    
    // Only update baseline if current state is better than baseline
    // This prevents baseline from degrading over time
    if (largestBlock > baselineLargestBlock) {
        Log.printf("[FragMon] Updating baseline: %u -> %u bytes (improvement: %d)\n",
                   baselineLargestBlock, largestBlock, (int32_t)(largestBlock - baselineLargestBlock));
        baselineLargestBlock = largestBlock;
        baselineFreeBytes = free;
        baselineUpdateTime = millis();
    }
}

void FragmentationMonitor::dumpToFile() {
    if (!operationBuffer || !bufferMutex) return;
    
    // Check filesystem space before writing
    if (!hasEnoughFsSpace()) {
        Log.println("[FragMon] WARNING: Insufficient filesystem space - attempting cleanup");
        cleanupOldLogs(FRAG_MIN_FS_FREE_BYTES + 20480); // Try to free up to 70KB
        
        // Check again after cleanup
        if (!hasEnoughFsSpace()) {
            Log.println("[FragMon] ERROR: Still insufficient space after cleanup - skipping log dump");
            return;
        }
    }
    
    // Get current time in local timezone using GeneralTimeConverter
    time_t now;
    time(&now);
    
    // Convert to local time if timeConverter is available
    time_t localTime = now;
    if (timeConverter && timeConverter->isSuccessfullyParsed()) {
        localTime = timeConverter->toLocal(now);
    }
    
    struct tm timeinfo;
    localtime_r(&localTime, &timeinfo);
    
    // Create filename with date/time in format: frag_yymmddhhmm.log (use stack allocation!)
    char filename[64];
    snprintf(filename, sizeof(filename), "/mem_debug/frag_%02d%02d%02d%02d%02d.log",
             (timeinfo.tm_year + 1900) % 100,  // yy
             timeinfo.tm_mon + 1,              // mm
             timeinfo.tm_mday,                 // dd
             timeinfo.tm_hour,                 // hh
             timeinfo.tm_min);                 // mm
    
    Log.printf("[FragMon] Dumping fragmentation log to %s\n", filename);
    
    // Open file for writing
    File logFile = LittleFS.open(filename, "w");
    if (!logFile) {
        Log.printf("[FragMon] ERROR: Failed to open %s for writing\n", filename);
        return;
    }
    
    // Write header with current heap state
    uint32_t free, largestBlock, freeBlocks;
    getHeapStats(free, largestBlock, freeBlocks);
    
    // Use stack-allocated buffer for writing (avoid heap!)
    char lineBuf[256];
    
    snprintf(lineBuf, sizeof(lineBuf), "=== Heap Fragmentation Log ===\n");
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    // Add date/time to header (already in local time)
    snprintf(lineBuf, sizeof(lineBuf), "Date/Time: %04d-%02d-%02d %02d:%02d:%02d\n",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    snprintf(lineBuf, sizeof(lineBuf), "Timestamp: %lu ms\n", millis());
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    snprintf(lineBuf, sizeof(lineBuf), "Current Heap: free=%u, largestBlock=%u (%.1f%%), freeBlocks=%u\n",
             free, largestBlock, (largestBlock * 100.0f / free), freeBlocks);
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    // Add baseline information
    int32_t degradation = baselineLargestBlock - largestBlock;
    float degradationPercent = (baselineLargestBlock > 0) ? 
                               (degradation * 100.0f / baselineLargestBlock) : 0.0f;
    
    snprintf(lineBuf, sizeof(lineBuf), "Baseline: largestBlock=%u (set %lu ms ago)\n",
             baselineLargestBlock, millis() - baselineUpdateTime);
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    snprintf(lineBuf, sizeof(lineBuf), "Degradation: %d bytes (%.1f%% loss from baseline)\n",
             degradation, degradationPercent);
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    // Add active logging status
    if (activeLoggingMode) {
        unsigned long now = millis();
        unsigned long elapsed = now - activeLoggingStartTime;
        unsigned long remaining = (elapsed < FRAG_ACTIVE_LOGGING_DURATION_MS) ? 
                                  (FRAG_ACTIVE_LOGGING_DURATION_MS - elapsed) : 0;
        snprintf(lineBuf, sizeof(lineBuf), "Active Logging: ENABLED (remaining: %lu ms)\n", remaining);
        logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Active Logging: DISABLED\n");
        logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    }
    
    snprintf(lineBuf, sizeof(lineBuf), "\n=== Recent Operations (last %d) ===\n", 
             min(operationCount, FRAG_MONITOR_BUFFER_SIZE));
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    // Dump operations from buffer (acquire mutex)
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int opsToWrite = min(operationCount, FRAG_MONITOR_BUFFER_SIZE);
        int startIdx = (bufferIndex - opsToWrite + FRAG_MONITOR_BUFFER_SIZE) % FRAG_MONITOR_BUFFER_SIZE;
        
        for (int i = 0; i < opsToWrite; i++) {
            int idx = (startIdx + i) % FRAG_MONITOR_BUFFER_SIZE;
            const MemoryOperation& op = operationBuffer[idx];
            
            snprintf(lineBuf, sizeof(lineBuf), 
                     "[%8lu] %-15s:%-4d | %-30s | heap=%6u, largest=%6u (%.1f%%)\n",
                     op.timestamp, op.module, op.line, op.operation,
                     op.heapFree, op.largestBlock,
                     (op.heapFree > 0) ? (op.largestBlock * 100.0f / op.heapFree) : 0.0f);
            logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
        }
        
        xSemaphoreGive(bufferMutex);
    }
    
    logFile.close();
    Log.printf("[FragMon] Log written successfully (%d operations)\n", 
               min(operationCount, FRAG_MONITOR_BUFFER_SIZE));
}

bool FragmentationMonitor::hasEnoughFsSpace() {
    // Get filesystem info
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    
    Log.printf("[FragMon] FS Space: total=%u, used=%u, free=%u\n", 
               totalBytes, usedBytes, freeBytes);
    
    return (freeBytes >= FRAG_MIN_FS_FREE_BYTES);
}

void FragmentationMonitor::cleanupOldLogs(size_t targetFreeSpace) {
    Log.println("[FragMon] Starting cleanup of old log files...");
    
    // Open mem_debug directory
    File dir = LittleFS.open("/mem_debug");
    if (!dir || !dir.isDirectory()) {
        Log.println("[FragMon] Cannot open /mem_debug directory");
        return;
    }
    
    // Collect all log files with their timestamps
    struct LogFileInfo {
        char name[64];
        time_t timestamp;
        size_t size;
    };
    
    // Use PSRAM for the array to avoid heap fragmentation
    LogFileInfo* logFiles = (LogFileInfo*)ps_malloc(sizeof(LogFileInfo) * 50);
    if (!logFiles) {
        Log.println("[FragMon] ERROR: Failed to allocate PSRAM for log file list");
        dir.close();
        return;
    }
    
    int fileCount = 0;
    File file = dir.openNextFile();
    while (file && fileCount < 50) {
        if (!file.isDirectory()) {
            const char* fname = file.name();
            // Check if it's a fragmentation log file (starts with "frag_")
            if (strncmp(fname, "frag_", 5) == 0) {
                strncpy(logFiles[fileCount].name, fname, sizeof(logFiles[fileCount].name) - 1);
                logFiles[fileCount].name[sizeof(logFiles[fileCount].name) - 1] = '\0';
                
                // Extract timestamp from filename (frag_TIMESTAMP.log)
                logFiles[fileCount].timestamp = atol(fname + 5);
                logFiles[fileCount].size = file.size();
                fileCount++;
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();
    
    Log.printf("[FragMon] Found %d log files\n", fileCount);
    
    if (fileCount == 0) {
        free(logFiles);
        return;
    }
    
    // Sort files by timestamp (oldest first) - simple bubble sort
    for (int i = 0; i < fileCount - 1; i++) {
        for (int j = 0; j < fileCount - i - 1; j++) {
            if (logFiles[j].timestamp > logFiles[j + 1].timestamp) {
                LogFileInfo temp = logFiles[j];
                logFiles[j] = logFiles[j + 1];
                logFiles[j + 1] = temp;
            }
        }
    }
    
    // Delete oldest files until we have enough space or reach max file limit
    size_t currentFree = LittleFS.totalBytes() - LittleFS.usedBytes();
    int filesDeleted = 0;
    
    for (int i = 0; i < fileCount; i++) {
        // Keep at least the newest FRAG_MAX_LOG_FILES files
        if ((fileCount - filesDeleted) <= FRAG_MAX_LOG_FILES && currentFree >= targetFreeSpace) {
            break;
        }
        
        char fullPath[80];
        snprintf(fullPath, sizeof(fullPath), "/mem_debug/%s", logFiles[i].name);
        
        Log.printf("[FragMon] Deleting old log: %s (size: %u bytes)\n", 
                   logFiles[i].name, logFiles[i].size);
        
        if (LittleFS.remove(fullPath)) {
            currentFree += logFiles[i].size;
            filesDeleted++;
        } else {
            Log.printf("[FragMon] WARNING: Failed to delete %s\n", fullPath);
        }
        
        // Stop if we've freed enough space
        if (currentFree >= targetFreeSpace) {
            break;
        }
    }
    
    free(logFiles);
    
    Log.printf("[FragMon] Cleanup complete: deleted %d files, free space now: %u bytes\n", 
               filesDeleted, currentFree);
}

const char* FragmentationMonitor::getShortFilename(const char* file) {
    if (!file) return "unknown";
    
    // Find last path separator (works for both / and \)
    const char* lastSlash = strrchr(file, '/');
    if (!lastSlash) {
        lastSlash = strrchr(file, '\\');
    }
    
    return lastSlash ? (lastSlash + 1) : file;
}

#endif // ENABLE_FRAG_MONITOR
