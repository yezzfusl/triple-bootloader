#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>

#define BOOTLOADER_START_ADDRESS 0x7000

class Bootloader {
private:
    static constexpr uint32_t F_CPU = 16000000UL;
    static constexpr uint32_t BAUD = 115200;
    static constexpr uint16_t BAUD_PRESCALE = ((F_CPU / 16 / BAUD) - 1);

public:
    static void initializeMCU() {
        // Disable interrupts
        cli();

        // Set up the watchdog timer
        MCUSR &= ~(1 << WDRF);  // Clear the Watchdog Reset Flag
        WDTCSR |= (1 << WDCE) | (1 << WDE);  // Enable Watchdog Timer changes
        WDTCSR = 0x00;  // Disable Watchdog Timer

        // Set up the clock
        // The Arduino Uno uses an external 16 MHz crystal
        // We'll set the clock prescaler to 1 (no prescaling)
        CLKPR = (1 << CLKPCE);  // Enable change of CLKPS bits
        CLKPR = 0;  // Set prescaler to 1 (no prescaling)

        // Set up the stack pointer
        // The stack grows downwards from the end of RAM
        SPH = (RAMEND & 0xFF00) >> 8;
        SPL = RAMEND & 0xFF;

        // Enable interrupts
        sei();
    }

    static void initializeUART() {
        // Set baud rate
        UBRR0H = (BAUD_PRESCALE >> 8);
        UBRR0L = BAUD_PRESCALE;

        // Enable receiver and transmitter
        UCSR0B = (1 << RXEN0) | (1 << TXEN0);

        // Set frame format: 8 data bits, 1 stop bit, no parity
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    }

    static void uartTransmit(uint8_t data) {
        // Wait for empty transmit buffer
        while (!(UCSR0A & (1 << UDRE0)));
        
        // Put data into buffer, sends the data
        UDR0 = data;
    }

    static uint8_t uartReceive() {
        // Wait for data to be received
        while (!(UCSR0A & (1 << RXC0)));
        
        // Get and return received data from buffer
        return UDR0;
    }

    static void run() {
        initializeMCU();
        initializeUART();

        // Main bootloader logic will be implemented here
        while (true) {
            // Echo received characters (for testing)
            uint8_t received = uartReceive();
            uartTransmit(received);
        }
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
