# Rogue 5.4.4 for the Vircon32 console, built with v32c++.
#
#   make          transpile, compile, assemble and pack bin/rogue.v32
#   make debug    the same, with debug info for the assembler and v32c++
#   make clean
#
# Needs v32c++ and the Vircon32 DevTools (compile, assemble, packrom) on
# PATH. There is nothing else to convert: the game has no texture and no
# sound of its own -- it draws with the BIOS font.

# --- Configuration ---
DEMO          = rogue
# -I ../../../libc finds the C library and curses that v32c++ ships
# (stdio.h, stdlib.h, curses.h, ... and their .c files); -I v32 finds this
# port's config.h; -I src finds rogue.h from the files in v32/. The whole
# program is pooled into one translation unit by the #include lines in
# $(DEMO).c -- Vircon32 C compiles exactly one file.
TRANSPILER    = v32c++ -I ../../../libc -I v32 -I src
SOURCES       = $(DEMO).c $(wildcard src/*.c) $(wildcard src/*.h) \
                $(wildcard v32/*.c) $(wildcard v32/*.h) \
                $(wildcard ../../../libc/*.c) $(wildcard ../../../libc/*.h)
COMPILER      = compile
OPTIMIZER     = v32opt -O3 -v
ASSEMBLER     = assemble
PACKER        = packrom

# Is the optimizer installed? Non-empty = found: the *Opt variants are
# built alongside the regular ones. Force it off with `make HAVE_OPTIMIZER=`.
HAVE_OPTIMIZER := $(shell command -v $(firstword $(OPTIMIZER)) 2>/dev/null)

# Empty by default; set by the debug target
DEBUG_FLAGS     =
ASM_DEBUG_FLAGS =

# A fixed random seed, for reproducible test runs: make SEED=3
ifdef SEED
TRANSPILER   += -DV32LIBC_FIXED_TIME=$(SEED)
endif

# --- Phony Targets ---
.PHONY: all clang asm vbin v32 debug clean prep

all: prep clang asm vbin v32

debug: DEBUG_FLAGS = -g
debug: ASM_DEBUG_FLAGS = -g program
debug: all

# --- Build Rules ---
prep:
	@mkdir -p bin obj
ifeq ($(HAVE_OPTIMIZER),)
	@echo "note: $(firstword $(OPTIMIZER)) not found in PATH -- skipping the optimized (Opt) build"
endif

clang: obj/$(DEMO).c

# Transpile C to Vircon32 C (this also writes obj/$(DEMO).xml)
obj/$(DEMO).c: $(SOURCES) | prep
	$(TRANSPILER) $(DEBUG_FLAGS)    -o obj/$(DEMO).c  $(DEMO).c

# Compile Vircon32 C to assembly
asm: obj/$(DEMO).asm

obj/$(DEMO).asm: obj/$(DEMO).c
	$(COMPILER)   $(DEBUG_FLAGS) -o obj/$(DEMO).asm    obj/$(DEMO).c
ifneq ($(HAVE_OPTIMIZER),)
	$(OPTIMIZER)  $(DEBUG_FLAGS) -o obj/$(DEMO)Opt.asm obj/$(DEMO).asm
endif
	cp obj/$(DEMO).xml .
ifneq ($(HAVE_OPTIMIZER),)
	cat $(DEMO).xml | sed 's/\.vbin/Opt.vbin/g' > $(DEMO)Opt.xml
endif

# Assemble to binary
vbin: obj/$(DEMO).vbin

obj/$(DEMO).vbin: obj/$(DEMO).asm
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$(DEMO).vbin    obj/$(DEMO).asm
ifneq ($(HAVE_OPTIMIZER),)
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$(DEMO)Opt.vbin obj/$(DEMO)Opt.asm
endif

# Pack the cartridge
v32: bin/$(DEMO).v32

bin/$(DEMO).v32: obj/$(DEMO).vbin
	$(PACKER) $(DEMO).xml    -o bin/$(DEMO).v32
ifneq ($(HAVE_OPTIMIZER),)
	$(PACKER) $(DEMO)Opt.xml -o bin/$(DEMO)Opt.v32
endif

# --- Clean Up ---
clean:
	rm -f bin/*.v32 obj/* *.xml
