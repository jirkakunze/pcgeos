COMMENT @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	Copyright (c) Berkeley Softworks 1990 -- All Rights Reserved

PROJECT:	PC GEOS
MODULE:		Epson 24-pin driver
FILE:		epshi24lbpHInfo.asm

AUTHOR:		Tsutomu Owa

REVISION HISTORY:
	Name	Date		Description
	----	----		-----------
	Owa	1/94		Initial revision

DESCRIPTION:
	This file contains the device information for the Toshiba LBP printer

	Other Printers Supported by this resource:
		Toshiba LBP H

	$Id: epshi24lbpHInfo.asm,v 1.1 97/04/18 11:54:12 newdeal Exp $

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@

;----------------------------------------------------------------------------
;	Toshiba LBP printer
;----------------------------------------------------------------------------

lbpHInfo	segment	resource

	; info blocks

PrinterInfo		   <		; ---- PrinterType -------------
				< PT_RASTER,
				BMF_MONO >,
					; ---- PrinterConnections ------
				< IC_NO_IEEE488,
				CC_NO_CUSTOM,
				SC_NO_SCSI,
				RC_NO_RS232C,
				CC_CENTRONICS,
				FC_FILE,
				AC_NO_APPLETALK >,
					; ---- PrinterSmarts -----------
				PS_DUMB_RASTER,	
					;-------Custom Entry Routine-------
				NULL,
					;-------Custom Exit Routine-------
				NULL,		
					; ---- Mode Info Offsets -------
				offset lbpHlowRes,
				offset lbpHmedRes,
				offset lbpHhiRes,
                                offset printerFontInfo:lq800draft,
                                offset printerFontInfo:lq800nlq,
                                        ; ---- Font Geometry -----------
                                offset lbpHfontGeometries,
                                        ; ---- Symbol Set list -----------
                                NULL,
					; ---- PaperMargins ------------
				< 12,	; Tractor Margins
				12,	;
				12,	;
				32>,	;
				< 12,	; ASF Margins, 4mm
				12,	; 4mm
				12,	; *
				32>,	; * + 5mm
					; ---- PaperInputOptions -------
				< MF_MANUAL1,
				TF_NO_TRACTOR,
				ASF_TRAY2 >,
					; ---- PaperOutputOptions ------
				< OC_COPIES,
				PS_REVERSE,
				OD_SIMPLEX,
				SO_NO_STAPLER,
				OS_NO_SORTER,
				OB_NO_OUTPUTBIN >,
					;
				842,	; paper width (points).297mm	
                                NULL,   ; Main UI
				ASF1BinOnlyOptionsDialogBox, ; Options UI
				PrintEvalASF1BinOnly    ; eval routine address
			      >

;----------------------------------------------------------------------------
;	Graphics modes info
;----------------------------------------------------------------------------

lbpHlowRes	GraphicsProperties < LO_RES_X_RES,	; xres
				     LO_RES_Y_RES,	; yres
				     LO_RES_BAND_HEIGHT,  ; band height
                                     LO_RES_BUFF_HEIGHT,  ; buffer height
                                     LO_RES_INTERLEAVE_FACTOR, ;#interleaves
				     BMF_MONO,		;color format
				     NULL >		; color format

lbpHmedRes	GraphicsProperties < MED_RES_X_RES,	; xres
				     MED_RES_Y_RES,	; yres
				     MED_RES_BAND_HEIGHT,  ; band height
                                     MED_RES_BUFF_HEIGHT,  ; buffer height
                                     MED_RES_INTERLEAVE_FACTOR, ;#interleaves
				     BMF_MONO,		;color format
				     NULL >		; color format

lbpHhiRes	GraphicsProperties < HI_RES_X_RES,	; xres
				     HI_RES_Y_RES,	; yres
				     HI_RES_BAND_HEIGHT,  ; band height
                                     HI_RES_BUFF_HEIGHT,  ; buffer height
                                     HI_RES_INTERLEAVE_FACTOR, ;#interleaves
				     BMF_MONO,		;color format
				     NULL >		; color format

;----------------------------------------------------------------------------
;	Text modes info
;----------------------------------------------------------------------------

                ;need to add geometries in ascending pointsize, grouped by font
lbpHfontGeometries     FontGeometry \
                <       FID_DTC_URW_ROMAN,
                        8,
                        offset lbpH_8ptpitchTab >,
                <       FID_DTC_URW_ROMAN,
                        12,
                        offset lbpH_12ptpitchTab >,
                <       FID_DTC_URW_SANS,
                        8,
                        offset lbpH_8ptpitchTab >,
                <       FID_DTC_URW_SANS,
                        12,
                        offset lbpH_12ptpitchTab >
                word    FID_INVALID             ;table terminator

lbpH_8ptpitchTab      label   byte
        byte    TP_15_PITCH
        byte    TP_PROPORTIONAL         ;"table Terminator"

lbpH_12ptpitchTab      label   byte
        byte    TP_20_PITCH
        byte    TP_17_PITCH
        byte    TP_12_PITCH
        byte    TP_10_PITCH
        byte    TP_6_PITCH
        byte    TP_5_PITCH
        byte    TP_PROPORTIONAL         ;"table Terminator"


lbpHInfo	ends
