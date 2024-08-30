#pragma once

/*
  this is a declaration of the CPU emulator for the game boy
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
// copied from SameBoy
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define GB_BIG_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define GB_LITTLE_ENDIAN
#else
#error Unable to detect endianess
#endif

union regs{ // the f reg is just for show; its not actually functional.
    struct {
        uint16_t af,
                 bc,
                 de,
                 hl,
                 sp,
                 pc;
	bool zf, nf, hf, cf;
    };
    struct {
#ifdef GB_BIG_ENDIAN
        uint8_t a, f,
                b, c,
	        d, e,
                h, l;
#else
        uint8_t f, a,
                c, b,
                e, d,
                l, h;
#endif
    };
    uint16_t regpairs[6]; // all the reg pairs in their order
    uint8_t registers[12]; // all the registers in their order
    // not including flags
};

struct SM83 {
    union regs regs;
    uint8_t mem[1<<16]; // assume the whole memory map is RAM for simplicity
    struct misc_state {
	bool halt;
    } misc;
};


// runs a single command from mem[pc]
// returns halting condition - true when halting, false otherwise
static bool run_single_command(struct SM83* cpu) ;

// runs until a loop is detected
static void run_cpu(struct SM83* cpu);

static void print_regs(const struct SM83* cpu);

// loads rom from a file
static void load_rom(struct SM83* cpu, const char* filename);

// resets the cpu block - all zeros, pc points to 0x100
static void clean_cpu(struct SM83* cpu);

#ifdef __cplusplus
}
#endif
