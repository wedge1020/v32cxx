# v32io examples: keyboard and mouse support through v32/v32io.hpp.
#
#   make          build every example into bin/<name>.v32
#   make debug    the same, with debug info for v32c++ and the assembler
#   make clean
#
# Needs v32c++ and the Vircon32 DevTools (compile, assemble, packrom) on
# the PATH. Running the cartridges needs a v32io device: the v32io adaptor,
# or the modified DesktopEmulator (its v32kbd/v32mouse gamepad devices);
# see docs/V32IO.md.
#
# -I ../../.. finds this repository's own v32/ headers (v32/v32io.hpp), so
# the examples build without `make sysinstall`.

DEMOS         = typewriter paint monitor notepad
TRANSPILER    = v32c++ -I ../../..
COMPILER      = compile
ASSEMBLER     = assemble
PACKER        = packrom

DEBUG_FLAGS   =
ASM_DEBUG_FLAGS =

.PHONY: all debug clean prep
.PRECIOUS: obj/%.c obj/%.asm obj/%.vbin

all: prep $(DEMOS:%=bin/%.v32)

debug: DEBUG_FLAGS = -g
debug: ASM_DEBUG_FLAGS = -g program
debug: all

prep:
	@mkdir -p bin obj

# C++ -> C. v32c++ also writes obj/<name>.xml, whose <binary path> is
# obj/<name>.vbin; it moves up here, where packrom runs.
obj/%.c: %.cpp ../../../v32/v32io.hpp | prep
	$(TRANSPILER) $(DEBUG_FLAGS) -o obj/$*.c $*.cpp
	mv obj/$*.xml .

obj/%.asm: obj/%.c
	$(COMPILER) $(DEBUG_FLAGS) -o obj/$*.asm obj/$*.c

obj/%.vbin: obj/%.asm
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$*.vbin obj/$*.asm

bin/%.v32: obj/%.vbin
	$(PACKER) $*.xml -o bin/$*.v32

clean:
	rm -f bin/*.v32 obj/* *.xml
