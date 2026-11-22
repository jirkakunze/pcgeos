/***********************************************************************
 *
 *                      Copyright FreeGEOS-Project
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * PROJECT:       FreeGEOS
 * MODULE:        PDF Viewer
 * FILE:          cidFont.h
 *
 * DESCRIPTION:
 *      Type0 (composite) fonts with a CIDFont descendant, Identity-H
 *      encoding only. See main/cidFont.c.
 ***********************************************************************/

#ifndef CIDFONT_H
#define CIDFONT_H

#include "obj.h"

/*
 * One CID of the font. Sorted by cid, one entry per CID. A CID without
 * an entry (or without CIDE_HAS_WIDTH) takes its width from the /W
 * ranges or /DW.
 */
#define CIDE_HAS_WIDTH  0x01
#define CIDE_HAS_CHAR   0x02

typedef struct
{
    word cid;
    word width;     /* glyph space units (1/1000 of the font size) */
    byte ch;        /* GEOS char or C_xx_LIGATURE, 0 = not drawable */
    byte flags;     /* CIDE_xxx */
} GfxCidEntry;

/* "cFirst cLast w" form of /W */
typedef struct
{
    word first;
    word last;
    word width;
} GfxCidRange;

/* Everything doShowText needs while the font's tables are locked. */
typedef struct
{
    GfxCidEntry *entries;
    word entryCount;
    GfxCidRange *ranges;
    word rangeCount;
    word defaultWidth;
} GfxCidLock;

/* Called by GfxFontInit for Subtype /Type0. */
extern void GfxCidFontInit(GfxFont *this, Dict *fontDict, XRef *xref);

/* Called by GfxFontFree. */
extern void GfxCidFontFree(GfxFont *this);

/* Lock the font's tables for one string (doShowText). */
extern void GfxCidFontLock(GfxFont *this, GfxCidLock *lock);
extern void GfxCidFontUnlock(GfxFont *this);

/* Width and GEOS char of one CID (tables locked). */
extern void GfxCidFontLookup(GfxCidLock *lock, word cid, word *width,
    byte *ch);

#endif /* CIDFONT_H */
