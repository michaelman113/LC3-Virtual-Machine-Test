#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <chrono>
/* windows only */
#include <Windows.h>
#include <conio.h>  // _kbhit

enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_COND,
    R_COUNT
};
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};
enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];  /* 65536 locations */
uint16_t reg[R_COUNT];

HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}
void read_image_file(FILE* file)
{
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}
void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

int running = 1;
template <unsigned op>
void ins(uint16_t instr)
{
    uint16_t r0, r1, r2, imm5, imm_flag;
    uint16_t pc_plus_off, base_plus_off;

    constexpr uint16_t opbit = (1 << op);
    if (0x4EEE & opbit) { r0 = (instr >> 9) & 0x7; }
    if (0x12F3 & opbit) { r1 = (instr >> 6) & 0x7; }
    if (0x0022 & opbit)
    {
        imm_flag = (instr >> 5) & 0x1;

        if (imm_flag)
        {
            imm5 = sign_extend(instr & 0x1F, 5);
        }
        else
        {
            r2 = instr & 0x7;
        }
    }
    if (0x00C0 & opbit)
    {   // Base + offset
        base_plus_off = reg[r1] + sign_extend(instr & 0x3F, 6);
    }
    if (0x4C0D & opbit)
    {
        // Indirect address
        pc_plus_off = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
    }
    if (0x0001 & opbit)
    {
        // BR
        uint16_t cond = (instr >> 9) & 0x7;
        if (cond & reg[R_COND]) { reg[R_PC] = pc_plus_off; }
    }
    if (0x0002 & opbit)  // ADD
    {
        if (imm_flag)
        {
            reg[r0] = reg[r1] + imm5;
        }
        else
        {
            reg[r0] = reg[r1] + reg[r2];
        }
    }
    if (0x0020 & opbit)  // AND
    {
        if (imm_flag)
        {
            reg[r0] = reg[r1] & imm5;
        }
        else
        {
            reg[r0] = reg[r1] & reg[r2];
        }
    }
    if (0x0200 & opbit) { reg[r0] = ~reg[r1]; } // NOT
    if (0x1000 & opbit) { reg[R_PC] = reg[r1]; } // JMP
    if (0x0010 & opbit)  // JSR
    {
        uint16_t long_flag = (instr >> 11) & 1;
        reg[R_R7] = reg[R_PC];
        if (long_flag)
        {
            pc_plus_off = reg[R_PC] + sign_extend(instr & 0x7FF, 11);
            reg[R_PC] = pc_plus_off;
        }
        else
        {
            reg[R_PC] = reg[r1];
        }
    }

    if (0x0004 & opbit) { reg[r0] = mem_read(pc_plus_off); } // LD
    if (0x0400 & opbit) { reg[r0] = mem_read(mem_read(pc_plus_off)); } // LDI
    if (0x0040 & opbit) { reg[r0] = mem_read(base_plus_off); }  // LDR
    if (0x4000 & opbit) { reg[r0] = pc_plus_off; } // LEA
    if (0x0008 & opbit) { mem_write(pc_plus_off, reg[r0]); } // ST
    if (0x0800 & opbit) { mem_write(mem_read(pc_plus_off), reg[r0]); } // STI
    if (0x0080 & opbit) { mem_write(base_plus_off, reg[r0]); } // STR
    if (0x8000 & opbit)  // TRAP
    {
         reg[R_R7] = reg[R_PC];
         
         switch (instr & 0xFF)
         {
             case TRAP_GETC:
                 /* read a single ASCII char */
                 reg[R_R0] = (uint16_t)getchar();
                 update_flags(R_R0);
                 break;
             case TRAP_OUT:
                 putc((char)reg[R_R0], stdout);
                 fflush(stdout);
                 break;
             case TRAP_PUTS:
                 {
                     /* one char per word */
                     uint16_t* c = memory + reg[R_R0];
                     while (*c)
                     {
                         putc((char)*c, stdout);
                         ++c;
                     }
                     fflush(stdout);
                 }
                 break;
             case TRAP_IN:
                 {
                     printf("Enter a character: ");
                     char c = getchar();
                     putc(c, stdout);
                     fflush(stdout);
                     reg[R_R0] = (uint16_t)c;
                     update_flags(R_R0);
                 }
                 break;
             case TRAP_PUTSP:
                 {
                     /* one char per byte (two bytes per word)
                        here we need to swap back to
                        big endian format */
                     uint16_t* c = memory + reg[R_R0];
                     while (*c)
                     {
                         char char1 = (*c) & 0xFF;
                         putc(char1, stdout);
                         char char2 = (*c) >> 8;
                         if (char2) putc(char2, stdout);
                         ++c;
                     }
                     fflush(stdout);
                 }
                 break;
             case TRAP_HALT:
                 puts("HALT");
                 fflush(stdout);
                 running = 0;
                 break;
         }
    }
    //if (0x0100 & opbit) { } // RTI
    if (0x4666 & opbit) { update_flags(r0); }
}
static void (*op_table[16])(uint16_t) = {
    ins<0>, ins<1>, ins<2>, ins<3>,
    ins<4>, ins<5>, ins<6>, ins<7>,
    NULL, ins<9>, ins<10>, ins<11>,
    ins<12>, NULL, ins<14>, ins<15>
};

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    
    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

   /* ▶ —– Stop-watch variables —– */
    using clock   = std::chrono::high_resolution_clock;
    auto  t_start = clock::now();
    uint64_t instr_count = 0;      // counts executed instructions
    /* ▶––––––––––––––––––––––––––– */

    while (running)
{
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t op    = instr >> 12;
    op_table[op](instr);

    ++instr_count;                       // keep this first

/* ---------- live-stats banner (refresh every 100 ms) ---------- */
static auto last_print = clock::now();
auto now = clock::now();
if (now - last_print >= std::chrono::milliseconds(100)) {
    last_print = now;

    uint64_t ns_tot = std::chrono::duration_cast<
                        std::chrono::nanoseconds>(now - t_start).count();
    double   ns_pi  = static_cast<double>(ns_tot) / instr_count;
    double   mips   = 1e3 / ns_pi;   // (1e9 / ns) / 1e6

    printf("\033[s");           // save cursor
    printf("\033[15;1H");       // move to row 15, col 1 (adjust if needed)
    printf("\033[K");           // clear line
    printf("Instr: %10llu | %.1f ns/op | %.2f MIPS",
           static_cast<unsigned long long>(instr_count),
           ns_pi, mips);
    printf("\033[u");           // restore cursor
    fflush(stdout);
}
/* ------------------------------------------------------------- */

/* ----------------------------------------------- */

/* ----------------------------------------------- */

}


    /* ▶ —– Stop-watch: print results —– */
    auto t_end   = clock::now();
    auto ns_total =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();

    double ns_per_instr = static_cast<double>(ns_total) / instr_count;
    double ips          = 1e9 / ns_per_instr;   // instr per second

    printf("\n===== LC-3 Benchmark =====\n");
    printf("Executed : %llu instructions\n", static_cast<unsigned long long>(instr_count));
    printf("Elapsed  : %.3f ms\n", ns_total / 1e6);
    printf("Latency  : %.1f ns / instr\n", ns_per_instr);
    printf("Throughput: %.2f M instr/s\n", ips / 1e6);
    printf("==========================\n");
    /* ▶––––––––––––––––––––––––––– */
    restore_input_buffering();
}
