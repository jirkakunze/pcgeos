/***********************************************************************
 *
 *                      Copyright FreeGEOS-Project
 *              Portions Copyright (c) GlobalPC 1999
 *         Portions Copyright 1996 Derek B. Noonburg (xpdf)
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * PROJECT:       FreeGEOS
 * MODULE:        PDF Viewer
 * FILE:          gfxFont.h
 *
 * AUTHOR:        Jirka Kunze: 18.08.2026
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      3/31/99   mevissen  Initial version (GlobalPC).
 *      18.08.26  JK        Relicensed under Apache 2.0, cleanup.
 *
 * DESCRIPTION:
 *      Port of Derek Noonburg's "gfxFont" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef GFXFONT_H
#define GFXFONT_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"
#include "gstring.h"
#include "object.h"

/*
 * Adobe seems to favor substituting this particular character for the "fi"
 * character pair, I suppose because of its kerning attributes.
 */
#define C_FI_LIGATURE   C_CTRL_F
/*
 * Further ligatures (Type0 fonts: ToUnicode or glyph names). Like
 * C_FI_LIGATURE these are no GEOS chars; doShowText draws them as text.
 */
#define C_FF_LIGATURE   C_CTRL_A
#define C_FFI_LIGATURE  C_CTRL_B
#define C_FFL_LIGATURE  C_CTRL_C
#define C_FL_LIGATURE   C_CTRL_D

/* GfxFont */

/* Constructor. */
extern void
GfxFontInit(GfxFont *this, char *tag1, Ref id1, Dict *fontDict, XRef *xref);

/* Destructor. */
extern
void GfxFontFree(GfxFont *this);

/* Map char. */
extern
char GfxFontMapChar(GfxFont *this, unsigned char c, char defaultChar);

/* Find char. */
extern
char GfxFontFindChar(GfxFont *this, unsigned char ch);

/* Handle gfx unicode to geos char. */
word GfxUnicodeToGeosChar(word unicodeVal);

/* Unicode value of a GEOS char (0 if none). */
word GfxGeosCharToUnicode(byte ch);

/* GEOS char (or C_xx_LIGATURE) for a glyph name, 0 if unknown. */
byte GfxGlyphNameToGeosChar(char *name);

/* Pick the GEOS substitute font class (flags) from the font name. */
void GfxFontMapName(GfxFont *this, const char *name);

/* Process gfx font matches. */
extern
GBool GfxFontMatches(GfxFont *this, char *tag1);

/* Process gfx font is16 bit. */
extern
GBool GfxFontIs16Bit(GfxFont *this);

/* GfxFontDict */

/* Build the font dictionary, given the PDF font dictionary. */
extern void
GfxFontDictInit(GfxFontDict *this, Dict *fontDict, XRef *xref);

/* Destructor. */
extern
void GfxFontDictFree(GfxFontDict *this);

/* Look up. */
extern
GfxFont *GfxFontDictLookup(GfxFontDict *this, char *tag);

/* Process gfx font matches. */
extern
GBool GfxFontMatches(GfxFont *this, char *tag1);

/* Process gfx font is16 bit. */
extern
GBool GfxFontIs16Bit(GfxFont *this);

/*
 * Convert a PDF text string (PDFDocEncoding or UTF-16BE with BOM) to a
 * null-terminated string in the GEOS character set. Characters without a
 * GEOS equivalent become '?', control characters a space. Returns the
 * length. Unicode-PM replaces this function.
 */
extern word PdfTextStringToGeos(GooString *str, char *dst, word dstSize);

#endif  /* GFXFONT_H */

