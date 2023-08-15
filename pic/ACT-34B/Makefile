# variables
TARGET = ACT-34B
PIC = 12f617
# software verion used:
# pk2cmd: 1.21
# sdcc: 3.4.0
# gputils: 1.3.0

all: off flash on

# flash program
flash: $(TARGET).hex
	pk2cmd -PPIC$(PIC) -F$< -M

# power on remote
on:
	pk2cmd -PPIC$(PIC) -T

# power off remote
off:
	pk2cmd -PPIC$(PIC) -V0

# compile program
compile: $(TARGET).hex

# compile program C source code (includes a default code)
$(TARGET).hex: $(TARGET).c
	sdcc --std-c99 --opt-code-size --use-non-free -mpic14 -p$(PIC) $<

# remove temporary files
clean:
	rm -f $(TARGET).hex $(TARGET).lst $(TARGET).asm $(TARGET).adb $(TARGET).o $(TARGET).cod
