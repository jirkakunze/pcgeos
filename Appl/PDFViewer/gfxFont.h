//========================================================================
//
// GfxFont.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef _GFXFONT_H
#define _GFXFONT_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"
#include "gstring.h"
#include "object.h"

/* This is a special case handled by the drawing code.  Adobe seems to favor
 * substituting this particular character for the "fi" character pair, I
 * suppose because of its kerning attributes.
 */
#define C_FI_LIGATURE   C_CTRL_F

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

  // Constructor.
extern void
  GfxFontInit(GfxFont *this, char *tag1, Ref id1, Dict *fontDict, XRef *xref);

  // Destructor.
extern
void GfxFontFree(GfxFont *this);

extern
char GfxFontMapChar(GfxFont *this, unsigned char c, char defaultChar);

extern
char GfxFontFindChar(GfxFont *this, unsigned char ch);
word GfxUnicodeToGeosChar(word unicodeVal);

extern
  GBool GfxFontMatches(GfxFont *this, char *tag1);
extern
  GBool GfxFontIs16Bit(GfxFont *this);

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------


  // Build the font dictionary, given the PDF font dictionary.
extern void
  GfxFontDictInit(GfxFontDict *this, Dict *fontDict, XRef *xref);

  // Destructor.
extern
void GfxFontDictFree(GfxFontDict *this);

extern
GfxFont *GfxFontDictLookup(GfxFontDict *this, char *tag);

extern
  GBool GfxFontMatches(GfxFont *this, char *tag1);
extern
  GBool GfxFontIs16Bit(GfxFont *this);


#endif	/* _GFXFONT_H */
