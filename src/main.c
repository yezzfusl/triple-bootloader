#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>

#define BOOTLOADER_START_ADDRESS 0x7000
#define F_CPU 16000000UL
#define BAUD 115200
#define BAUD_PRESCALE ((F_CPU / 16 / BAUD) - 1)

#define PAGESIZE SPM_PAGESIZE
#define APP_END (BOOTLOADER_START_ADDRESS - 1)

// STK500 constants
#define STK_OK              0x10
#define STK_FAILED          0x11
#define STK_UNKNOWN         0x12
#define STK_INSYNC          0x14
#define STK_NOSYNC          0x15
#define CRC_EOP             0x20

void initialize_mcu(void) {
    cli();
    MCUSR &= ~(1 << WDRF);
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0x00;
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;
    SPH = (RAMEND & 0xFF00) >> 8;
    SPL = RAMEND & 0xFF;
    sei();
}

void initialize_uart(void) {
    UBRR0H = (BAUD_PRESCALE >> 8);
    UBRR0L = BAUD_PRESCALE;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

unsigned char uart_receive(void) {
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

void flash_erase_page(uint32_t page) {
    boot_page_erase(page);
    boot_spm_busy_wait();
}

void flash_write_page(uint32_t page, uint8_t *buf) {
    for (uint16_t i = 0; i < SPM_PAGESIZE; i += 2) {
        uint16_t w = *buf++;
        w += (*buf++) << 8;
        boot_page_fill(page + i, w);
    }
    boot_page_write(page);
    boot_spm_busy_wait();
}

uint8_t getch(void) {
    return uart_receive();
}

void putch(uint8_t ch) {
    uart_transmit(ch);
}

uint8_t get_hex_nibble(void) {
    uint8_t ch = getch();
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return 0;
}

uint8_t get_hex_byte(void) {
    return (get_hex_nibble() << 4) | get_hex_nibble();
}

void bootloader(void) {
    uint16_t address = 0;
    uint8_t buffer[PAGESIZE];

    for (;;) {
        uint8_t ch = getch();

        if (ch == ':') {
            uint8_t len = get_hex_byte();
            uint16_t addr = (get_hex_byte() << 8) | get_hex_byte();
            uint8_t type = get_hex_byte();

            if (type == 0x00) {  // Data record
                for (uint8_t i = 0; i < len; i++) {
                    if (address < APP_END) {
                        buffer[address % PAGESIZE] = get_hex_byte();
                        address++;

                        if ((address % PAGESIZE) == 0) {
                            flash_erase_page(address - PAGESIZE);
                            flash_write_page(address - PAGESIZE, buffer);
                        }
                    } else {
                        get_hex_byte();  // Discard data
                    }
                }
            } else if (type == 0x01) {  // End of file record
                if (address % PAGESIZE) {
                    flash_erase_page(address - (address % PAGESIZE));
                    flash_write_page(address - (address % PAGESIZE), buffer);
                }
                putch(STK_OK);
                break;
            }

            get_hex_byte();  // Checksum (ignored)
            putch(STK_OK);
        } else if (ch == 'Q') {
            putch(STK_OK);
            break;
        }
    }
}

int main(void) {
    initialize_mcu();
    initialize_uart();
    bootloader();

    // Jump to application
    asm volatile(
        "clr r30\n\t"
        "clr r31\n\t"
        "ijmp\n\t"
    );

    return 0;
}

// Bootloader section attribute
__attribute__((section(".bootloader")))
void bootloader_section(void) {
    // This function is placed in the bootloader section
}

// Set the bootloader start address
BOOTLOADER_SECTION __attribute__((used, section(".vectors"))) void (*boot_start)(void) = (void (*)(void))BOOTLOADER_START_ADDRESS;
