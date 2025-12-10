#pragma once
#include <atomic>
#include <cstdint>
#include <cassert>
#include <new> // Required for strict alignment if using std::hardware_... (optional)

template<typename T, size_t Size>
class RingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");

    // The Layout Struct
    // We group them to enforce alignment relative to each other.
    struct Indices {
        alignas(64) std::atomic<size_t> head{0}; // Producer writes this
        alignas(64) std::atomic<size_t> tail{0}; // Consumer writes this
    };

    T buffer[Size];
    Indices indices; // One instance of the struct

public:
    bool push(const T& item) {
        // CHANGE 1: Access via 'indices.head' and 'indices.tail'
        size_t h = indices.head.load(std::memory_order_relaxed);
        size_t t = indices.tail.load(std::memory_order_acquire);

        if (((h + 1) & (Size - 1)) == t) return false; // full

        buffer[h] = item;
        
        // CHANGE 2: Store to 'indices.head'
        indices.head.store((h + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        // CHANGE 3: Access via 'indices.tail' and 'indices.head'
        size_t t = indices.tail.load(std::memory_order_relaxed);
        size_t h = indices.head.load(std::memory_order_acquire);

        if (t == h) return false; // empty

        item = buffer[t];
        
        // CHANGE 4: Store to 'indices.tail'
        indices.tail.store((t + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
};