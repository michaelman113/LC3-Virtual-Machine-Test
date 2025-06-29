#pragma once
#include <atomic>
#include <cstdint>
#include <cassert>

template<typename T, size_t Size>
class RingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");

    T buffer[Size];
    std::atomic<size_t> head{0}; // producer index
    std::atomic<size_t> tail{0}; // consumer index

public:
    bool push(const T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);

        if (((h + 1) & (Size - 1)) == t) return false; // full

        buffer[h] = item;
        head.store((h + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);

        if (t == h) return false; // empty

        item = buffer[t];
        tail.store((t + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
};
