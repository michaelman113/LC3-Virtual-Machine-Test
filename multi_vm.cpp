#include <thread>

// Declare the run_vm function from lc3-alt-win.cpp
extern void run_vm(const char*);

int main() {
    std::thread vm1(run_vm, "producer.obj");
    std::thread vm2(run_vm, "consumer.obj");

    vm1.join();
    vm2.join();
}
