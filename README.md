# Triple Bootloader
-- Arduino Uno bootloader in C, C++, and assembly
This project implements a complete Arduino Uno bootloader in C, C++, and assembly. The bootloader is       responsible for loading the main program into the microcontroller's flash memory and starting its execution.

## Project Structure
- /triple-bootloader
- ├── src
- │   ├── main.c
- │   ├── main.cpp
- │   ├── bootloader.asm
- ├── Makefile
- ├── README.md
## Building the Project

To build the project, make sure you have the AVR-GCC toolchain installed. Then, run the following command:
```make```
This will generate three hex files:
- `bootloader_c.hex`: C implementation
- `bootloader_cpp.hex`: C++ implementation
- `bootloader_asm.hex`: Assembly implementation

## Cleaning the Project

To clean the build artifacts, run:
```make clean```
## Implementation Details

The bootloader is implemented in three different languages:
1. C (`src/main.c`)
2. C++ (`src/main.cpp`)
3. Assembly (`src/bootloader.asm`)

Each implementation provides the same functionality, demonstrating different approaches to creating an Arduino Uno bootloader.
