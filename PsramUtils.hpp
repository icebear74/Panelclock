#ifndef PSRAMUTILS_HPP
#define PSRAMUTILS_HPP

#include <Arduino.h>
#include <cstdlib>
#include <string>
#include <new>
#include <esp_heap_caps.h>

// NOTE: Do NOT define psramFound() here because esp32-hal-psram.h already declares it.
// Use the SDK's psramFound() (declared in esp32-hal-psram.h) directly.

static inline size_t psramFree() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

// PSRAM Allocator for std containers
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

// Psram-backed std::string
using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

#endif // PSRAMUTILS_HPP