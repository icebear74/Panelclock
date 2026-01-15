#include "FragmentationMonitor.hpp"
#include "MultiLogger.hpp"

#if ENABLE_FRAG_MONITOR

// Static members - ALL ALLOCATED IN PSRAM to avoid heap fragmentation!
MemoryOperation* FragmentationMonitor::operationBuffer = nullptr;
int FragmentationMonitor::bufferIndex = 0;
int FragmentationMonitor::operationCount = 0;
SemaphoreHandle_t FragmentationMonitor::bufferMutex = nullptr;

// Global instance
FragmentationMonitor* g_FragMonitor = nullptr;

FragmentationMonitor::FragmentationMonitor() 
    : fragmentedSince(0), lastFragmentedState(false) {
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
    Log.println("[FragMon] Initializing FragmentationMonitor...");
    
    // Allocate operation buffer in PSRAM (NOT heap!)
    if (!operationBuffer) {
        operationBuffer = (MemoryOperation*)ps_malloc(sizeof(MemoryOperation) * FRAG_MONITOR_BUFFER_SIZE);
        if (!operationBuffer) {
            Log.println("[FragMon] ERROR: Failed to allocate PSRAM buffer!");
            return;
        }
        memset(operationBuffer, 0, sizeof(MemoryOperation) * FRAG_MONITOR_BUFFER_SIZE);
        Log.printf("[FragMon] Allocated %d bytes in PSRAM for operation buffer\n", 
                   sizeof(MemoryOperation) * FRAG_MONITOR_BUFFER_SIZE);
    }
    
    // Create mutex for buffer protection
    if (!bufferMutex) {
        bufferMutex = xSemaphoreCreateMutex();
        if (!bufferMutex) {
            Log.println("[FragMon] ERROR: Failed to create mutex!");
            return;
        }
    }
    
    // Create mem_debug directory if it doesn't exist
    if (!LittleFS.exists("/mem_debug")) {
        if (LittleFS.mkdir("/mem_debug")) {
            Log.println("[FragMon] Created /mem_debug directory");
        } else {
            Log.println("[FragMon] ERROR: Failed to create /mem_debug directory");
        }
    }
    
    // Log startup
    uint32_t free, largestBlock, freeBlocks;
    getHeapStats(free, largestBlock, freeBlocks);
    Log.printf("[FragMon] Startup heap: free=%u, largestBlock=%u, blocks=%u\n", 
               free, largestBlock, freeBlocks);
    
    Log.println("[FragMon] Initialization complete");
}

void FragmentationMonitor::periodicTick() {
    // Check current fragmentation state
    bool currentlyFragmented = isFragmented();
    
    // State change detection
    if (currentlyFragmented && !lastFragmentedState) {
        // Just became fragmented
        fragmentedSince = millis();
        Log.println("[FragMon] WARNING: Heap fragmentation detected!");
        
        uint32_t free, largestBlock, freeBlocks;
        getHeapStats(free, largestBlock, freeBlocks);
        Log.printf("[FragMon] Heap: free=%u, largestBlock=%u (%.1f%%), blocks=%u\n",
                   free, largestBlock, (largestBlock * 100.0f / free), freeBlocks);
        
    } else if (!currentlyFragmented && lastFragmentedState) {
        // Fragmentation resolved
        unsigned long duration = millis() - fragmentedSince;
        Log.printf("[FragMon] Fragmentation resolved after %lu ms\n", duration);
        fragmentedSince = 0;
        
    } else if (currentlyFragmented && fragmentedSince > 0) {
        // Check if fragmentation persists
        unsigned long duration = millis() - fragmentedSince;
        
        if (duration >= FRAG_PERSIST_TIME_MS) {
            // Persistent fragmentation - dump to file
            Log.printf("[FragMon] ALERT: Fragmentation persisted for %lu ms - dumping log!\n", duration);
            dumpToFile();
            
            // Reset timer to avoid repeated dumps
            fragmentedSince = millis();
        }
    }
    
    lastFragmentedState = currentlyFragmented;
}

void FragmentationMonitor::logOperation(const char* file, const char* operation) {
    if (!operationBuffer || !bufferMutex) return;
    
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Get current heap stats
        uint32_t free, largestBlock, freeBlocks;
        getHeapStats(free, largestBlock, freeBlocks);
        
        // Write to circular buffer
        MemoryOperation& op = operationBuffer[bufferIndex];
        op.timestamp = millis();
        
        // Extract short filename (avoid heap allocation!)
        const char* shortFile = getShortFilename(file);
        strncpy(op.module, shortFile, sizeof(op.module) - 1);
        op.module[sizeof(op.module) - 1] = '\0';
        
        // Copy operation name (avoid heap allocation!)
        strncpy(op.operation, operation, sizeof(op.operation) - 1);
        op.operation[sizeof(op.operation) - 1] = '\0';
        
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
    
    // Fragmented if largest block < FRAG_THRESHOLD_PERCENT% of total free
    // AND we have enough free memory to care about fragmentation
    if (info.total_free_bytes < FRAG_MIN_FREE_BYTES) {
        return false;  // Too little memory to worry about fragmentation
    }
    
    return (info.largest_free_block < (info.total_free_bytes * FRAG_THRESHOLD_PERCENT / 100));
}

void FragmentationMonitor::getHeapStats(uint32_t& free, uint32_t& largestBlock, uint32_t& freeBlocks) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    
    free = info.total_free_bytes;
    largestBlock = info.largest_free_block;
    freeBlocks = info.free_blocks;
}

void FragmentationMonitor::dumpToFile() {
    if (!operationBuffer || !bufferMutex) return;
    
    // Create filename with timestamp (use stack allocation!)
    char filename[64];
    snprintf(filename, sizeof(filename), "/mem_debug/frag_%lu.log", millis() / 1000);
    
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
    
    snprintf(lineBuf, sizeof(lineBuf), "Timestamp: %lu ms\n", millis());
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
    snprintf(lineBuf, sizeof(lineBuf), "Current Heap: free=%u, largestBlock=%u (%.1f%%), freeBlocks=%u\n",
             free, largestBlock, (largestBlock * 100.0f / free), freeBlocks);
    logFile.write((const uint8_t*)lineBuf, strlen(lineBuf));
    
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
                     "[%8lu] %-15s | %-30s | heap=%6u, largest=%6u (%.1f%%)\n",
                     op.timestamp, op.module, op.operation,
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
