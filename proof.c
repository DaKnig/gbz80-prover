#include "code.h"
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

extern struct SM83 global_cpu;

static inline void call_from(struct SM83 *cpu, uint16_t call, uint16_t from) {
  cpu->regs.pc = from - 3;

  const char call_imm16 = 0xcd;

  // inject ret into the cpu,
  __builtin_memcpy_inline(&cpu->mem[cpu->regs.pc],
                          (const char[]){call_imm16, (char)call, call >> 8}, 3);
  // then run it
  run_single_command(cpu);
}

// runs the machine code in `code.h:code` as a function,
// on the arguments of this function
char pino(char a, char b) {
  // this is done to make sure the function is compiled correctly,
  // context free manner, "for whatever the state of the cpu would be"
  struct SM83 cpu = global_cpu;

  cpu.regs.a = a;
  cpu.regs.b = b;

  // this is because we must somehow assume the memcpy of code wont cause
  // UB. [1]
  const uint16_t code_location = 100;
  cpu.regs.pc = code_location;

  // fix the stack at 0xfefe.
  // otherwise any access to sp might change the code (including `ret`
  // and `call` and we use `call` instruction  to call into the Game Boy
  // code). [1]
  const uint16_t stack_location = 0xfefe;
  cpu.regs.sp = stack_location;

  __builtin_memcpy_inline(&cpu.mem[cpu.regs.pc], code, sizeof(code));

  const uint16_t ret_addr = 0xfeb0; // call it from there
  call_from(&cpu, code_location, ret_addr);

#pragma clang loop unroll(full)
  for (int i = 0; i < 22; i++)
    if (ret_addr != cpu.regs.pc)
      // run a single instruction in the cpu, mutating cpu in place.
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
