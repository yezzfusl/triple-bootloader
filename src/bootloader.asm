.include "m328pdef.inc"

; Define constants
.equ BOOTLOADER_START = 0x7000
.equ RAMEND = 0x08FF
.equ PAGESIZE = 128
.equ APP_END = BOOTLOADER_START - 1

; UART constants
.equ BAUD = 115200
.equ F_CPU = 16000000
.equ UBRR_VALUE = F_CPU / 16 / BAUD - 1

; STK500 constants
.equ STK_OK = 0x10
.equ STK_FAILED = 0x11
.equ STK_UNKNOWN = 0x12
.equ STK_INSYNC = 0x14
.equ STK_NOSYNC = 0x15
.equ CRC_EOP = 0x20

; Register usage
.def temp = r16
.def temp2 = r17
.def address_low = r24
.def address_high = r25

; Interrupt vector table
.org 0x0000
    rjmp reset
.org BOOTLOADER_START
reset:
    ; Initialize stack pointer
    ldi temp, high(RAMEND)
    out SPH, temp
    ldi temp, low(RAMEND)
    out SPL, temp

    ; Disable watchdog timer
    in temp, MCUSR
    andi temp, 0xF7  ; Clear WDRF in MCUSR
    out MCUSR, temp
    ldi temp, (1<<WDCE) | (1<<WDE)
    sts WDTCSR, temp
    ldi temp, 0x00
    sts WDTCSR, temp

    ; Set clock prescaler to 1 (no prescaling)
    ldi temp, 0x80
    sts CLKPR, temp  ; Enable change of CLKPS bits
    ldi temp, 0x00
    sts CLKPR, temp  ; Set prescaler to 1 (no prescaling)

    ; Initialize UART
    ldi temp, high(UBRR_VALUE)
    sts UBRR0H, temp
    ldi temp, low(UBRR_VALUE)
    sts UBRR0L, temp
    ldi temp, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, temp
    ldi temp, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, temp

    ; Enable interrupts
    sei

    ; Initialize address
    clr address_low
    clr address_high

main_loop:
    rcall uart_receive
    cpi temp, ':'
    brne check_quit

process_hex:
    rcall get_hex_byte  ; Get length
    mov temp2, temp
    rcall get_hex_byte  ; Get address high
    mov address_high, temp
    rcall get_hex_byte  ; Get address low
    mov address_low, temp
    rcall get_hex_byte  ; Get record type

    cpi temp, 0x00
    breq data_record
    cpi temp, 0x01
    breq eof_record
    rjmp discard_record

data_record:
    ; Process data record
    mov temp, temp2  ; Restore length
data_loop:
    rcall get_hex_byte
    rcall write_flash_byte
    dec temp
    brne data_loop
    rjmp end_record

eof_record:
    ; Process end of file record
    rcall finish_page
    ldi temp, STK_OK
    rcall uart_transmit
    rjmp bootloader_exit

discard_record:
    ; Discard unknown record type
    mov temp, temp2  ; Restore length
discard_loop:
    rcall get_hex_byte
    dec temp
    brne discard_loop

end_record:
    rcall get_hex_byte  ; Discard checksum
    ldi temp, STK_OK
    rcall uart_transmit
    rjmp main_loop

check_quit:
    cpi temp, 'Q'
    brne main_loop
    ldi temp, STK_OK
    rcall uart_transmit

bootloader_exit:
    ; Jump to application
    clr ZH
    clr ZL
    ijmp

uart_receive:
    lds temp, UCSR0A
    sbrs temp, RXC0
    rjmp uart_receive
    lds temp, UDR0
    ret

uart_transmit:
    lds temp2, UCSR0A
    sbrs temp2, UDRE0
    rjmp uart_transmit
    sts UDR0, temp
    ret

get_hex_nibble:
    rcall uart_receive
    cpi temp, '0'
    brlo get_hex_nibble
    cpi temp, '9' + 1
    brlo hex_digit
    cpi temp, 'A'
    brlo get_hex_nibble
    cpi temp, 'F' + 1
    brlo hex_upper
    cpi temp, 'a'
    brlo get_hex_nibble
    cpi temp, 'f' + 1
    brlo hex_lower
    rjmp get_hex_nibble

hex_digit:
    subi temp, '0'
    ret

hex_upper:
    subi temp, 'A' - 10
    ret

hex_lower:
    subi temp, 'a' - 10
    ret

get_hex_byte:
    rcall get_hex_nibble
    swap temp
    mov temp2, temp
    rcall get_hex_nibble
    or temp, temp2
    ret

write_flash_byte:
    ; Check if we need to erase the page
    movw ZL, address_low
    andi ZL, low(~(PAGESIZE - 1))
    andi ZH, high(~(PAGESIZE - 1))
    ldi temp2, (1<<PGERS) | (1<<SPMEN)
    rcall do_spm

    ; Write the byte to the temporary buffer
    movw ZL, address_low
    ldi temp2, (1<<SPMEN)
    rcall do_spm

    ; Increment address
    adiw address_low, 1

    ; Check if we need to write the page
    movw ZL, address_low
    andi ZL, low(PAGESIZE - 1)
    andi ZH, high(PAGESIZE - 1)
    brne write_flash_byte_exit

    ; Write the page
    subi ZL, low(PAGESIZE)
    sbci ZH, high(PAGESIZE)
    ldi temp2, (1<<PGWRT) | (1<<SPMEN)
    rcall do_spm

write_flash_byte_exit:
    ret

finish_page:
    ; Write any remaining data in the temporary buffer
    movw ZL, address_low
    andi ZL, low(~(PAGESIZE - 1))
    andi ZH, high(~(PAGESIZE - 1))
    ldi temp2, (1<<PGWRT) | (1<<SPMEN)
    rcall do_spm
    ret

do_spm:
    ; Save SREG
    in temp, SREG
    push temp

    ; Wait for previous SPM to complete
wait_spm:
    in temp, SPMCSR
    sbrc temp, SPMEN
    rjmp wait_spm

    ; Set up command
    out SPMCSR, temp2

    ; Execute spm
    spm

    ; Wait for SPM to complete
wait_spm2:
    in temp, SPMCSR
    sbrc temp, SPMEN
    rjmp wait_spm2

    ; Restore SREG
    pop temp
    out SREG, temp
    ret

; Bootloader section attribute
.section .bootloader
bootloader_section:
    ret

; Set the bootloader start address
.section .vectors
.org (BOOTLOADER_START - 2)
    .word BOOTLOADER_START
