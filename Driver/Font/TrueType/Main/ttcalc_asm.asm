COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	Copyright FreeGEOS-Project

PROJECT:	PC/GEOS
MODULE:		TrueType Font Driver
FILE:		ttcalc_asm.asm

AUTHOR:		Jirka Kunze

ROUTINES:
	Name			Description
	----			-----------
	

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	jk	9/01/24		Initial revision


DESCRIPTION:
	This file contains the PC/GEOS implementations of the C routines.

NOTES:

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@

ttcalc_TEXT	segment	public	'CODE'

.model	medium, c

COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		Neg64
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

SYNOPSIS:	Calculates the two's complement (negation) of a 64-bit integer.
                It handles the special case where the input is the smallest 
                possible 64-bit signed integer, which remains unchanged when 
                negated due to two's complement representation.

CALLED BY:	
PASS:		TT_Int64        *x: ptr to 64-bit integer

RETURN:		nothing

DESTROYED:	
 
PSEUDO CODE/STRATEGY:


REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	jk	01.09.24	Initial version

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@
COMMENT @%%
.386
Neg64           proc    near    x:fptr
        uses    di
        .enter

        lds	si, x                   ;ds:di = x

        neg     dword ptr [si]          ;low = -low
        adc     dword ptr [si + 4], 0   ;add carry to the upper 32 bits
        neg     dword ptr [si + 4]      ;high = -high

        ;handle special case where input was -0x80000000 (0x8000000000000000)
        jno     done

        ;if overflow, set result to 0x7FFFFFFF (the maximum 32-bit signed integer)
        mov     dword ptr [si], 0FFFFFFFFh
        mov     dword ptr [si + 4], 7FFFFFFFh
done:
        .leave
        ret
Neg64 ENDP
%%@

COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		Add64
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

SYNOPSIS:	This function performs a 64-bit addition of two 64-bit integers
                pointed to by the pointers `x` and `y`, and stores the result
                in the location pointed to by `z`.

CALLED BY:	
PASS:		TT_Int64        *x: ptr to 64-bit integer
                TT_Int64        *y: ptr to 64-bit integer
                TT_Int64        *z: ptr to 64-bit integer

RETURN:		result in *z

DESTROYED:
 
REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	jk	01.09.24	Initial version

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@
global	ADD64:far
Add64	equ	ADD64
ADD64   proc    far     x:fptr, y:fptr, z:fptr
        uses ds, si, es, di, eax, ebx
        .enter

        lds     si, x           ;ds:si = x
        les     di, y           ;es:di = y

        mov     eax, [si]       ;eax = x->lo
        add     eax, [di]       ;add = y->lo

        mov     ebx, [si + 4]   ;eax = x->hi
        adc     ebx, [di + 4]   ;add y->hi with carry

        lds     si, z           ;ds:si = z
        mov     [si], eax       ;store result z->lo
        mov     [si + 4], ebx   ;store result z->hi

        .leave
        ret
ADD64 ENDP


COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		Sub64
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

SYNOPSIS:	This function performs a 64-bit subtraction of two 64-bit integers.
                It subtracts the 64-bit integer pointed to by `y` from the 64-bit 
                integer pointed to by `x` and stores the result in the memory 
                location pointed to by `z`.

CALLED BY:	
PASS:		TT_Int64        *x: ptr to 64-bit integer
                TT_Int64        *y: ptr to 64-bit integer
                TT_Int64        *z: ptr to 64-bit integer

RETURN:		result in *z

DESTROYED:

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	jk	01.09.24	Initial version

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@
global  SUB64: far
Sub64   equ     SUB64
SUB64   proc    far     x:fptr, y:fptr, z:fptr
        uses ds, si, es, di, eax, ebx
        .enter

        lds     si, x           ;ds:si = x
        les     di, y           ;es:di = y

        mov     eax, [si]       ;eax x->lo
        mov     ebx, [si + 4]   ;edx x->hi

        sub     eax, [di]       ;subtract y->lo from x->lo
        sbb     ebx, [di + 4]   ;subtract y->hi from x->hi with borrow

        lds     si, z           ;ds:si = z
        mov     [si], eax       ;store result z->lo
        mov     [si + 4], ebx   ;store result z->hi

        .leave
        ret
SUB64   ENDP

ttcalc_TEXT	ends

