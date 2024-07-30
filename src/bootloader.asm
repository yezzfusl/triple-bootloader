.include "m328pdef.inc"

; Define bootloader start address
.equ BOOTLOADER_START = 0x7000

; Define some constants
.equ RAMEND = 0x08FF
.equ WDTCSR = 0x60
.equ MCUSR = 0x54
.equ CLKPR = 0x61

; UART constants
.equ BAUD = 115200
.equ F_CPU = 16000000
.equ UBRR_VALUE = F_CPU / 16 / BAUD - 1

; Interrupt vector table
.org 0x0000
    rjmp reset
.org BOOTLOADER_START
reset:
    ; Initialize stack pointer
    ldi r16, high(RAMEND)
    out SPH, r16
    ldi r16, low(RAMEND)
    out SPL, r16

    ; Disable watchdog timer
    in r16, MCUSR
    andi r16, 0xF7  ; Clear WDRF in MCUSR
    out MCUSR, r16
    ldi r16, (1<<WDCE) | (1<<WDE)
    sts WDTCSR, r16
    ldi r16, 0x00
    sts WDTCSR, r16

    ; Set clock prescaler to 1 (no prescaling)
    ldi r16, 0x80
    sts CLKPR, r16  ; Enable change of CLKPS bits
    ldi r16, 0x00
    sts CLKPR, r16  ; Set prescaler to 1 (no prescaling)

    ; Clear registers
    eor r1, r1
    out SREG, r1
    ldi r28, low(RAMEND - 1)
    ldi r29, high(RAMEND - 1)
    out SPH, r29
    out SPL, r28
    
    ; Enable interrupts
    sei

    ; Initialize UART
    rcall uart_init

main_loop:
    ; Main bootloader logic will be implemented here
    rcall uart_receive
    rcall uart_transmit  ; Echo received character
    rjmp main_loop

uart_init:
    ; Set baud rate
    ldi r16, high(UBRR_VALUE)
    sts UBRR0H, r16
    ldi r16, low(UBRR_VALUE)
    sts UBRR0L, r16

    ; Enable receiver and transmitter
    ldi r16, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, r16

    ; Set frame format: 8 data bits, 1 stop bit, no parity
    ldi r16, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, r16

    ret

uart_transmit:
    ; Wait for empty transmit buffer
    lds r16, UCSR0A
    sbrs r16, UDRE0
    rjmp uart_transmit

    ; Put data (r24) into buffer, sends the data
    sts UDR0, r24
    ret

uart_receive:
    ; Wait for data to be received
    lds r16, UCSR0A
    sbrs r16, RXC0
    rjmp uart_receive

    ; Get and return received data from buffer
    lds r24, UDR0
    ret

; Bootloader section attribute
.section .bootloader
bootloader_section:
    ; This section is placed in the bootloader section
    ret

; Set the bootloader start address
.section .vectors
.org (BOOTLOADER_START - 2)
    .word BOOTLOADER_START
