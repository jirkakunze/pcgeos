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
 * FILE:          gfxState.h
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
 *      Port of Derek Noonburg's "GfxState" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef GFXSTATE_H
#define GFXSTATE_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

/*
 * GfxColor
 */

extern void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k);

/* Set gray. */
extern void GfxColorSetGray(GfxColor *this, short gray);

/* Set rgb. */
extern void GfxColorSetRGB(GfxColor *this, short r1, short g1, short b1);

/*
 * GfxColorSpace
 */

/*
 * Parse a PDF colorspace object into mode, component count, and lookup table.
 */
extern void GfxColorSpaceInit(GfxColorSpace *this, Obj *colorSpace, XRef *xref);

/* Release storage owned by a colorspace. */
extern void GfxColorSpaceFree(GfxColorSpace *this);

/* Get number pixel comps. */
extern short GfxColorSpaceGetNumPixelComps(GfxColorSpace *this);

/* Convert a raw component tuple to a device color. */
extern void GfxColorSpaceGetColor(GfxColorSpace *this, short x[4],
    GfxColor *color);

/*
 * GfxImageColorMap
 */

/* Build a lookup table mapping image samples to device colors. */
extern void GfxImageColorMapInit(GfxImageColorMap *this, short bits1,
    Obj *decode, GfxColorSpace *colorSpace1, XRef *xref);

/* Release storage owned by an image color map. */
extern void GfxImageColorMapFree(GfxImageColorMap *this);

/* Map get number pixel comps. */
extern short GfxImageColorMapGetNumPixelComps(GfxImageColorMap *this);

/* Map get bits. */
extern short GfxImageColorMapGetBits(GfxImageColorMap *this);

/* Convert a raw image sample to a device color. */
extern void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4],
    GfxColor *color);

/*
 * GfxState
 */

/* Construct a default GfxState bound to a GEOS gstring. */
extern void GfxStateInit(GfxState *state, Handle gstring);

/* Release the saved-state chain owned by a GfxState. */
extern void GfxStateFree(GfxState *state);

/* Copy. */
extern void GfxStateCopy(GfxState *dest, GfxState *state);

/* Push the current state onto its own saved-state stack. */
extern void GfxStateSave(GfxState *state);

/* Pop and restore the most recently saved state. */
extern void GfxStateRestore(GfxState *state);

/* Get font. */
extern GfxFont *GfxStateGetFont(GfxState *state);

/* Get font size. */
extern gdouble GfxStateGetFontSize(GfxState *state);

/* Get text mat. */
extern gdouble *GfxStateGetTextMat(GfxState *state);

/* Get char space. */
extern sdword GfxStateGetCharSpace(GfxState *state);

/* Get word space. */
extern gdouble GfxStateGetWordSpace(GfxState *state);

/* Get horiz scaling. */
extern sdword GfxStateGetHorizScaling(GfxState *state);

/* Get leading. */
extern gdouble GfxStateGetLeading(GfxState *state);

/* Get rise. */
extern sdword GfxStateGetRise(GfxState *state);

/* Get render. */
extern short GfxStateGetRender(GfxState *state);

/* Set font. */
extern void GfxStateSetFont(GfxState *state, GfxFont *font1, gdouble fontSize1);

/* Set text mat. */
extern void GfxStateSetTextMat(GfxState *state, gdouble a, gdouble b, gdouble c,
    gdouble d, gdouble e, gdouble f);

/* Set char space. */
extern void GfxStateSetCharSpace(GfxState *state, gdouble space);

/* Set word space. */
extern void GfxStateSetWordSpace(GfxState *state, gdouble space);

/* Set horiz scaling. */
extern void GfxStateSetHorizScaling(GfxState *state, gdouble scale);

/* Set leading. */
extern void GfxStateSetLeading(GfxState *state, gdouble leading1);

/* Set rise. */
extern void GfxStateSetRise(GfxState *state, gdouble rise1);

/* Set render. */
extern void GfxStateSetRender(GfxState *state, short render1);

/* Move the text line origin to (tx, ty) in text space. */
extern void GfxStateTextMoveTo(GfxState *state, gdouble tx, gdouble ty);

/* Shift the text position horizontally by tx in text space. */
extern void GfxStateTextShift(GfxState *state, gdouble tx);

#endif  /* GFXSTATE_H */

