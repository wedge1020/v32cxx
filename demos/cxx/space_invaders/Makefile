# --- Configuration ---
DEMO          = space_invaders
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
	$(SOUND_TOOL)   sounds/alien_death.wav  -o sounds/alien_death.vsnd
	$(SOUND_TOOL)   sounds/bunker_hit.wav   -o sounds/bunker_hit.vsnd
	$(SOUND_TOOL)   sounds/extra_life.wav   -o sounds/extra_life.vsnd
	$(SOUND_TOOL)   sounds/march0.wav       -o sounds/march0.vsnd
	$(SOUND_TOOL)   sounds/march1.wav       -o sounds/march1.vsnd
	$(SOUND_TOOL)   sounds/march2.wav       -o sounds/march2.vsnd
	$(SOUND_TOOL)   sounds/march3.wav       -o sounds/march3.vsnd
	$(SOUND_TOOL)   sounds/march4.wav       -o sounds/march4.vsnd
	$(SOUND_TOOL)   sounds/march5.wav       -o sounds/march5.vsnd
	$(SOUND_TOOL)   sounds/march6.wav       -o sounds/march6.vsnd
	$(SOUND_TOOL)   sounds/player_death.wav -o sounds/player_death.vsnd
	$(SOUND_TOOL)   sounds/saucer_death.wav -o sounds/saucer_death.vsnd
	$(SOUND_TOOL)   sounds/saucer.wav       -o sounds/saucer.vsnd
	$(SOUND_TOOL)   sounds/shoot.wav        -o sounds/shoot.vsnd

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
