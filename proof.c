#include "cpu.c"
#include <string.h>
#define inline __attribute__((always_inline))

inline char gb(char a, char b) {
	struct SM83 cpu;
	clean_cpu(&cpu);
	cpu.mem[cpu.regs.pc] = 0x80;
	cpu.regs.a = a;
	run_single_command(&cpu);
	return cpu.regs.a;
}

/* char double_DAAbble */
char pino(char a, char b, char c) {
	struct SM83 cpu;
	/* clean_cpu(&cpu); */

	cpu.regs.a = a;
	cpu.regs.b = b;
	cpu.regs.c = c;
	cpu.regs.pc = 100;

	const char code [] = {
		0xcb, 0x37, 0x47, 0xe6,
		0x0f, 0xb7, 0x27, 0xcb,
		0x20, 0x8f, 0x27, 0xcb,
		0x20, 0x8f, 0x27, 0xcb, 0x10, 0x8f, 0x27, 0xcb, 0x10, 0x8f, 0x27, 0xcb, 0x10, 0xc9
		/* 0xc9 */};
	__builtin_memcpy_inline(&cpu.mem[cpu.regs.pc], code, sizeof(code));

	#pragma clang loop unroll(full)
	for(int i=0; i<18; i++)
		run_single_command(&cpu);
	return cpu.regs.a;
}

inline char source(char a, char) {
	return a+1;
}

typedef bool is_true(char a, char data);

inline bool forall_a(is_true f, char data) {
	for (int a=0; a<=255; a++) {
		if (!f(a, data)) {
			return false;
		}
	}
	return true;
}

inline bool source_equals_gb(char a, char inst0) {
	return source(a, inst0) == gb(a, inst0);
}

bool tgt(char a, char inst0) { 
	/* { // original synthesis theorem */
	/* 	exists inst0: */
	/* 		forall a: */
	/* 	    	source(a) == gb(a, inst0); */
	/* } => */
	/* { // simple transform forall to exists */
	/* 	false = forall inst0: */
	/* 		!(forall a: */
	/* 			source(a) == gb(a, inst0)) */
	/* } => */
	/* { // remove the inner not */
	/* 	false = forall inst0: */
	/* 		exists a: */
	/* 			source(a) != gb(a, inst0) */
	/* } */

	/* return forall_a(source_equals_gb, inst0); */

	/* !forall inst0: */
	/* 	!(forall a: */
	/* 	    source(a) == gb(a, inst0) */
	/* forall_a(source_is_diff_from) */
	/* 	if (source(a, inst0) != gb(a, inst0)) */
	/* 		return false; */
	/* return true; */

	return source(a, inst0) != gb(a, inst0) || source(a, inst0) != gb(a, inst0);
}

bool src(char a, char) { return true; }
