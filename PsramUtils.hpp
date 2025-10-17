#ifndef PSRAMUTILS_HPP
#define PSRAMUTILS_HPP

#define ENABLE_MEMORY_LOGGING 1

#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <new>
#include <esp_heap_caps.h>

#if __has_include(<WString.h>)
#include <WString.h>
#endif

// Vorwärtsdeklaration für MemoryLogger
void logMemoryUsage(const char* tag);

static inline size_t psramFree() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

static inline char* psram_strdup(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str) + 1;
    char* new_str = (char*)ps_malloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

template <typename T>
struct PsramAllocator {
    using value_type = T;
    PsramAllocator() = default;
    template <typename U>
    constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}
    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
        if (auto p = static_cast<T*>(ps_malloc(n * sizeof(T)))) return p;
        throw std::bad_alloc();
    }
    void deallocate(T* p, std::size_t) noexcept { std::free(p); }
};
template <typename T, typename U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }
template <typename T, typename U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }

using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

template <typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;

// --- EINZIGE KORREKTE DEFINITION VON indexOf ---
static inline int indexOf(const PsramString& str, const char* substring, size_t fromIndex = 0) {
    const char* found = strstr(str.c_str() + fromIndex, substring);
    if (found == nullptr) return -1;
    return found - str.c_str();
}

static inline int indexOf(const PsramString& str, const String& substring, size_t fromIndex = 0) {
    return indexOf(str, substring.c_str(), fromIndex);
}

// --- Operatoren für Flash-Strings ---
#if __has_include(<WString.h>)
inline PsramString& operator+=(PsramString& s, const __FlashStringHelper* f) {
  s += String(f).c_str();
  return s;
}

inline PsramString operator+(const PsramString& s, const __FlashStringHelper* f) {
    return s + String(f).c_str();
}

inline PsramString operator+(const __FlashStringHelper* f, const PsramString& s) {
    return String(f).c_str() + s;
}
#endif

#endif // PSRAMUTILS_HPP