#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>

class Bootloader {
private:
    static constexpr uint32_t F_CPU = 16000000UL;
    static constexpr uint32_t BAUD = 115200;
    static constexpr uint16_t BAUD_PRESCALE = ((F_CPU / 16 / BAUD) - 1);
    static constexpr uint16_t BOOTLOADER_START_ADDRESS = 0x7000;
    static constexpr uint16_t PAGESIZE = SPM_PAGESIZE;
    static constexpr uint16_t APP_END = (BOOTLOADER_START_ADDRESS - 1);

    // STK500 constants
    static constexpr uint8_t STK_OK = 0x10;
    static constexpr uint8_t STK_FAILED = 0x11;
    static constexpr uint8_t STK_UNKNOWN = 0x12;
    static constexpr uint8_t STK_INSYNC = 0x14;
    static constexpr uint8_t STK_NOSYNC = 0x15;
    static constexpr uint8_t CRC_EOP = 0x20;

    static void initializeMCU() {
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

    static void initializeUART() {
        UBRR0H = (BAUD_PRESCALE >> 8);
        UBRR0L = BAUD_PRESCALE;
        UCSR0B = (1 << RXEN0) | (1 << TXEN0);
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    }

    static void uartTransmit(uint8_t data) {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = data;
    }

    static uint8_t uartReceive() {
        while (!(UCSR0A & (1 << RXC0)));
        return UDR0;
    }

    static void flashErasePage(uint32_t page) {
        boot_page_erase(page);
        boot_spm_busy_wait();
    }

    static void flashWritePage(uint32_t page, uint8_t *buf) {
        for (uint16_t i = 0; i < SPM_PAGESIZE; i += 2) {
            uint16_t w = *buf++;
            w += (*buf++) << 8;
            boot_page_fill(page + i, w);
        }
        boot_page_write(page);
        boot_spm_busy_wait();
    }

    static uint8_t getch() {
        return uartReceive();
    }

    static void putch(uint8_t ch) {
        uartTransmit(ch);
    }

    static uint8_t getHexNibble() {
        uint8_t ch = getch();
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return 0;
    }

    static uint8_t getHexByte() {
        return (getHexNibble() << 4) | getHexNibble();
    }

public:
    static void run() {
        initializeMCU();
        initializeUART();

        uint16_t address = 0;
        uint8_t buffer[PAGESIZE];

        for (;;) {
            uint8_t ch = getch();

            if (ch == ':') {
                uint8_t len = getHexByte();
                uint16_t addr = (getHexByte() << 8) | getHexByte();
                uint8_t type = getHexByte();

                if (type == 0x00) {  // Data record
                    for (uint8_t i = 0; i < len; i++) {
                        if (address < APP_END) {
                            buffer[address % PAGESIZE] = getHexByte();
                            address++;

                            if ((address % PAGESIZE) == 0) {
                                flashErasePage(address - PAGESIZE);
                                flashWritePage(address - PAGESIZE, buffer);
                            }
                        } else {
                            getHexByte();  // Discard data
                        }
                    }
                } else if (type == 0x01) {  // End of file record
                    if (address % PAGESIZE) {
                        flashErasePage(address - (address % PAGESIZE));
                        flashWritePage(address - (address % PAGESIZE), buffer);
                    }
                    putch(STK_OK);
                    break;
                }

                getHexByte();  // Checksum (ignored)
                putch(STK_OK);
            } else if (ch == 'Q') {
                putch(STK_OK);
                break;
            }
        }

        // Jump to application
        asm volatile(
            "clr r30\n\t"
            "clr r31\n\t"
            "ijmp\n\t"
        );
    }
};

int main() {
    Bootloader::run();
    return 0;
}

// Bootloader section attribute
__attribute__((section(".bootloader")))
void bootloader_section() {
    // This function is placed in the bootloader section
}

// Set the bootloader start address
BOOTLOADER_SECTION __attribute__((used, section(".vectors"))) void (*boot_start)(void) = (void (*)(void))BOOTLOADER_START_ADDRESS;
