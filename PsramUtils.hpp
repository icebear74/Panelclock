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

// Deklarationen für Funktionen, die in PsramUtils.cpp implementiert werden
size_t psramFree();
char* psram_strdup(const char* str);

// Template-Implementierungen müssen im Header bleiben
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

// Deklarationen für Funktionen, die in PsramUtils.cpp implementiert werden
PsramString readFromStream(Stream& stream);
int indexOf(const PsramString& str, const char* substring, size_t fromIndex = 0);
int indexOf(const PsramString& str, const String& substring, size_t fromIndex = 0);
void replaceAll(PsramString& str, const PsramString& from, const PsramString& to);
PsramString escapeJsonString(const PsramString& input);

#if __has_include(<WString.h>)
// Operatoren, die auf PsramString operieren, müssen auch deklariert werden
PsramString& operator+=(PsramString& s, const __FlashStringHelper* f);
PsramString operator+(const PsramString& s, const __FlashStringHelper* f);
PsramString operator+(const __FlashStringHelper* f, const PsramString& s);
#endif

#endif // PSRAMUTILS_HPP