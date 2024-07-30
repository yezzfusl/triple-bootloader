#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>

#define BOOTLOADER_START_ADDRESS 0x7000
#define F_CPU 16000000UL
#define BAUD 115200
#define BAUD_PRESCALE ((F_CPU / 16 / BAUD) - 1)

void initialize_mcu(void) {
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

void initialize_uart(void) {
    // Set baud rate
    UBRR0H = (BAUD_PRESCALE >> 8);
    UBRR0L = BAUD_PRESCALE;

    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {
    // Wait for empty transmit buffer
    while (!(UCSR0A & (1 << UDRE0)));
    
    // Put data into buffer, sends the data
    UDR0 = data;
}

unsigned char uart_receive(void) {
    // Wait for data to be received
    while (!(UCSR0A & (1 << RXC0)));
    
    // Get and return received data from buffer
    return UDR0;
}

int main(void) {
    // Initialize the microcontroller
    initialize_mcu();

    // Initialize UART
    initialize_uart();

    // Main bootloader logic will be implemented here
    while (1) {
        // Echo received characters (for testing)
        unsigned char received = uart_receive();
        uart_transmit(received);
    }
    return 0;
}

// Bootloader section attribute
__attribute__((section(".bootloader")))
void bootloader_section(void) {
    // This function is placed in the bootloader section
}

// Set the bootloader start address
BOOTLOADER_SECTION __attribute__((used, section(".vectors"))) void (*boot_start)(void) = (void (*)(void))BOOTLOADER_START_ADDRESS;
