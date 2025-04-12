	.global _getsysvars
    .assume adl = 1	
    .text
	
_getsysvars:
	push	ix
	ld		a, $08 ; mos_sysvars
	rst.lil	$08
	push	ix
	pop		hl
	pop		ix
	ret

end
