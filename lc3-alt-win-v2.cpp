// lc3-alt-win-v3.cpp — refactored for thread-safe dual-VM ring buffer communication

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <chrono>
#include <Windows.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include <mutex>
#include "ring_buffer.hpp"

RingBuffer<uint16_t, 1024> ring_bus; 
HANDLE hStdin;
DWORD fdwOldMode;

void disable_input_buffering() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode);
    DWORD mode = fdwOldMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, mode);
    FlushConsoleInputBuffer(hStdin);
}

void restore_input_buffering() {
    SetConsoleMode(hStdin, fdwOldMode);
}
class LC3VM {
public:
    uint16_t memory[1 << 16]{};
    uint16_t reg[10]{};  // R0–R7, PC, COND
    bool running = true;
    uint64_t instr_count = 0;
    uint64_t msg_send = 0;
    uint64_t msg_recv = 0;
    uint64_t recv_spin_total = 0;
    const char* image_name;

    void load_image(const char* path) {
        image_name = path;
        FILE* file = fopen(path, "rb");
        if (!file) {
            printf("failed to load image: %s\n", path);
            exit(1);
        }
        uint16_t origin;
        fread(&origin, sizeof(origin), 1, file);
        origin = swap16(origin);
        size_t max_read = (1 << 16) - origin;
        fread(memory + origin, sizeof(uint16_t), max_read, file);
        for (size_t i = 0; i < max_read; ++i) memory[origin + i] = swap16(memory[origin + i]);
        fclose(file);
        reg[8] = origin;  // R_PC
    }

    void run() {
        disable_input_buffering();
        signal(SIGINT, [](int) { restore_input_buffering(); exit(-2); });

        auto start = std::chrono::high_resolution_clock::now();

        while (running && instr_count < 50000) {
            uint16_t instr = mem_read(reg[8]++);  // PC
            uint16_t op = instr >> 12;
            
            // Debug output for TRAP x30
            if (op == 0xF && (instr & 0xFF) == 0x30) {
                printf("[PROD R0=%d] PC=0x%04X\n", reg[0], reg[8]-1);
            }
            
            execute(op, instr);
            ++instr_count;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        uint64_t elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double ns_per_instr = static_cast<double>(elapsed_ns) / instr_count;
        double mips = 1e3 * 1e6 / ns_per_instr; // or: 1e9 / ns_per_instr


        printf("\n==== VM (%s) Metrics ====" "\n", image_name);
        printf("Instructions: %llu\n", instr_count);
        printf("ns/op              : %.2f ns\n", ns_per_instr);
        printf("Throughput         : %.2f MIPS\n", mips);
        printf("Messages Sent: %llu, Recv: %llu\n", msg_send, msg_recv);
        printf("Recv spin iters: %llu\n", recv_spin_total);
        printf("Msgs/sec: %.2f\n", 1000.0 * msg_recv / elapsed_ms);
        double avg_spins_per_msg = msg_recv ? static_cast<double>(recv_spin_total) / msg_recv : 0;
        double us_per_msg = msg_recv ? (elapsed_ms * 1000.0) / msg_recv : 0;

        printf("Avg spins/msg      : %.2f\n", avg_spins_per_msg);
        printf("Avg us/msg         : %.2f\n", us_per_msg);
        printf("Elapsed time       : %.2f ms\n", elapsed_ms);

        printf("=========================\n");

        restore_input_buffering();
    }

private:
    uint16_t mem_read(uint16_t addr) {
        if (addr == 0xFE00) memory[0xFE00] = (_kbhit() ? (1 << 15) : 0);
        return memory[addr];
    }

    void mem_write(uint16_t addr, uint16_t val) {
        memory[addr] = val;
    }

    void update_flags(uint16_t r) {
        if (reg[r] == 0) reg[9] = 0x2;
        else if (reg[r] >> 15) reg[9] = 0x4;
        else reg[9] = 0x1;
    }

    void execute(uint16_t op, uint16_t instr) {
        switch (op) {
            case 0x0: { // BR
                uint16_t cond = (instr >> 9) & 0x7;
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                bool should_branch = false;
                
                if (cond == 0) should_branch = true;  // BR
                else if (cond == 1) should_branch = (reg[9] & 0x1);  // BRP
                else if (cond == 2) should_branch = (reg[9] & 0x2);  // BRZ
                else if (cond == 3) should_branch = (reg[9] & 0x1) || (reg[9] & 0x2);  // BRZP
                else if (cond == 4) should_branch = (reg[9] & 0x4);  // BRN
                else if (cond == 5) should_branch = (reg[9] & 0x4) || (reg[9] & 0x1);  // BRNP
                else if (cond == 6) should_branch = (reg[9] & 0x4) || (reg[9] & 0x2);  // BRNZ
                else if (cond == 7) should_branch = true;  // BRNZP
                
                if (should_branch) {
                    reg[8] += offset;
                }
                break;
            }
            case 0x1: { // ADD
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t sr1 = (instr >> 6) & 0x7;
                if ((instr >> 5) & 1) reg[dr] = reg[sr1] + sign_extend(instr & 0x1F, 5);
                else reg[dr] = reg[sr1] + reg[instr & 0x7];
                update_flags(dr);
                break;
            }
            case 0x2: { // LD
                uint16_t dr = (instr >> 9) & 0x7;
                reg[dr] = mem_read(reg[8] + sign_extend(instr & 0x1FF, 9));
                update_flags(dr);
                break;
            }
            case 0x3: { // ST
                uint16_t sr = (instr >> 9) & 0x7;
                mem_write(reg[8] + sign_extend(instr & 0x1FF, 9), reg[sr]);
                break;
            }
            case 0x4: { // JSR
                reg[7] = reg[8];
                reg[8] += sign_extend(instr & 0x7FF, 11);
                break;
            }
            case 0x5: { // AND
                uint16_t dr = (instr >> 9) & 0x7;
                uint16_t sr1 = (instr >> 6) & 0x7;
                if ((instr >> 5) & 1) reg[dr] = reg[sr1] & sign_extend(instr & 0x1F, 5);
                else reg[dr] = reg[sr1] & reg[instr & 0x7];
                update_flags(dr);
                break;
            }
            case 0x9: { // NOT
                uint16_t dr = (instr >> 9) & 0x7;
                reg[dr] = ~reg[(instr >> 6) & 0x7];
                update_flags(dr);
                break;
            }
            case 0xC: { // JMP
                reg[8] = reg[(instr >> 6) & 0x7];
                break;
            }
            case 0xF: { // TRAP
                uint8_t trap = instr & 0xFF;
                switch (trap) {
                    case 0x21: putchar(reg[0]); fflush(stdout); break;
                    case 0x22: putchar('\n'); fflush(stdout); break;
                    case 0x25: running = false; break;
                    case 0x30:  // SEND
                        //printf("[producer] SEND: %d\n", reg[0]);  // 🔍 DEBUG LINE
                        while (!ring_bus.push(reg[0])) {};
                        ++msg_send;
                        break;
                    case 0x31: { // RECV
                        uint16_t val;
                        while (!ring_bus.pop(val)) ++recv_spin_total;
                        reg[0] = val;
                        //printf("[consumer] RECV: %d\n", val);  // 🔍 DEBUG LINE
                        ++msg_recv;
                        update_flags(0);
                        break;
                    }
                }
                break;
            }
        }
    }

    static uint16_t swap16(uint16_t x) { return (x << 8) | (x >> 8); }
    static uint16_t sign_extend(uint16_t x, int bit_count) {
        if ((x >> (bit_count - 1)) & 1) x |= (0xFFFF << bit_count);
        return x;
    }
};

// Windows-specific input buffering controls


// Main multi-threaded test harness
int main() {
    std::thread vm1([] {
        LC3VM vm;
        vm.load_image("producer.obj");
        vm.run();
    });

    std::thread vm2([] {
        LC3VM vm;
        vm.load_image("consumer.obj");
        vm.run();
    });

    vm1.join();
    vm2.join();
}
