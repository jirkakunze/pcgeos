COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PROJECT:        PDF Viewer
FILE:           jpegerrManager.asm

DESCRIPTION:
        set_error_handler for the IJG JPEG library (ijgjpeg).

        Works like setjmp: saves the caller's registers and stack position
        in a memory block and returns 0. When libjpeg hits a fatal error,
        its error_exit calls jmp_to_error_handler (inside the library),
        which unwinds the resource call frames and returns a second time
        from set_error_handler, now with ax = -1.

        Only si, di, bp, ds, es and sp are restored -- exactly what a
        Watcom __cdecl caller expects to survive a call. Code after the
        error return must not rely on anything else.

        Same routine as Appl/Dump/jpegerr.asm and ImpGraph's ASMIMP.

        C prototype (main/stream.goc):
            extern word set_error_handler(word *context);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@

include stdapp.def
include Internal/heapInt.def

ErrorHandlerContext	struct
	EHC_ss	word
	EHC_ds	word
	EHC_es	word
	EHC_di	word
	EHC_si	word
	EHC_cs	word
	EHC_ip	word
	EHC_sp	word
	EHC_bp	word
	EHC_stackBot	word
ErrorHandlerContext	ends

global _set_error_handler:far

	SetDefaultConvention

; WARNING: THIS ROUTINE MUST BE LOCATED IN A FIXED RESOURCE (see pdfvu.gp)!
; The resource call unwinding in jmp_to_error_handler breaks if a
; ResourceCallInt frame lies between this routine and its caller.

ASM_FIXED  		segment resource

_set_error_handler	proc far	errorContextP:fptr.word
		uses	es, di
		.enter
		mov	ax, size ErrorHandlerContext
		mov	cx, ALLOC_DYNAMIC_LOCK
		call	MemAlloc
		tst	bx
		jz	returnMem
		mov	cx, es		; original ES
		mov	es, ax
		clr	ax
		mov	es:[EHC_ss], ss
		mov	es:[EHC_ds], ds
		mov	es:[EHC_es], cx
		mov	es:[EHC_di], di
		mov	es:[EHC_si], si
		mov	ax, ss:[bp]	; caller's BP
		mov	es:[EHC_bp], ax
		mov	ax, ss:[bp]+2	; return off
		mov	es:[EHC_ip], ax
		mov	ax, ss:[bp]+4	; return seg
		mov	es:[EHC_cs], ax
		mov	ax, bp
		add	ax, 2		; SP to return to caller
		mov	es:[EHC_sp], ax
		mov	ax, ss:[TPD_stackBot]
		mov	es:[EHC_stackBot], ax
		call	MemUnlock
returnMem:
		mov	es, errorContextP.segment
		mov	di, errorContextP.offset
		mov	es:[di], bx
		mov	ax, 0			; error handler set
		.leave
		ret
_set_error_handler	endp

ASM_FIXED                ends

	SetGeosConvention
