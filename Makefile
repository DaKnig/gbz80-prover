CLANG := time -p clang
LLFLAGS:= -O1  -mno-outline -fno-slp-vectorize
MIN_PASSES_PEPT=gvn,simplifycfg
MIN_PASSES_2=$(MIN_PASSES_PEPT),$(MIN_PASSES_PEPT)
MIN_PASSES_4=$(MIN_PASSES_2),$(MIN_PASSES_2)
MINIMAL_PASSES= -passes='$(MIN_PASSES_4),gvn,simplifycfg,gvn,simplifycfg'
CFLAGS := -std=gnu23 $(LLFLAGS)
OPT := opt
LLC := llc

%.ll: %.c cpu.c Makefile
	$(CLANG) $(CFLAGS) -S -emit-llvm -o 0$@ $<

	$(OPT) $(MINIMAL_PASSES) -o 1$@ 0$@
	$(OPT) $(MINIMAL_PASSES) -o 2$@ 1$@
	$(OPT) $(MINIMAL_PASSES) -o 3$@ 2$@
	$(OPT) $(MINIMAL_PASSES) -o 4$@ 3$@

	$(CLANG) -O3 -S -emit-llvm -o $@ 4$@

%.asm: %.ll Makefile
	$(LLC) $< --x86-asm-syntax=intel -o $@
