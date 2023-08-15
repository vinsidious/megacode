# hex file to be flashed
TARGET = MDR
# pic chip on which to flash the code
PIC = 16f1847
# source code
SRC := $(wildcard *.c)
# compiled code (assembly)
ASM := $(patsubst %.c,%.asm,$(SRC))
# the object files
OBJ := $(patsubst %.c,%.o,$(SRC))
# software verion used:
# pk2cmd: 1.21
# sdcc: 3.4.0
# gputils: 1.3.0

all: compile flash

# flash program (power the board externally since the PICkit2 might not provided enough current)
flash: $(TARGET).hex
	pk2cmd -P PIC$(PIC) -F $< -M -R

# compile stages
assembly: $(ASM)

objects: $(OBJ)

hex: $(TARGET).hex

compile: hex

# compile steps
%.asm: %.c
	sdcc -S --use-non-free -mpic14 -p$(PIC) -I. -o $@ $<

%.o: %.asm
	gpasm -I . -o $@ -c $<

$(TARGET).hex: $(OBJ)
	gplink -w -r -s /usr/share/gputils/lkr/$(PIC)_g.lkr -I /usr/share/sdcc/lib/pic14 -I /usr/share/sdcc/non-free/lib/pic14 -I . -o $@ libsdcce.lib pic$(PIC).lib $(OBJ)

# remove temporary files
clean:
	rm -f $(ASM) $(OBJ) $(TARGET).hex $(patsubst %.c,%.lst,$(SRC)) $(patsubst %.c,%.cod,$(SRC))
