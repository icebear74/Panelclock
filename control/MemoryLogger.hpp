#ifndef MEMORYLOGGER_HPP
#define MEMORYLOGGER_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Deklaration des globalen Mutex, der in Panelclock.ino definiert wird
extern SemaphoreHandle_t serialMutex;

// ====================================================================================
// =================== ZENTRALE STEUERUNG DES LOG-LEVELS ===============================
// 0: Logging komplett deaktiviert. Kein Code wird kompiliert.
// 1: Nur strategische Punkte (Anfang/Ende wichtiger Funktionen).
// 2: Volle Details (jeder einzelne Schritt zum Debuggen).
#define LOG_LEVEL 2
// ====================================================================================

#if LOG_LEVEL > 0

#include <Arduino.h>
#include <esp_heap_caps.h>

// Extrahiert nur den Dateinamen aus dem kompletten Pfad
static inline const char* _get_short_filename(const char* file) {
    const char* short_file = strrchr(file, '\\'); // Für Windows-Pfade
    if (!short_file) {
        short_file = strrchr(file, '/'); // Für Linux/Mac-Pfade
    }
    return (short_file) ? short_file + 1 : file;
}

/**
 * @brief Interne Log-Funktion. Nutze die Makros stattdessen.
 */
static inline void _logMemory(const char* tag, const char* file, int line, const char* func) {
    // MINIMALE ÄNDERUNG: Warte, bis der serielle Port frei ist.
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        multi_heap_info_t internal_info;
        heap_caps_get_info(&internal_info, MALLOC_CAP_INTERNAL);

        multi_heap_info_t psram_info;
        heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM);
        
        Serial.printf(
            "[MEM] %-25s @ %s:%d (%s)\n      | HEAP:  Free %7u, MaxAlloc %7u, FreeBlocks %4u, UsedBlocks %4u\n      | PSRAM: Free %7u, MaxAlloc %7u, FreeBlocks %4u, UsedBlocks %4u\n",
            tag, _get_short_filename(file), line, func,
            (unsigned)internal_info.total_free_bytes,
            (unsigned)internal_info.largest_free_block,
            (unsigned)internal_info.free_blocks,
            (unsigned)internal_info.allocated_blocks,
            (unsigned)psram_info.total_free_bytes,
            (unsigned)psram_info.largest_free_block,
            (unsigned)psram_info.free_blocks,
            (unsigned)psram_info.allocated_blocks
        );

        // MINIMALE ÄNDERUNG: Gib den seriellen Port sofort wieder frei.
        xSemaphoreGive(serialMutex);
    }
}

#else
// Wenn LOG_LEVEL 0 ist, werden die Makros zu nichts.
#endif


// --- Makro-Definitionen basierend auf dem Log-Level ---

#if LOG_LEVEL >= 1
    #define LOG_MEMORY_STRATEGIC(tag) _logMemory(tag, __FILE__, __LINE__, __func__)
#else
    #define LOG_MEMORY_STRATEGIC(tag)
#endif

#if LOG_LEVEL >= 2
    #define LOG_MEMORY_DETAILED(tag) _logMemory(tag, __FILE__, __LINE__, __func__)
#else
    #define LOG_MEMORY_DETAILED(tag)
#endif

#endif // MEMORYLOGGER_HPP