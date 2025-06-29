#include "ring_buffer.hpp"
#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>

RingBuffer<uint16_t, 1024> bus;
std::atomic<bool> done{false};

void producer() {
    for (uint16_t i = 1; i <= 1000000; ++i) {
        while (!bus.push(i)); // spin until push succeeds
    }
    done = true;
}

void consumer() {
    uint16_t x;
    uint64_t total = 0;
    auto t0 = std::chrono::high_resolution_clock::now();

    while (!done || bus.pop(x)) {
        if (bus.pop(x)) {
            total += x;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "Sum = " << total << ", Time = " << elapsed << " us\n";
}

int main() {
    std::thread prod(producer);
    std::thread cons(consumer);
    prod.join();
    cons.join();
}
