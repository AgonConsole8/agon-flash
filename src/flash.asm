;
; Title:		Flash interface
; Author:		Jeroen Venema
; Created:		16/12/2022
; Last Updated:	22/04/2025
; 
; Modinfo:
;	14/10/2023: VDP update routine added
;   12/04/2025: Updated for agondev
;   22/04/2025: Saving BC/DE/HL registers required in _startVDPupdate

	.global _enableFlashKeyRegister
	.global _fastmemcpy
	.global _reset
	.global _startVDPupdate

    .assume adl = 1	
    .text

BUFFERSIZE	EQU 1024
buffer		EQU $50000	; memory location

_enableFlashKeyRegister:
	PUSH	IX
	LD		IX, 0
	ADD		IX, SP
	
	; actual work here
	LD		A, $b6	; unlock
	OUT0	($F5), A
	LD		A, $49
	OUT0	($F5), A
	
	LD		SP, IX
	POP		IX
	RET
	
_reset:
	RST	0
	RET ; will never get here
	
_fastmemcpy:
	PUSH    IX
	LD      IX, 0
	ADD     IX, SP

	PUSH    BC
	PUSH    DE
	PUSH    HL
	
	LD		DE, (IX+6)	; destination address
	LD		HL, (IX+9)	; source
	LD		BC, (IX+12)	; number of bytes to write to flash
	LDIR

	POP     HL
	POP     DE
	POP     BC

	LD      SP, IX
	POP     IX
	RET

_startVDPupdate:
	PUSH    IX
	LD      IX, 0
	ADD     IX, SP

    PUSH    BC
    PUSH    DE
    PUSH    HL

	LD	A, (IX+6)
	LD	(filehandle), A
	LD	HL, (IX+9)
	LD	(filesize), HL

sendstartsequence:
    LD  A, 23
    RST.LIL $10
    LD  A, 0
    RST.LIL $10
    LD  A, $A1
    RST.LIL $10
    LD  A, 1
    RST.LIL $10

sendsize:
	LD	HL, (filesize)
    LD   A, L
    RST.LIL $10 ; send LSB
    LD   A, H
    RST.LIL $10 ; send middle byte
    PUSH HL
    INC  SP
    POP  AF
    DEC  SP
    RST.LIL $10 ; send MSB

senddata_start:
    LD   A, 0
    LD   (checksum), A

senddata:
    LD   A, (filehandle)
    LD   C, A
    LD   HL, buffer
    LD   DE, BUFFERSIZE
    LD   A, $1A ; mos_fread
    RST.LIL $08
    LD   A, D
    OR   E        ; 0 bytes read?
    JR   Z, sendchecksum

    LD   HL, buffer
    PUSH DE
    POP  BC          ; length of the bufferstream to bc
    LD   A, 0        ; don't care as bc is set
    PUSH DE
    RST.LIL $18
    POP  DE
; calculate checksum
    LD   HL, buffer
1:
    LD   A, (checksum)
    ADD  A, (HL)
    LD   (checksum), A
    DEC  DE
    INC  HL
    LD   A, D ; check if de == 0
    OR   E
    JR   NZ, 1b ; more bytes to send

    JR   senddata ; next buffer read from disk

sendchecksum:
    LD   A, (checksum)
    NEG ; calculate two's complement
    RST.LIL $10

    PUSH    HL
    PUSH    DE
    PUSH    BC

    LD      A, 0
	LD		SP, IX
	POP		IX
	RET

    .data
checksum:
    .db 0
filehandle:
    .db 0
filesize:
    .d24 0
end
