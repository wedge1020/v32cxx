# --- Configuration ---
DEMO          = star_raiders
TRANSPILER    = v32c++
COMPILER      = compile
ASSEMBLER     = assemble
TEXTURE_TOOL  = png2vircon
SOUND_TOOL    = wav2vircon
PACKER        = packrom

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

clang: obj/$(DEMO).c

# Transpile C++ to C
obj/$(DEMO).c: prep $(DEMO).cpp
	$(TRANSPILER) $(DEBUG_FLAGS)    -o obj/$(DEMO).c  $(DEMO).cpp
	#$(TRANSPILER) $(DEBUG_FLAGS) -c -o obj/assets.h   inc/assets.h
	#$(TRANSPILER) $(DEBUG_FLAGS) -c -o obj/core.h     inc/core.h
	#$(TRANSPILER) $(DEBUG_FLAGS) -c -o obj/entities.h inc/entities.h
	#$(TRANSPILER) $(DEBUG_FLAGS) -c -o obj/game.h     inc/game.h
	#$(TRANSPILER) $(DEBUG_FLAGS) -c -o obj/platform.h inc/platform.h
	#$(TRANSPILER) $(DEBUG_FLAGS)    -o obj/$(DEMO).c  src/main.cpp

# Compile C to Assembly
asm: obj/$(DEMO).asm

obj/$(DEMO).asm: prep obj/$(DEMO).c
	$(COMPILER)   $(DEBUG_FLAGS) -o obj/$(DEMO).asm obj/$(DEMO).c
	mv obj/$(DEMO).xml .

# Assemble Assembly to Binary
vbin: obj/$(DEMO).vbin

obj/$(DEMO).vbin: prep obj/$(DEMO).asm
	$(ASSEMBLER) $(ASM_DEBUG_FLAGS) -o obj/$(DEMO).vbin obj/$(DEMO).asm

# Process PNG sprite sheet to Vircon texture
vtex: textures/$(DEMO)_sprites.vtex

textures/$(DEMO)_sprites.vtex: textures/$(DEMO)_sprites.png
	$(TEXTURE_TOOL) textures/$(DEMO)_sprites.png -o textures/$(DEMO)_sprites.vtex

vsnd:
	$(SOUND_TOOL)   sounds/alert.wav      -o sounds/alert.vsnd
	$(SOUND_TOOL)   sounds/beep.wav       -o sounds/beep.vsnd
	$(SOUND_TOOL)   sounds/gameplay.wav   -o sounds/gameplay.vsnd
	$(SOUND_TOOL)   sounds/engine.wav     -o sounds/engine.vsnd
	$(SOUND_TOOL)   sounds/explosion.wav  -o sounds/explosion.vsnd
	$(SOUND_TOOL)   sounds/hyperspace.wav -o sounds/hyperspace.vsnd
	$(SOUND_TOOL)   sounds/missile.wav    -o sounds/missile.vsnd
	$(SOUND_TOOL)   sounds/replenish.wav  -o sounds/replenish.vsnd
	$(SOUND_TOOL)   sounds/title.wav      -o sounds/title.vsnd

# Pack ROM cartridge (Depends on the binary and texture to ensure they are built first)
v32: vsnd bin/$(DEMO).v32

bin/$(DEMO).v32: obj/$(DEMO).vbin
	$(PACKER) $(DEMO).xml -o bin/$(DEMO).v32

context:
	split -a 1 -b 45K -d obj/$(DEMO).asm obj/$(DEMO)_part
	@for file in obj/$(DEMO)_part?; do \
		mv "$$file" "$$file.asm.txt"; \
	done

# --- Clean Up ---
clean:
	rm -f bin/*.v32 obj/* textures/*.vtex sounds/*.vsnd *.xml
