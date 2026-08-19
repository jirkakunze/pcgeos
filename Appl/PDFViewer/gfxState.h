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


//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k);
void GfxColorSetGray(GfxColor *this, short gray);
void GfxColorSetRGB(GfxColor *this, short r1, short g1, short b1);



//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

void GfxColorSpaceInit(GfxColorSpace *this, Obj *colorSpace, XRef *xref);
void GfxColorSpaceFree(GfxColorSpace *this);

short GfxColorSpaceGetNumPixelComps(GfxColorSpace *this);
void GfxColorSpaceGetColor(GfxColorSpace *this, short x[4], GfxColor *color);


//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

void GfxImageColorMapInit(GfxImageColorMap *this, short bits1, Obj *decode,
				   GfxColorSpace *colorSpace1, XRef *xref);
void GfxImageColorMapFree(GfxImageColorMap *this);

short GfxImageColorMapGetNumPixelComps(GfxImageColorMap *this);
short GfxImageColorMapGetBits(GfxImageColorMap *this);
void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4], GfxColor *color);



//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------


  // Construct a default GfxState, for a device with resolution <dpi>,
  // page box (<x1>,<y1>)-(<x2>,<y2>), page rotation <rotate>, and
  // coordinate system specified by <upsideDown>.
//  GfxState(int dpi, double px1a, double py1a, double px2a, double py2a,
//	   int rotate, GBool upsideDown);

extern
void GfxStateInit(GfxState *state, Handle gstring);
extern
void GfxStateFree(GfxState *state);

  // Copy.
extern
void GfxStateCopy(GfxState *dest, GfxState *state);
extern
void GfxStateSave(GfxState *state);
extern
void GfxStateRestore(GfxState *state);



  // Accessors.

extern
  GfxFont *GfxStateGetFont(GfxState *state);
extern
  gdouble GfxStateGetFontSize(GfxState *state);
extern
  gdouble *GfxStateGetTextMat(GfxState *state);
extern
  sdword GfxStateGetCharSpace(GfxState *state);
extern
  gdouble GfxStateGetWordSpace(GfxState *state);
extern
  sdword GfxStateGetHorizScaling(GfxState *state);
extern
  gdouble GfxStateGetLeading(GfxState *state);
extern
  sdword GfxStateGetRise(GfxState *state);
extern
  short GfxStateGetRender(GfxState *state);


  // Change state parameters.
extern
void GfxStateSave(GfxState *state);
extern
void GfxStateRestore(GfxState *state);
extern
  void GfxStateSetFont(GfxState *state, GfxFont *font1, gdouble fontSize1);
extern
  void GfxStateSetTextMat(GfxState *state, gdouble a, gdouble b, gdouble c,
		  gdouble d, gdouble e, gdouble f);
extern
  void GfxStateSetCharSpace(GfxState *state, gdouble space);
extern
  void GfxStateSetWordSpace(GfxState *state, gdouble space);
extern
  void GfxStateSetHorizScaling(GfxState *state, gdouble scale);
extern
  void GfxStateSetLeading(GfxState *state, gdouble leading1);
extern
  void GfxStateSetRise(GfxState *state, gdouble rise1);
extern
  void GfxStateSetRender(GfxState *state, short render1);

  // Text position.
extern
  void GfxStateTextMoveTo(GfxState *state, gdouble tx, gdouble ty);
extern
void GfxStateTextShift(GfxState *state, gdouble tx);


//  GBool hasSaves() { return saved != NULL; }


#endif	/* gfxstate_h */
