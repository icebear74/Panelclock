#ifndef PSRAMUTILS_HPP
#define PSRAMUTILS_HPP

#include <Arduino.h> // Für ps_malloc
#include <string>    // Für std::basic_string
#include <new>       // Für std::bad_alloc

// PSRAM Allocator für std Container
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

// Definiert einen std::string, der den PSRAM Allocator verwendet
using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

#endif // PSRAMUTILS_HPP