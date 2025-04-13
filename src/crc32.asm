;
; Title:	crc32
; Author:	Leigh Brown
; Created:	26/05/2023
; Last Updated:	13/04/2025 - Jeroen Venema

; Modinfo
;	crc32 can han.d32e different blocks now
;   crc32 is now assembled using gnu-as

	.global	_crc32
	.global	_crc32_initialize
	.global	_crc32_finalize

    .assume adl = 1	
    .text
; UINT32 crc32(const char *s, UINT24 len);
;              IX+6           IX+9
; NB: We use local stack storage without allocating it by decrementing SP.
;     That's okay, /as.d32 as/ we don't call any other functions.

_crc32_initialize:
    ; Initialise output to 0xFFFFFFFF
    EXX
    LD      A, $FF
    LD      (crc32result+3), A
    LD      (crc32result+2), A
    LD      (crc32result+1), A
    LD      (crc32result), A
    EXX

    ; replacement for LD HL, crc32_lookup_table >> 2
    ; which doesn't work in gnu-as with linked code segments, where the symbol address is unknown at assembly
    PUSH    HL
    LD      HL, crc32_lookup_table      ; load address of the crc32_lookup_table
    LD      (shiftedtable), HL          ; save the address, so we can reach the upper byte individually
    LD      A, (shiftedtable+2)         ; get HLU
    SRL     A                           ; start shifting 1st bit
    RR      H
    RR      L
    SRL     A                           ; start shifting 2nd bit
    RR      H
    RR      L
    LD      (shiftedtable), HL
    LD      (shiftedtable+2), A         ; crc32_lookup_table >> 2 is now stored in (shiftedtable) 
    POP     HL
    RET

_crc32_finalize:
	PUSH	IX
	LD		IX,0
	ADD		IX,SP

    LD      A, (crc32result+3)
    CPL
    LD      (crc32result+3), A
    LD      A, (crc32result+2)
    CPL
    LD      (crc32result+2), A
    LD      A, (crc32result+1)
    CPL
    LD      (crc32result+1), A
    LD      A, (crc32result)
    CPL
    LD      (crc32result), A

	LD      A, (crc32result+3)
	LD      DE, 0
	LD      E, A
	LD      HL, (crc32result)
	POP     IX
	RET

_crc32:
	; Function prologue
	PUSH	IX
	LD		IX,0
	ADD		IX,SP

	LD		DE, (IX+6)
	LD		HL, (IX+9)
; expects DE to point to buffer to check
; expects HL to contain the bytecount
    LD		BC,1

    EXX
    LD      A, (crc32result+3)
    LD      D, A
    LD      A, (crc32result+2)
    LD      E, A
    LD      A, (crc32result+1)
    LD      B, A
    LD      A, (crc32result)
    LD      C, A
    EXX

1:
    LD      A,(DE)
    INC     DE
    
    ; Calculate offset into lookup table
    EXX
    XOR     C
    LD      HL, (shiftedtable) ;LD	HL,crc32_lookup_table >> 2
    LD      L,A
    ADD     HL,HL
    ADD     HL,HL

    LD      A,B
    XOR     (HL)
    INC     HL
    LD      C,A

    LD      A,E
    XOR     (HL)
    INC     HL
    LD      B,A

    LD      A,D
    XOR     (HL)
    INC     HL
    LD      E,A

    LD      D,(HL)
    EXX

    ; Decrement count and loop if not zero
    OR      A,A
    SBC     HL,BC
    JR      NZ,1b

    EXX
    LD      A, D
    LD      (crc32result+3), A
    LD      A, E
    LD      (crc32result+2), A
    LD      A, B
    LD      (crc32result+1), A
    LD      A, C
    LD      (crc32result), A
    EXX

	; Function epilogue
	POP     IX
	RET

    .section .rodata
		; The crc32 routine is optimised in such a way as to require
		; the following table to be aligned on a 1024 byte boundary.

		.ALIGN	10
crc32_lookup_table:
		.d32	$00000000, $77073096, $ee0e612c, $990951ba
		.d32	$076dc419, $706af48f, $e963a535, $9e6495a3
		.d32	$0edb8832, $79dcb8a4, $e0d5e91e, $97d2d988
		.d32	$09b64c2b, $7eb17cbd, $e7b82d07, $90bf1d91
		.d32	$1db71064, $6ab020f2, $f3b97148, $84be41de
		.d32	$1adad47d, $6ddde4eb, $f4d4b551, $83d385c7
		.d32	$136c9856, $646ba8c0, $fd62f97a, $8a65c9ec
		.d32	$14015c4f, $63066cd9, $fa0f3d63, $8d080df5
		.d32	$3b6e20c8, $4c69105e, $d56041e4, $a2677172
		.d32	$3c03e4d1, $4b04d447, $d20d85fd, $a50ab56b
		.d32	$35b5a8fa, $42b2986c, $dbbbc9d6, $acbcf940
		.d32	$32d86ce3, $45df5c75, $dcd60dcf, $abd13d59
		.d32	$26d930ac, $51de003a, $c8d75180, $bfd06116
		.d32	$21b4f4b5, $56b3c423, $cfba9599, $b8bda50f
		.d32	$2802b89e, $5f058808, $c60cd9b2, $b10be924
		.d32	$2f6f7c87, $58684c11, $c1611dab, $b6662d3d
		.d32	$76dc4190, $01db7106, $98d220bc, $efd5102a
		.d32	$71b18589, $06b6b51f, $9fbfe4a5, $e8b8d433
		.d32	$7807c9a2, $0f00f934, $9609a88e, $e10e9818
		.d32	$7f6a0dbb, $086d3d2d, $91646c97, $e6635c01
		.d32	$6b6b51f4, $1c6c6162, $856530d8, $f262004e
		.d32	$6c0695ed, $1b01a57b, $8208f4c1, $f50fc457
		.d32	$65b0d9c6, $12b7e950, $8bbeb8ea, $fcb9887c
		.d32	$62dd1ddf, $15da2d49, $8cd37cf3, $fbd44c65
		.d32	$4db26158, $3ab551ce, $a3bc0074, $d4bb30e2
		.d32	$4adfa541, $3dd895d7, $a4d1c46d, $d3d6f4fb
		.d32	$4369e96a, $346ed9fc, $ad678846, $da60b8d0
		.d32	$44042d73, $33031de5, $aa0a4c5f, $dd0d7cc9
		.d32	$5005713c, $270241aa, $be0b1010, $c90c2086
		.d32	$5768b525, $206f85b3, $b966d409, $ce61e49f
		.d32	$5edef90e, $29d9c998, $b0d09822, $c7d7a8b4
		.d32	$59b33d17, $2eb40d81, $b7bd5c3b, $c0ba6cad
		.d32	$edb88320, $9abfb3b6, $03b6e20c, $74b1d29a
		.d32	$ead54739, $9dd277af, $04db2615, $73dc1683
		.d32	$e3630b12, $94643b84, $0d6d6a3e, $7a6a5aa8
		.d32	$e40ecf0b, $9309ff9d, $0a00ae27, $7d079eb1
		.d32	$f00f9344, $8708a3d2, $1e01f268, $6906c2fe
		.d32	$f762575d, $806567cb, $196c3671, $6e6b06e7
		.d32	$fed41b76, $89d32be0, $10da7a5a, $67dd4acc
		.d32	$f9b9df6f, $8ebeeff9, $17b7be43, $60b08ed5
		.d32	$d6d6a3e8, $a1d1937e, $38d8c2c4, $4fdff252
		.d32	$d1bb67f1, $a6bc5767, $3fb506dd, $48b2364b
		.d32	$d80d2bda, $af0a1b4c, $36034af6, $41047a60
		.d32	$df60efc3, $a867df55, $316e8eef, $4669be79
		.d32	$cb61b38c, $bc66831a, $256fd2a0, $5268e236
		.d32	$cc0c7795, $bb0b4703, $220216b9, $5505262f
		.d32	$c5ba3bbe, $b2bd0b28, $2bb45a92, $5cb36a04
		.d32	$c2d7ffa7, $b5d0cf31, $2cd99e8b, $5bdeae1d
		.d32	$9b64c2b0, $ec63f226, $756aa39c, $026d930a
		.d32	$9c0906a9, $eb0e363f, $72076785, $05005713
		.d32	$95bf4a82, $e2b87a14, $7bb12bae, $0cb61b38
		.d32	$92d28e9b, $e5d5be0d, $7cdcefb7, $0bdbdf21
		.d32	$86d3d2d4, $f1d4e242, $68ddb3f8, $1fda836e
		.d32	$81be16cd, $f6b9265b, $6fb077e1, $18b74777
		.d32	$88085ae6, $ff0f6a70, $66063bca, $11010b5c
		.d32	$8f659eff, $f862ae69, $616bffd3, $166ccf45
		.d32	$a00ae278, $d70dd2ee, $4e048354, $3903b3c2
		.d32	$a7672661, $d06016f7, $4969474d, $3e6e77db
		.d32	$aed16a4a, $d9d65adc, $40df0b66, $37d83bf0
		.d32	$a9bcae53, $debb9ec5, $47b2cf7f, $30b5ffe9
		.d32	$bdbdf21c, $cabac28a, $53b39330, $24b4a3a6
		.d32	$bad03605, $cdd70693, $54de5729, $23d967bf
		.d32	$b3667a2e, $c4614ab8, $5d681b02, $2a6f2b94
		.d32	$b40bbe37, $c30c8ea1, $5a05df1b, $2d02ef8d	

    .data
crc32result:
    .d32 0
shiftedtable:
    .d24 0

		END
