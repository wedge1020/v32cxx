# --- Configuration ---
DEMO          = tempest_32k
# -I ../../.. finds <v32/...>; -I inc finds this project's own headers
# (tempest.hpp, config.hpp, state.hpp). The modules in src/ are pooled
# into the one translation unit by #include lines in $(DEMO).cpp.
TRANSPILER    = v32c++ -I ../../.. -I inc
SOURCES       = $(DEMO).cpp $(wildcard src/*.cpp) $(wildcard inc/*.hpp)
COMPILER      = compile
OPTIMIZER     = v32opt -O3 -v
ASSEMBLER     = assemble
TEXTURE_TOOL  = png2vircon
SOUND_TOOL    = wav2vircon
PACKER        = packrom

# Is the optimizer installed? Looks up the first word of OPTIMIZER (v32opt)
# in PATH once, when make starts. Non-empty = found: the *Opt variants
# (Opt.asm, Opt.xml, Opt.vbin, Opt.v32) are built alongside the regular
# ones. Empty = not found: the optimizer is never invoked and only the
# regular build runs. Force it off with `make HAVE_OPTIMIZER=`.
HAVE_OPTIMIZER := $(shell command -v $(firstword $(OPTIMIZER)) 2>/dev/null)

# This variable is empty by default, but is set to -g during a debug build
DEBUG_FLAGS   =
ASM_DEBUG_FLAGS =

# --- Phony Targets ---
.PHONY: all asm vbin vsnd v32 debug clean

# Default target: Performs all steps in order
all: prep clang asm vbin v32

# Debug target: Sets the debug flags and builds all targets
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

# Transpile C++ to C
obj/$(DEMO).c: prep $(SOURCES)
	$(TRANSPILER) $(DEBUG_FLAGS)    -o obj/$(DEMO).c  $(DEMO).cpp

# Compile C to Assembly
asm: obj/$(DEMO).asm

obj/$(DEMO).asm: prep obj/$(DEMO).c
	$(COMPILER)   $(DEBUG_FLAGS) -o obj/$(DEMO).asm    obj/$(DEMO).c
ifneq ($(HAVE_OPTIMIZER),)
	$(OPTIMIZER)  $(DEBUG_FLAGS) -o obj/$(DEMO)Opt.asm obj/$(DEMO).asm
endif
	mv obj/$(DEMO).xml .
ifneq ($(HAVE_OPTIMIZER),)
	cat $(DEMO).xml | sed 's/\.vbin/Opt.vbin/g' > $(DEMO)Opt.xml
endif


# Assemble Assembly to Binary
vbin: obj/$(DEMO).vbin

obj/$(DEMO).vbin: prep obj/$(DEMO).asm
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$(DEMO).vbin    obj/$(DEMO).asm
ifneq ($(HAVE_OPTIMIZER),)
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$(DEMO)Opt.vbin obj/$(DEMO)Opt.asm
endif

# Process PNG sprite sheet to Vircon texture
vtex: textures/$(DEMO)_sprites.vtex

textures/$(DEMO)_sprites.vtex: textures/$(DEMO)_sprites.png
	$(TEXTURE_TOOL) textures/$(DEMO)_sprites.png -o textures/$(DEMO)_sprites.vtex

vsnd:
	$(SOUND_TOOL)   sounds/blip.wav       -o sounds/blip.vsnd
	$(SOUND_TOOL)   sounds/boom.wav       -o sounds/boom.vsnd
	$(SOUND_TOOL)   sounds/clear.wav      -o sounds/clear.vsnd
	$(SOUND_TOOL)   sounds/death.wav      -o sounds/death.vsnd
	$(SOUND_TOOL)   sounds/jump.wav       -o sounds/jump.vsnd
	$(SOUND_TOOL)   sounds/m_title.wav    -o sounds/m_title.vsnd
	$(SOUND_TOOL)   sounds/m_track1.wav   -o sounds/m_track1.vsnd
	$(SOUND_TOOL)   sounds/m_track2.wav   -o sounds/m_track2.vsnd
	$(SOUND_TOOL)   sounds/m_track3.wav   -o sounds/m_track3.vsnd
	$(SOUND_TOOL)   sounds/m_track4.wav   -o sounds/m_track4.vsnd
	$(SOUND_TOOL)   sounds/pickup.wav     -o sounds/pickup.vsnd
	$(SOUND_TOOL)   sounds/shootsound.wav -o sounds/shootsound.vsnd
	$(SOUND_TOOL)   sounds/zap.wav        -o sounds/zap.vsnd

# Pack ROM cartridge (Depends on the binary and texture to ensure they are built first)
v32: vsnd bin/$(DEMO).v32

bin/$(DEMO).v32: obj/$(DEMO).vbin
	$(PACKER) $(DEMO).xml    -o bin/$(DEMO).v32
ifneq ($(HAVE_OPTIMIZER),)
	$(PACKER) $(DEMO)Opt.xml -o bin/$(DEMO)Opt.v32
endif

context:
	split -a 1 -b 45K -d obj/$(DEMO).asm obj/$(DEMO)_part
	@for file in obj/$(DEMO)_part?; do \
		mv "$$file" "$$file.asm.txt"; \
	done

# --- Clean Up ---
clean:
	rm -f bin/*.v32 obj/* textures/*.vtex sounds/*.vsnd *.xml
