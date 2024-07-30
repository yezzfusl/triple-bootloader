# Makefile for Arduino Uno Bootloader

# MCU settings
MCU = atmega328p
F_CPU = 16000000UL

# Compiler settings
CC = avr-gcc
CXX = avr-g++
AS = avr-as
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size

# Flags
CFLAGS = -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = -mmcu=$(MCU)

# Source files
C_SRC = src/main.c
CPP_SRC = src/main.cpp
ASM_SRC = src/bootloader.asm

# Output files
C_OUTPUT = bootloader_c.elf
CPP_OUTPUT = bootloader_cpp.elf
ASM_OUTPUT = bootloader_asm.elf

# Hex files
C_HEX = bootloader_c.hex
CPP_HEX = bootloader_cpp.hex
ASM_HEX = bootloader_asm.hex

all: $(C_HEX) $(CPP_HEX) $(ASM_HEX)

$(C_HEX): $(C_SRC)
	$(CC) $(CFLAGS) -o $(C_OUTPUT) $(C_SRC)
	$(OBJCOPY) -O ihex -R .eeprom $(C_OUTPUT) $@

$(CPP_HEX): $(CPP_SRC)
	$(CXX) $(CXXFLAGS) -o $(CPP_OUTPUT) $(CPP_SRC)
	$(OBJCOPY) -O ihex -R .eeprom $(CPP_OUTPUT) $@

$(ASM_HEX): $(ASM_SRC)
	$(AS) $(ASFLAGS) -o $(ASM_OUTPUT) $(ASM_SRC)
	$(OBJCOPY) -O ihex -R .eeprom $(ASM_OUTPUT) $@

clean:
	rm -f *.hex *.elf *.o

.PHONY: all clean

