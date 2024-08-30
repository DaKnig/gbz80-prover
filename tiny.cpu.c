/* #define NDEBUG */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "cpu.h"

static inline bool run_single_prefix_command(struct SM83* cpu);

__attribute__((always_inline)) static inline uint8_t fetch(struct SM83* cpu) {
	return cpu->mem[cpu->regs.pc++];
}

__attribute__((always_inline)) static inline uint16_t alu(struct SM83* cpu, uint8_t src, int instr, bool cf, uint32_t a) {
	int16_t result = a;
	cpu->regs.f = 0;
	switch (instr) {
	case 0: // ADD
		result += src;
		break;
	case 1: // ADC
		result += src + cf;
		break;
	case 2: // SUB
		result -= src;
		cpu->regs.f |= 1<<6;
		break;
	case 3: // SBC
		result -= src + cf;
		cpu->regs.f |= 1<<6;
		break;
	case 4: // AND
		result &= src;
		cpu->regs.f |= 1<<5;
		break;
	case 5: // XOR
		result ^= src;
		break;
	case 6: // OR
		result |= src;
		break;
	case 7: // CP
		result -= src;
		cpu->regs.f |= 1<<6;
		break;
	default: __builtin_unreachable();
	}

	if (instr != 6) // hf
		cpu->regs.f |= ((result ^ a ^ src) & 0x10) << 1;
	cpu->regs.f |= ((uint8_t)result == 0) << 7;
	cpu->regs.f |= !!(result & 0x100) << 4;
	if (instr != 7)
		return result;
	else
		return a;
}

__attribute__((always_inline)) static inline uint8_t* regpair_offset_bcdehlhl(struct SM83 *cpu, uint32_t idx) {
	switch(idx) {
	case 0: return &cpu->mem[cpu->regs.bc];
	case 1: return &cpu->mem[cpu->regs.de];
	case 2: return &cpu->mem[cpu->regs.hl++];
	case 3: return &cpu->mem[cpu->regs.hl--];
	default: __builtin_unreachable();
	}
}

__attribute__((always_inline)) static inline uint8_t* reg_offset_bcdehlhla(struct SM83 *cpu, uint32_t idx) {
	switch(idx) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5: return cpu->regs.registers + 2 + (1^idx);
	case 6: return &cpu->mem[cpu->regs.hl];
	case 7: return &cpu->regs.a;
	default: __builtin_unreachable();
	}
}

__attribute__((always_inline)) bool run_single_command(struct SM83* cpu) {
    // add flags and all later
    uint8_t instr[4]; memcpy(instr, &cpu->mem[cpu->regs.pc], sizeof(instr));
	uint8_t imm8 = instr[1];
	uint16_t imm16 = instr[1] + (instr[2] << 8);

    const bool zf =     (cpu->regs.f>>7), nf = 1 & (cpu->regs.f>>6);
    const bool hf = 1 & (cpu->regs.f>>5), cf = 1 & (cpu->regs.f>>4);
    (void)hf, (void)nf;

	uint8_t cond = !(instr[0] & 8) - ((instr[0] & 16) ? cf : zf);

#define imm8_f(cpu) (	  \
        cpu->regs.pc++,   \
        imm8		      \
		)
#define imm16_f(cpu) (	 \
        cpu->regs.pc+=2, \
        imm16            \
		)
#define regpair_offset_bcdehlsp(idx) ( \
        cpu->regs.regpairs + (idx) + 1 \
		)
#define regpair_offset_bcdehlaf(idx) (		 \
		cpu->regs.regpairs + ((idx) + 1) % 4 \
		)

	cpu->regs.pc++;
    switch (instr[0]) {
	case 0x00:
		return 0;
	case 0x10: // stop
		cpu->regs.pc++;
		return 1;
    case 0x07: case 0x17: case 0x0f: case 0x1f: // rlca rla rrca rra
		cpu->regs.pc--;
		run_single_prefix_command(cpu);
		cpu->regs.f &= 0x10; // zero the zero
		return 0;
    // https://github.com/pinobatch/numism/blob/main/docs/gb_emu_testing.md
    case 0x27: {// DAA
	{
	    int16_t result = cpu->regs.a;
	    enum {GB_ZERO_FLAG=0x80, GB_SUBTRACT_FLAG=0x40,
		GB_HALF_CARRY_FLAG=0x20, GB_CARRY_FLAG=0x10};

	    cpu->regs.f &= ~GB_ZERO_FLAG;

	    if (cpu->regs.f & GB_SUBTRACT_FLAG) {
		if (cpu->regs.f & GB_HALF_CARRY_FLAG) {
		    result = (result - 0x06) & 0xFF;
		}

		if (cpu->regs.f & GB_CARRY_FLAG) {
		    result -= 0x60;
		}
	    }
	    else {
		if ((cpu->regs.f & GB_HALF_CARRY_FLAG) || (result & 0x0F) > 0x09) {
		    result += 0x06;
		}

		if ((cpu->regs.f & GB_CARRY_FLAG) || result > 0x9F) {
		    result += 0x60;
		}
	    }

	    if ((result & 0xFF) == 0) {
		cpu->regs.f |= GB_ZERO_FLAG;
	    }

	    if ((result & 0x100) == 0x100) {
		cpu->regs.f |= GB_CARRY_FLAG;
	    }

	    cpu->regs.f &= ~GB_HALF_CARRY_FLAG;
	    cpu->regs.a = result;
	}
	return 0;
	}
    case 0x37: //SCF
	cpu->regs.f &= 0x8f; // only keep zf
	cpu->regs.f |= 1 << 4; //set cf

	return 0;

    case 0x08: { // ld (a16), sp
	uint16_t imm16 = imm16_f(cpu);
	cpu->mem[imm16] = cpu->regs.sp & 0xff;
	cpu->mem[imm16+1]=cpu->regs.sp >> 8;
	return 0;
    }
	case 0x20: case 0x28: case 0x30: case 0x38: // jr cc, e8
		if (!cond) {
			imm8_f(cpu);
			return 0;
		} // fallthrough
	case 0x18: // jr e8
		cpu->regs.pc += (int8_t) imm8_f(cpu);
		return 0;

	case 0x09: case 0x19: case 0x29: case 0x39: { // add hl, r16
		uint16_t* regpair = regpair_offset_bcdehlsp(instr[0] >> 4);
	    uint32_t new_val = cpu->regs.hl+ *regpair;
	    cpu->regs.f &= 0x80; // keep zf
	    cpu->regs.f |= new_val > 0xffff ? 0x10 : 0;
	    uint16_t bottom_12 = (cpu->regs.hl & 0x0fff) + (*regpair & 0x0fff);
	    cpu->regs.f |= bottom_12 >= 0x1000 ? 0x20 : 0;
	    cpu->regs.hl = new_val;
	    return 0;
	}

	case 0x0a: case 0x1a: case 0x2a: case 0x3a: // ld a, [r16]
		cpu->regs.a = *regpair_offset_bcdehlhl(cpu, instr[0] >> 4);
		return 0;

    case 0x2f: // CPL
	cpu->regs.f |= 0x60;
	cpu->regs.a ^= 0xff;

	return 0;
    case 0x3f: // CCF
	cpu->regs.f &= 0x90; // nf = hf = 0
	cpu->regs.f ^= 0x10; // invert cf
	return 0;

	case 0xc0: case 0xd0: case 0xc8: case 0xd8: // ret cc
		if (!cond) {
			return 0;
		} // fallthrough
	case 0xc9: case 0xd9: // ret, reti
		cpu->regs.pc = cpu->mem[cpu->regs.sp++];
		cpu->regs.pc|= cpu->mem[cpu->regs.sp++]<<8;
		return 0;

    case 0xe0:
	cpu->mem[0xff00|imm8_f(cpu)] = cpu->regs.a;
	return 0;
    case 0xf0:
	cpu->regs.a = cpu->mem[0xff00|imm8_f(cpu)];
	return 0;
	case 0xf5: // push af
		cpu->mem[--cpu->regs.sp] = cpu->regs.af >> 8;
		cpu->mem[--cpu->regs.sp] = cpu->regs.af;
		return 0;
    case 0xf1: // pop af
		cpu->regs.f = cpu->mem[cpu->regs.sp++] & 0xf0;
		cpu->regs.a = cpu->mem[cpu->regs.sp++];
		return 0;
	case 0xc2: case 0xd2: case 0xca: case 0xda: // jp cc, a16
		if (!cond) {
			(void) imm16_f(cpu);
			return 0;
		} // fallthrough
	case 0xc3: {
		cpu->regs.pc = imm16_f(cpu);
		return 0;
	}

    case 0xe2:
	cpu->mem[0xff00|cpu->regs.c] = cpu->regs.a;
	return 0;
    case 0xf2:
	cpu->regs.a = cpu->mem[0xff00|cpu->regs.c];
	return 0;
    // some bad instructions - hang and error out.
    case 0xf3: // DI; we dont need this.
    // acts as a noop
	return 0;
	case 0xc4: case 0xd4: case 0xcc: case 0xdc: // call cc, n16
		if (!cond) {
			(void) imm16_f(cpu);
			return 0;
		} // fallthrough
	case 0xcd: { // call n16
		uint16_t imm16 = imm16_f(cpu);
		cpu->mem[--cpu->regs.sp] = cpu->regs.pc >> 8;
		cpu->mem[--cpu->regs.sp] = cpu->regs.pc;
		cpu->regs.pc = imm16;
		return 0;
	}

	case 0xc5: case 0xd5: case 0xe5: { // push r16
		uint16_t regpair = *regpair_offset_bcdehlaf((instr[0] >> 4) - 0xc);
		cpu->mem[--cpu->regs.sp] = regpair >> 8;
		cpu->mem[--cpu->regs.sp] = regpair;
		return 0;
	}

	case 0xc1: case 0xd1: case 0xe1: { // pop r16
		uint16_t* regpair = regpair_offset_bcdehlaf((instr[0] >> 4) - 0xc);
		*regpair = cpu->mem[cpu->regs.sp++];
		*regpair|= cpu->mem[cpu->regs.sp++]<<8;
		return 0;
	}

	case 0xc7: case 0xcf: case 0xd7: case 0xdf:
	case 0xe7: case 0xef: case 0xf7: case 0xff: // rst
		cpu->mem[--cpu->regs.sp] = cpu->regs.pc >> 8;
		cpu->mem[--cpu->regs.sp] = cpu->regs.pc;
		cpu->regs.pc = instr[0] - 0xc7;
		return 0;
    case 0xe8: case 0xf8: { // ADD SP, r8    &&    LD HL, SP + r8
	alu(cpu, imm8_f(cpu), 0, 0, cpu->regs.sp & 0xff);
	cpu->regs.f &= ~0x80;
	// auto dest = instr[0] == 0xf8 ? &hl : &sp;
	auto dest = (instr[0] == 0xe8) + &cpu->regs.hl;
	*dest = cpu->regs.sp + (int8_t) imm8;
	return 0;
    }
    case 0xe9: case 0xf9: // jp hl && ld sp, hl
	/* auto dest = instr[0] == 0xf9? &cpu->regs.sp : &cpu->regs.pc; */
	auto dest = (instr[0] == 0xe9) + &cpu->regs.sp;
	*dest = cpu->regs.hl;
	return 0;

    case 0xea: // ld (a16), a
	cpu->mem[imm16_f(cpu)] = cpu->regs.a;
	return 0;
    case 0xfa:
        cpu->regs.a = cpu->mem[imm16_f(cpu)];
	return 0;

    case 0xcb: // prefix
	return run_single_prefix_command(cpu);
    case 0xfb: //EI; no interrupts -> noop
	return 0;
    }
	uint8_t *src = reg_offset_bcdehlhla(cpu, instr[0] & 7);
	uint8_t *dest = reg_offset_bcdehlhla(cpu, (instr[0] >> 3) & 7);

	switch(instr[0] & 0300) {
	case 0100: // ld r8, r8
		*dest = *src;
		return instr[0] == 0166; // ld [hl], [hl] == halt
	case 0200: // op a, r8
		cpu->regs.a = alu(cpu, *src, (instr[0] >> 3) & 7, cf, cpu->regs.a);
		return 0;
	case 0000: // regular ops of first quadrant
		switch (instr[0] & 0xf) { // the column
		case 0x1: // ld r16, n16
			uint16_t* regpair = regpair_offset_bcdehlsp(instr[0] >> 4);
			*regpair = imm16_f(cpu);
			return 0;
		case 0x2: // ld [r16], a
			*regpair_offset_bcdehlhl(cpu, instr[0] >> 4) = cpu->regs.a;
			return 0;
		case 0x3: case 0xb: // inc r16 ; dec r16
			*regpair_offset_bcdehlsp(instr[0] >> 4) += (instr[0] & 8 ? -1 : 1);
			return 0;
		case 0x4: case 0x5: case 0xc: case 0xd:  // inc r8 ; dec r8
			uint8_t *reg = dest;
			uint8_t f = cpu->regs.f & 0x10;
			*reg = alu(cpu, 0, (instr[0] * 2 + 1) & 3, 1, *reg);
			cpu->regs.f &= ~0x10;
			cpu->regs.f |= f;
			return 0;
		case 0x6: case 0xe: { // ld r8, n8
			uint8_t *reg = dest;
			*reg = imm8_f(cpu);
			return 0;
		}
		}
	}
	if (0xfe == (instr[0] | 070)) { // op a, n8
		cpu->regs.a = alu(cpu, imm8_f(cpu), (instr[0] >> 3) & 7, cf, cpu->regs.a);
		return 0;
	}

	/* __builtin_unreachable(); */
    /* fprintf(stderr, "unhandled opcode: %02x\n", instr[0]); */
    return 1;
}

__attribute__((always_inline)) static inline bool run_single_prefix_command(struct SM83* cpu) {
    uint8_t opc = fetch(cpu);
    uint8_t* hl_ = &cpu->mem[cpu->regs.hl];

	int op = opc & 7;
	int n = (opc >> 3) & 7;

	uint8_t* reg;
	switch(op) {
	case 0: reg = &cpu->regs.b; break;
	case 1: reg = &cpu->regs.c; break;
	case 2: reg = &cpu->regs.d; break;
	case 3: reg = &cpu->regs.e; break;
	case 4: reg = &cpu->regs.h; break;
	case 5: reg = &cpu->regs.l; break;
	case 6: reg = &     (*hl_); break;
	case 7: reg = &cpu->regs.a; break;
	default: __builtin_unreachable();
	}

	int block = opc >> 6;
	if (block) {
		int bit = 1 << n;
		if (block == 3) {        // SET instructions
			*reg |= bit;
		} else if (block == 2) { // RES instructions
			*reg &= ~bit;
		} else if (block == 1) { // BIT instructions
			cpu->regs.f&= 0x10;
			cpu->regs.f|= 0x20|((*reg & bit)?0:0x80);
		}
	} else {         // one of the 8 "tiny instructions"
		int opc = n; // more meaningful name in this case
		uint8_t bit7_cf = *reg & 0x80 ? 0x10 : 0;
		uint8_t bit0_cf = *reg & 1 ? 0x10 : 0;
		uint8_t old_f = cpu->regs.f;
		switch(opc) {
		case 0: // rlc
			*reg = (*reg << 1) | (*reg >> 7);
			break;
		case 1: // rrc
			*reg = (*reg >> 1) | (*reg << 7);
			break;
		case 2: // rl
			*reg = (*reg << 1) | (old_f & 0x10? 1 : 0);
			break;
		case 3: // rr
			*reg = (*reg >> 1) | (old_f & 0x10? 0x80 : 0);
			break;
		case 4: // sla
			*reg = *reg << 1;
			break;
		case 5: // sra
			*reg = (*reg >> 1) | (*reg & 0x80);
			break;
		case 6: // swap
			*reg = (*reg >> 4) | (*reg << 4);
			break;
		case 7: // srl
			*reg = *reg >> 1;
			break;
		}
		if (opc == 0 || opc == 2 || opc == 4) // rlc rl sla
			cpu->regs.f = bit7_cf;
		else if (opc == 6) // swap
			cpu->regs.f = 0;
		else // rrc rr sra srl
			cpu->regs.f = bit0_cf;
		cpu->regs.f |= *reg == 0? 0x80: 0x00;
	}
    return 0;
}

void run_cpu(struct SM83* cpu) {
    bool halting;
    uint16_t prev_pc;
    do {
	prev_pc = cpu->regs.pc;
	halting = run_single_command(cpu);
    } while (!halting && prev_pc != cpu->regs.pc) ;
}

void print_regs(const struct SM83* cpu) {
    printf("af: %04x\nbc: %04x\nde: %04x\nhl: %04x\nsp: %04x\npc: %04x\n"
	   "z: %x, n: %x, h: %x, c: %x\n",
	   cpu->regs.af,
	   cpu->regs.bc,
	   cpu->regs.de,
	   cpu->regs.hl,
	   cpu->regs.sp,
	   cpu->regs.pc,
	   cpu->regs.zf, cpu->regs.nf, cpu->regs.hf, cpu->regs.cf);
}

void load_rom(struct SM83* cpu, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
	fprintf(stderr, "can't open file `%s`\n", filename);
	perror("");
	return;
    }

    size_t bytes_read = fread(cpu->mem, 1, sizeof(cpu->mem), f);
    printf("read %zd bytes\n", bytes_read);

    fclose(f);
}

void clean_cpu(struct SM83* cpu) {
    memset(&cpu->mem, 0x00, sizeof(cpu->mem));
    cpu->regs.pc = 0x0100;
    // stuff set up by the bootrom
    cpu->regs.af = 0x01B0;
    cpu->regs.bc = 0x0013;
    cpu->regs.de = 0x00d8;
    cpu->regs.hl = 0x014d;
    cpu->regs.sp = 0xfffe;
    // setup constant [rLY] = 144 = 0x90 - for tests
    cpu->mem[0xff44] = 0x90;
    // start non halted and without breakpoints
    cpu->misc.halt = false;
}
