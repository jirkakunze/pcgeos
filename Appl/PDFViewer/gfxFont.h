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

#endif  /* GFXFONT_H */

