/*
 * Basic Arduino Bootloader 
 * 
 * Author: Fayssal Chokri
 * Team: ElectroMath-Hub 
 * Last modified: 2024-08-31
 * 
 * This bootloader supports:
 * - Intel HEX file format
 * - Watchdog timer for safety
 * - EEPROM reading/writing
 * - Custom commands for device info
 * - Basic error logging
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

// Bootloader configuration
#define BOOTLOADER_START_ADDRESS 0x7000
#define F_CPU 16000000UL
#define BAUD 115200
#define BAUD_PRESCALE ((F_CPU / 16 / BAUD) - 1)

#define PAGESIZE SPM_PAGESIZE
#define APP_END (BOOTLOADER_START_ADDRESS - 1)

// Version info
#define BOOTLOADER_VERSION_MAJOR 1
#define BOOTLOADER_VERSION_MINOR 2

// STK500 constants
#define STK_OK              0x10
#define STK_FAILED          0x11
#define STK_UNKNOWN         0x12
#define STK_INSYNC          0x14
#define STK_NOSYNC          0x15
#define CRC_EOP             0x20

// Custom commands
#define CMD_READ_VERSION    0x41
#define CMD_READ_FUSES      0x42
#define CMD_WRITE_EEPROM    0x43
#define CMD_READ_EEPROM     0x44

// Error codes
#define ERR_NONE            0
#define ERR_VERIFY          1
#define ERR_CHECKSUM        2
#define ERR_INVALID_RECORD  3
#define ERR_WRITE_FAILED    4
#define ERR_EEPROM_WRITE    5

// Timeout in milliseconds
#define BOOTLOADER_TIMEOUT 10000

// Error log in EEPROM
#define ERROR_LOG_ADDR 0x00
#define ERROR_LOG_SIZE 16

// Function prototypes
void initialize_mcu(void);
void initialize_uart(void);
void uart_transmit(uint8_t data);
uint8_t uart_receive(void);
void flash_erase_page(uint32_t page);
void flash_write_page(uint32_t page, uint8_t *buf);
uint8_t flash_read_byte(uint32_t addr);
uint8_t getch(void);
void putch(uint8_t ch);
uint8_t get_hex_nibble(void);
uint8_t get_hex_byte(void);
void send_debug_message(const char* message);
uint8_t bootloader(void);
void handle_custom_command(uint8_t cmd);
void log_error(uint8_t error_code);

// Global variables
volatile uint8_t watchdog_triggered = 0;

// Watchdog interrupt handler
ISR(WDT_vect) {
    watchdog_triggered = 1;
}

void initialize_mcu(void) {
    cli();
    
    // Disable watchdog
    MCUSR &= ~(1 << WDRF);
    wdt_disable();
    
    // Set clock prescaler to 1 (full speed)
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;
    
    // Initialize stack pointer
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

void uart_transmit(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

uint8_t uart_receive(void) {
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

uint8_t flash_read_byte(uint32_t addr) {
    return pgm_read_byte(addr);
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

void send_debug_message(const char* message) {
    while (*message) {
        putch(*message++);
    }
    putch('\r');
    putch('\n');
}

void handle_custom_command(uint8_t cmd) {
    switch (cmd) {
        case CMD_READ_VERSION:
            putch(BOOTLOADER_VERSION_MAJOR);
            putch(BOOTLOADER_VERSION_MINOR);
            break;
        case CMD_READ_FUSES:
            putch(boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS));
            putch(boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS));
            putch(boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS));
            putch(boot_lock_fuse_bits_get(GET_LOCK_BITS));
            break;
        case CMD_WRITE_EEPROM:
            {
                uint16_t addr = (getch() << 8) | getch();
                uint8_t data = getch();
                eeprom_write_byte((uint8_t*)addr, data);
                putch(STK_OK);
            }
            break;
        case CMD_READ_EEPROM:
            {
                uint16_t addr = (getch() << 8) | getch();
                uint8_t data = eeprom_read_byte((uint8_t*)addr);
                putch(data);
            }
            break;
        default:
            putch(STK_UNKNOWN);
    }
}

void log_error(uint8_t error_code) {
    uint8_t log[ERROR_LOG_SIZE];
    eeprom_read_block(log, (void*)ERROR_LOG_ADDR, ERROR_LOG_SIZE);
    
    // Shift log entries
    memmove(log + 1, log, ERROR_LOG_SIZE - 1);
    log[0] = error_code;
    
    eeprom_write_block(log, (void*)ERROR_LOG_ADDR, ERROR_LOG_SIZE);
}

uint8_t bootloader(void) {
    uint16_t address = 0;
    uint8_t buffer[PAGESIZE];
    uint8_t checksum = 0;
    uint8_t error = ERR_NONE;
    uint32_t timeout = BOOTLOADER_TIMEOUT;

    send_debug_message("Bootloader v1.2 started");

    // Enable watchdog timer (1s timeout)
    wdt_enable(WDTO_1S);

    while (timeout > 0 && !watchdog_triggered) {
        wdt_reset();

        if (UCSR0A & (1 << RXC0)) {
            uint8_t ch = getch();

            if (ch == ':') {
                uint8_t len = get_hex_byte();
                uint16_t addr = (get_hex_byte() << 8) | get_hex_byte();
                uint8_t type = get_hex_byte();

                checksum = len + (addr >> 8) + (addr & 0xFF) + type;

                if (type == 0x00) {  // Data record
                    send_debug_message("Processing data record");
                    for (uint8_t i = 0; i < len; i++) {
                        if (address < APP_END) {
                            uint8_t data = get_hex_byte();
                            checksum += data;
                            buffer[address % PAGESIZE] = data;
                            address++;

                            if ((address % PAGESIZE) == 0) {
                                flash_erase_page(address - PAGESIZE);
                                flash_write_page(address - PAGESIZE, buffer);
                                
                                // Verify written data
                                for (uint16_t j = 0; j < PAGESIZE; j++) {
                                    if (flash_read_byte(address - PAGESIZE + j) != buffer[j]) {
                                        error = ERR_VERIFY;
                                        send_debug_message("Verification failed");
                                        break;
                                    }
                                }
                                if (error != ERR_NONE) break;
                            }
                        } else {
                            get_hex_byte();  // Discard data
                        }
                    }
                } else if (type == 0x01) {  // End of file record
                    send_debug_message("Processing end of file record");
                    if (address % PAGESIZE) {
                        flash_erase_page(address - (address % PAGESIZE));
                        flash_write_page(address - (address % PAGESIZE), buffer);
                        
                        // Verify written data
                        for (uint16_t j = 0; j < (address % PAGESIZE); j++) {
                            if (flash_read_byte(address - (address % PAGESIZE) + j) != buffer[j]) {
                                error = ERR_VERIFY;
                                send_debug_message("Verification failed");
                                break;
                            }
                        }
                    }
                    break;
                } else {
                    error = ERR_INVALID_RECORD;
                    send_debug_message("Invalid record type");
                    break;
                }

                uint8_t received_checksum = get_hex_byte();
                if (received_checksum != (uint8_t)(~checksum + 1)) {
                    error = ERR_CHECKSUM;
                    send_debug_message("Checksum error");
                    break;
                }

                putch(STK_OK);
            } else if (ch == 'Q') {
                send_debug_message("Quit command received");
                putch(STK_OK);
                break;
            } else if (ch >= 0x40 && ch <= 0x4F) {
                handle_custom_command(ch);
            }

            timeout = BOOTLOADER_TIMEOUT;
        } else {
            _delay_ms(1);
            timeout--;
        }
    }

    if (watchdog_triggered) {
        send_debug_message("Watchdog timeout");
        error = ERR_WRITE_FAILED;
    } else if (timeout == 0) {
        send_debug_message("Bootloader timed out");
    }

    // Disable watchdog timer
    wdt_disable();

    if (error != ERR_NONE) {
        log_error(error);
    }

    return error;
}

int main(void) {
    initialize_mcu();
    initialize_uart();
    
    uint8_t error = bootloader();

    if (error == ERR_NONE) {
        send_debug_message("Programming successful");
    } else {
        send_debug_message("Programming failed");
    }

    // Jump to application
    if (error == ERR_NONE) {
        asm volatile(
            "clr r30\n\t"
            "clr r31\n\t"
            "ijmp\n\t"
        );
    }

    // If there was an error, stay in bootloader
    while (1) {
        _delay_ms(1000);
        send_debug_message("Bootloader idle due to error");
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
