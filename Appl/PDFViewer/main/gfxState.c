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
 * FILE:          gfxState.c
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
 *      Port of Derek Noonburg's "GfxState.cc" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif


#include <stddef.h>
#include <geos.h>
#include <math.h>
#include <graphics.h>	
#include <Ansi/string.h>
#include "gmem.h"
#include "obj.h"
#include "array.h"
#include "dict.h"
#include "stream.h"
#include "gfxState.h"
#include "gstr.h"
#include <ec.h>


/*
 * Forward decls
 */

static void GfxColorSpaceSetMode(GfxColorSpace *this, Obj *colorSpace, XRef *xref);
static GBool GfxParseType2Function(Obj *funcObj, XRef *xref,
				    gdouble c0[4], gdouble c1[4], gdouble *n);



//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k) {
  if ((this->r = 255 - (c + k)) < 0)
    this->r = 0;
  if ((this->g = 255 - (m + k)) < 0)
    this->g = 0;
  if ((this->b = 255 - (y + k)) < 0)
    this->b = 0;
}

  // Set color.
void GfxColorSetGray(GfxColor *this, short gray)
    { this->r = this->g = this->b = gray; }
void GfxColorSetRGB(GfxColor *this, short r1, short g1, short b1)
    { this->r = r1; this->g = g1; this->b = b1; }


//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

/***********************************************************************
 *		GfxParseType2Function
 ***********************************************************************
 * SYNOPSIS:	    Parse a PDF Function dictionary as a Type 2
 *		    (Exponential Interpolation) function: out[j] =
 *		    C0[j] + x^N * (C1[j] - C0[j]). This is the only
 *		    Function type this port evaluates -- it's also
 *		    the standard, near-universal choice for simple
 *		    spot-color tint transforms, which only ever have
 *		    one input value (the tint itself), exactly what
 *		    Type 2 is specified for.
 * PARAMETERS:	    Obj *funcObj (dict or stream -- Type 2 functions are
 *		    normally plain dicts, but nothing stops a
 *		    conforming writer from wrapping one in a stream),
 *		    XRef *xref, gdouble c0[4]/c1[4] (out), gdouble *n (out)
 * RETURNS:	    gTrue if funcObj is a well-formed Type 2 function
 *		    (c0/c1/n filled in, using the spec's defaults for
 *		    any of /C0, /C1, /N that are missing); gFalse for
 *		    any other Function type or a malformed dict, in
 *		    which case the caller falls back to the
 *		    tintApprox approximation.
 *
 ***********************************************************************/
static void
GfxParseCoordArray(Dict *dict, const char *key, XRef *xref, gdouble coords[4])
{
    Obj arrObj, elemObj;
    word i, len;

    DictLookup(dict, key, &arrObj, xref);
    if (isArray(&arrObj)) {
        len = ObjArrayGetLength(&arrObj);
        if (len > 4)
            len = 4;

        for (i = 0; i < len; ++i) {
            ObjArrayGet(&arrObj, i, &elemObj, xref);
            if (isNum(&elemObj))
                coords[i] = getNum(&elemObj);

            ObjFree(&elemObj);
        }
    }
    ObjFree(&arrObj);
}

static GBool
GfxParseType2Function(Obj *funcObj, XRef *xref, gdouble c0[4], gdouble c1[4], gdouble *n)
{
    Obj typeObj, nObj;
    Dict *dict;
    word i;

    if (isDict(funcObj)) {
        dict = getDict(funcObj);
    } else if (isStream(funcObj)) {
        dict = ObjStreamGetDict(funcObj);
    } else {
        return gFalse;
    }

    DictLookup(dict, "FunctionType", &typeObj, xref);
    if (!isInt(&typeObj) || getInt(&typeObj) != 2) {
        ObjFree(&typeObj);
        return gFalse;
    }
    ObjFree(&typeObj);

    /* spec defaults: C0 = [0.0], C1 = [1.0], N required but default
     * to 1 (linear) defensively if a writer omits it anyway */
    for (i = 0; i < 4; ++i) {
        c0[i] = 0.0;
        c1[i] = 1.0;
    }
    *n = 1.0;

    GfxParseCoordArray(dict, "C0", xref, c0);
    GfxParseCoordArray(dict, "C1", xref, c1);

    DictLookup(dict, "N", &nObj, xref);
    if (isNum(&nObj)) {
        *n = getNum(&nObj);
    }
    ObjFree(&nObj);

    return gTrue;
}

void GfxColorSpaceInit(GfxColorSpace *this, Obj *colorSpace, XRef *xref) {
  Obj csObj;
  Obj obj, obj2;
  char *s;
  long x;
  short i, j;
  word tintComps;
  Obj altObj, funcObj;
  GfxColorSpace altCs;

  this->ok = gTrue;
  this->lookup = NULL;
  this->tintApprox = gFalse;
  this->hasFunction = gFalse;
  tintComps = 0;
  // check for Separation colorspace
  ObjCopy(&csObj, colorSpace);
  if (isArray(colorSpace)) {
    ObjArrayGet(colorSpace, 0, &obj, xref);
    if (isNameSame(&obj, "Separation") || isNameSame(&obj, "DeviceN")) {
      if (isNameSame(&obj, "Separation")) {
	tintComps = 1;
      } else {
	ObjArrayGet(colorSpace, 1, &obj2, xref);
	tintComps = isArray(&obj2) ? ObjArrayGetLength(&obj2) : 1;
	ObjFree(&obj2);
      }
      if (tintComps < 1 || tintComps > 4) {
	/* more colorants than our fixed 4-slot color arrays can hold */
	ObjFree(&obj);
	goto err1;
      }

      /*
       * Try the real tint-transform Function -- only meaningful for
       * a single input value, which covers every Separation and the
       * (fairly rare) 1-colorant DeviceN case. Type 2 (Exponential
       * Interpolation) is the only Function type this port
       * evaluates, but it's also by far the most common one for
       * spot-color tint transforms in practice.
       */
      if (tintComps == 1) {
	ObjArrayGet(colorSpace, 2, &altObj, xref);
	ObjArrayGet(colorSpace, 3, &funcObj, xref);
	if (GfxParseType2Function(&funcObj, xref, this->funcC0,
				   this->funcC1, &this->funcN)) {
	  altCs.tintApprox = gFalse;
	  altCs.hasFunction = gFalse;
	  altCs.ok = gTrue;
	  GfxColorSpaceSetMode(&altCs, &altObj, xref);
	  if (altCs.ok) {
	    this->altMode = altCs.mode;
	    this->altNumComps = altCs.numComps;
	    this->hasFunction = gTrue;
	  }
	}
	ObjFree(&funcObj);
	ObjFree(&altObj);
      }

      if (this->hasFunction) {
	/* Mode is fully resolved already (altMode/altNumComps); skip
	 * the generic "get mode" step below entirely. numComps stays
	 * the real 1-byte-per-pixel count, NOT the alternate space's
	 * component count -- that distinction is the whole reason
	 * this bug existed in the first place. */
	this->indexed = gFalse;
	this->mode = colorGray;	/* unused when hasFunction is set */
	this->numComps = tintComps;
      } else {
	/* Fallback: DeviceN with >1 colorant, or a Function we can't
	 * evaluate (not Type 2, or malformed). Approximate as
	 * averaged/inverted gray -- see GfxImageColorMapGetColor. */
	ObjFree(&csObj);
	initName(&csObj, "DeviceGray");
      }
    }
    ObjFree(&obj);
  }

  // get mode
  if (!this->hasFunction) {
    this->indexed = gFalse;
    if (isName(&csObj)) {
      GfxColorSpaceSetMode(this, &csObj, xref);
    } else if (isArray(&csObj)) {
      ObjArrayGet(&csObj, 0, &obj, xref);
      if (isNameSame(&obj, "Indexed") || isNameSame(&obj, "I")) {
	this->indexed = gTrue;
	ObjArrayGet(&csObj, 1, &obj2, xref);
	GfxColorSpaceSetMode(this, &obj2, xref);
	ObjFree(&obj2);
      } else {
	GfxColorSpaceSetMode(this, &csObj, xref);
      }
      ObjFree(&obj);
    } else {
      goto err1;
    }
    if (!this->ok) {
      ObjFree(&csObj);
      return;
    }
    if (tintComps > 0) {
      /* override the numComps GfxColorSpaceSetMode just set for our
       * placeholder DeviceGray (1) with the real colorant count */
      this->numComps = tintComps;
      this->tintApprox = gTrue;
    }
  }

  // get lookup table for indexed colorspace
  if (this->indexed) {
    ObjArrayGet(&csObj, 2, &obj, xref);
    if (!isInt(&obj))
      goto err2;
    this->indexHigh = getInt(&obj);
    ObjFree(&obj);
    this->lookup = (Guchar (*)[4])gmalloc((this->indexHigh + 1) * 4 * sizeof(Guchar));
    if (!this->lookup)
      goto err1;
    ObjArrayGet(&csObj, 3, &obj, xref);
    if (isStream(&obj)) {
      ObjStreamReset(&obj);
      for (i = 0; i <= this->indexHigh; ++i) {
	for (j = 0; j < this->numComps; ++j) {
	  if ((x = ObjStreamGetChar(&obj)) == EOF)
	    goto err2;
	  this->lookup[i][j] = (Guchar)x;
	}
      }
    } else if (isString(&obj)) {
      s = GStrGetCString(getString(&obj));
      for (i = 0; i <= this->indexHigh; ++i)
	for (j = 0; j < this->numComps; ++j)
	  this->lookup[i][j] = (Guchar)*s++;
    } else {
      goto err2;
    }
    ObjFree(&obj);
  }

  ObjFree(&csObj);
  return;

 err2:
  ObjFree(&obj);
 err1:
  ObjFree(&csObj);
  this->ok = gFalse;
  EC_WARNING(-1);
}


void GfxColorSpaceFree(GfxColorSpace *this) {
//  if (sepFunc)
//    delete sepFunc;
  gfree(this->lookup);
}


static void GfxColorSpaceSetMode(GfxColorSpace *this, Obj *colorSpace, XRef *xref) {
  Obj obj;

  if (isNameSame(colorSpace, "DeviceGray") || isNameSame(colorSpace, "G")) {
    this->mode = colorGray;
    this->numComps = 1;
  } else if (isNameSame(colorSpace, "DeviceRGB") || isNameSame(colorSpace, "RGB")) {
    this->mode = colorRGB;
    this->numComps = 3;
  } else if (isNameSame(colorSpace, "DeviceCMYK") || isNameSame(colorSpace, "CMYK")) {
    this->mode = colorCMYK;
    this->numComps = 4;
  } else if (isArray(colorSpace)) {
    ObjArrayGet(colorSpace, 0, &obj, xref);
    if (isNameSame(&obj, "CalGray")) {
      this->mode = colorGray;
      this->numComps = 1;
    } else if (isNameSame(&obj, "CalRGB")) {
      this->mode = colorRGB;
      this->numComps = 3;
    } else if (isNameSame(&obj, "CalCMYK")) {
      this->mode = colorCMYK;
      this->numComps = 4;
    } else if (isNameSame(&obj, "ICCBased")) {
      /*
       * We don't interpret the ICC profile itself (that's a whole
       * subsystem this port never had). Per the spec (ISO 32000-1,
       * 8.6.5.5): if the profile isn't usable, fall back to a Device
       * colorspace picked by the stream dict's /N (number of
       * components: 1/3/4). This is the single most common
       * non-Device colorspace in real-world PDFs (most producers
       * wrap DeviceRGB/Gray/CMYK in an ICC profile even when they
       * don't need real color management), so this alone covers a
       * lot more files than CalGray/CalRGB ever will.
       */
      Obj streamObj, nObj;
      ObjArrayGet(colorSpace, 1, &streamObj, xref);
      if (isStream(&streamObj)) {
	DictLookup(ObjStreamGetDict(&streamObj), "N", &nObj, xref);
	if (isInt(&nObj)) {
	  switch (getInt(&nObj)) {
	  case 1:
	    this->mode = colorGray;
	    this->numComps = 1;
	    break;
	  case 3:
	    this->mode = colorRGB;
	    this->numComps = 3;
	    break;
	  case 4:
	    this->mode = colorCMYK;
	    this->numComps = 4;
	    break;
	  default:
	    /* /N is required to be 1, 3, or 4 -- anything else means
	     * a malformed file, not a colorspace we simply don't
	     * support yet. */
	    this->ok = gFalse;
	    break;
	  }
	} else {
	  this->ok = gFalse;
	}
	ObjFree(&nObj);
      } else {
	this->ok = gFalse;
      }
      ObjFree(&streamObj);
    } else {
      this->ok = gFalse;
    }
    ObjFree(&obj);
  } else {
    this->ok = gFalse;
  }
}

/***********************************************************************
 *		GfxEvalType2Function
 ***********************************************************************
 * SYNOPSIS:	    Evaluate a parsed Type 2 function for one raw tint
 *		    byte: out[j] = C0[j] + (tint/255)^N * (C1[j]-C0[j]),
 *		    scaled back to 0-255 and clamped. altNumComps
 *		    outputs are filled in (1-4, from the real
 *		    alternate colorspace).
 *
 * STRATEGY:	    gdouble is NOT a real IEEE double in this build --
 *		    local.mk sets USE_NATIVE_FLOAT_TYPE, which makes it
 *		    a 16.16 fixed-point WWFixed value (see gtypes.goh),
 *		    matching the rest of this codebase (GEOS shipped on
 *		    FPU-less hardware). Plain C division/pow() on a
 *		    gdouble silently truncates any fractional result
 *		    back to an integer -- every tint value except
 *		    exactly 0 or 255 collapsed to 0 before this fix, a
 *		    real bug, not a rounding nicety. Everything here
 *		    goes through GrMulWWFixed/GrUDivWWFixed instead,
 *		    same helpers already used for the font-width-factor
 *		    fixed-point math elsewhere in this port.
 *
 *		    There's no general fixed-point pow(), so the
 *		    exponent N is handled exactly for N==1 (by far the
 *		    common case for spot-color tint transforms) and any
 *		    small positive integer via repeated multiplication;
 *		    anything else falls back to linear (N==1) rather
 *		    than risk reintroducing a float/fixed-point mismatch.
 *
 ***********************************************************************/
static void
GfxEvalType2Function(GfxColorSpace *this, short tint, short out[4])
{
    gdouble t, tp, span, v;
    gdouble twoFiveFive;
    word nInt;
    short j;

    twoFiveFive = GdoubleToWWFixed(IntToGdouble(255));

    /* tint (0-255) as a WWFixed fraction of 1.0. 255 is a plain
     * integer constant here, not itself a WWFixed value, so plain
     * division is correct and safe (tint<<16 is at most ~16.7M, far
     * from overflowing 32 bits) -- GrUDivWWFixed is for dividing one
     * WWFixed value by another, not by a plain scalar. */
    t = GdoubleToWWFixed(IntToGdouble((word) tint)) / 255;

    tp = t;
    nInt = (word) GdoubleToWord(this->funcN);
    if (nInt >= 2 && nInt <= 8 && this->funcN == IntToGdouble(nInt)) {
	for (j = 1; j < nInt; ++j) {
	    tp = GrMulWWFixed(tp, t);
	}
    }

    for (j = 0; j < this->altNumComps; ++j) {
	span = GdoubleToWWFixed(this->funcC1[j]) - GdoubleToWWFixed(this->funcC0[j]);
	v = GdoubleToWWFixed(this->funcC0[j]) + GrMulWWFixed(tp, span);
	v = GrMulWWFixed(v, twoFiveFive);
	out[j] = (short) GdoubleToWord(v);
	if (out[j] < 0) {
	    out[j] = 0;
	} else if (out[j] > 255) {
	    out[j] = 255;
	}
    }
}

void GfxColorSpaceGetColor(GfxColorSpace *this, short x[4], GfxColor *color) {
  short y[4];
  Guchar *p;

  if (this->hasFunction) {
    GfxEvalType2Function(this, x[0], y);
    switch (this->altMode) {
    case colorGray:
      GfxColorSetGray(color, y[0]);
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, y[0], y[1], y[2], y[3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, y[0], y[1], y[2]);
      break;
    }
    return;
  }

//  if (sepFunc) {
//    sepFunc->transform(x, y);
//  } else {
    y[0] = x[0];
    y[1] = x[1];
    y[2] = x[2];
    y[3] = x[3];
//  }
  if (this->indexed) {
    p = this->lookup[y[0]];
    switch (this->mode) {
    case colorGray:
      GfxColorSetGray(color, p[0]);
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, p[0], p[1], p[2], p[3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, p[0], p[1], p[2]);
      break;
    }
  } else {
    switch (this->mode) {
    case colorGray:
      if (this->tintApprox) {
	/* DeviceN approximation: average the real colorant tints and
	 * invert (tint 1.0 = full ink = dark, DeviceGray 1.0 = white).
	 * numComps is 1-4, guaranteed by GfxColorSpaceInit. */
	word sum = 0;
	short k;
	for (k = 0; k < this->numComps; ++k) {
	  sum += (word) y[k];
	}
	GfxColorSetGray(color, 255 - (short) (sum / this->numComps));
      } else {
	GfxColorSetGray(color, y[0]);
      }
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, y[0], y[1], y[2], y[3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, y[0], y[1], y[2]);
      break;
    }
  }
}

short GfxColorSpaceGetNumPixelComps(GfxColorSpace *this) {
    return this->indexed ? 1 : this->numComps; 
}

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

void GfxImageColorMapInit(GfxImageColorMap *this, short bits1, Obj *decode,
				   GfxColorSpace *colorSpace1, XRef *xref) {
  GfxColor color;
  short x[4];
  short maxPixel;
  Obj obj;
  short i, j;

  this->ok = gTrue;

  // bits per component and colorspace
  this->bits = bits1;
  maxPixel = (1 << this->bits) - 1;
  this->colorSpace = colorSpace1;
  this->mode = colorSpace1->mode;

  // get decode map
  if (isNull(decode)) {
    if (this->colorSpace->indexed) {
      this->indexed = gTrue;
      this->numComps = 1;
      this->decodeLow[0] = 0;
      this->decodeRange[0] = IntToGdouble(maxPixel);
    } else {
      this->indexed = gFalse;
      this->numComps = GfxColorSpaceGetNumPixelComps(this->colorSpace);
      EC_ERROR_IF(this->numComps < 1 || this->numComps > 4,
		  -1);
      for (i = 0; i < this->numComps; ++i) {
	this->decodeLow[i] = 0;
	this->decodeRange[i] = IntToGdouble(1);
      }
    }
  } else if (isArray(decode)) {
    this->numComps = ObjArrayGetLength(decode) >> 1;
    if (this->numComps < 1 || this->numComps > 4)
      goto err1;
    if (this->numComps != GfxColorSpaceGetNumPixelComps(this->colorSpace))
      goto err1;
    this->indexed = this->colorSpace->indexed;
    for (i = 0; i < this->numComps; ++i) {
      ObjArrayGet(decode, 2*i, &obj, xref);
      if (!isNum(&obj))
	goto err2;
      this->decodeLow[i] = getNum(&obj);
      ObjFree(&obj);
      ObjArrayGet(decode, 2*i+1, &obj, xref);
      if (!isNum(&obj))
	goto err2;
      this->decodeRange[i] = getNum(&obj) - this->decodeLow[i];
      ObjFree(&obj);
    }
  } else {
    goto err1;
  }

  /*
   * Identity check (see project optimization analysis H4 and the
   * struct field's own comment) -- must run after bits/indexed/
   * numComps/decodeLow/decodeRange are all finalized above, and
   * before the lookup table itself gets built below (this doesn't
   * replace building it -- GfxImageColorMapGetColor still needs a
   * valid table for the non-fast-path cases, e.g. a different image
   * using the same colorspace with a real custom /Decode array).
   */
  this->isIdentity = !this->indexed && this->bits == 8 &&
		      !this->colorSpace->hasFunction &&
		      !this->colorSpace->tintApprox;
  if (this->isIdentity) {
    for (i = 0; i < this->numComps; ++i) {
      if (this->decodeLow[i] != 0 || this->decodeRange[i] != IntToGdouble(1)) {
	this->isIdentity = gFalse;
	break;
      }
    }
  }

  // construct lookup table
  this->lookup = (short (*)[4])gmalloc((maxPixel + 1) * 4 * sizeof(short));
  if (!this->lookup) {
    this->ok = gFalse;
    return;
  }
  if (this->indexed) {
    for (i = 0; i <= maxPixel; ++i) {
      x[0] = i;
      GfxColorSpaceGetColor(this->colorSpace, x, &color);
      this->lookup[i][0] = color.r;
      this->lookup[i][1] = color.g;
      this->lookup[i][2] = color.b;
    }
  } else {
    for (i = 0; i <= maxPixel; ++i)
      for (j = 0; j < this->numComps; ++j)
	this->lookup[i][j] = GdoubleToWord(255 * (this->decodeLow[j] + 
					   (i * this->decodeRange[j]) / maxPixel));
  }

  return;

 err2:
  ObjFree(&obj);
 err1:
  this->ok = gFalse;
}

void GfxImageColorMapFree(GfxImageColorMap *this) {
  GfxColorSpaceFree(this->colorSpace);
  gfree(this->lookup);
}

short GfxImageColorMapGetNumPixelComps(GfxImageColorMap *this) {
    return this->numComps; 
}

short GfxImageColorMapGetBits(GfxImageColorMap *this) {
    return this->bits; 
}

void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4], GfxColor *color) {
  short *p;
  short y[4];

  /*
   * Fast path for the identity mapping (see project optimization
   * analysis H4 and this struct's own comment on isIdentity): skips
   * the lookup[][] table entirely, using the raw sample byte
   * directly -- safe because isIdentity is only ever set when doing
   * so is mathematically exact (bits==8, default decode array, not
   * indexed, no hasFunction/tintApprox special-casing). Guchar (x[])
   * widens to short without any value loss (0-255 fits entirely
   * within short's positive range), matching what lookup[][] would
   * have handed back anyway for a genuinely identity table.
   */
  if (this->isIdentity) {
    switch (this->mode) {
    case colorGray:
      GfxColorSetGray(color, x[0]);
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, x[0], x[1], x[2], x[3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, x[0], x[1], x[2]);
      break;
    }
    return;
  }

  if (this->colorSpace->hasFunction) {
    /* real tint value, through its (usually identity) decode curve
     * first, same as every other component already does */
    GfxEvalType2Function(this->colorSpace, this->lookup[x[0]][0], y);
    switch (this->colorSpace->altMode) {
    case colorGray:
      GfxColorSetGray(color, y[0]);
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, y[0], y[1], y[2], y[3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, y[0], y[1], y[2]);
      break;
    }
    return;
  }

  if (this->indexed) {
    p = this->lookup[x[0]];
    GfxColorSetRGB(color, p[0], p[1], p[2]);
  } else {
    switch (this->mode) {
    case colorGray:
      if (this->colorSpace->tintApprox) {
	/* DeviceN/Separation approximation: average the real colorant
	 * tints (each through its own decode-curve entry, same as the
	 * CMYK/RGB cases below) and invert -- tint 1.0 = full ink =
	 * dark, the opposite sense of DeviceGray's 1.0 = white.
	 * numComps is 1-4, guaranteed by GfxColorSpaceInit. */
	word sum = 0;
	short k;
	for (k = 0; k < this->numComps; ++k) {
	  sum += (word) this->lookup[x[k]][k];
	}
	GfxColorSetGray(color, 255 - (short) (sum / this->numComps));
      } else {
	GfxColorSetGray(color, this->lookup[x[0]][0]);
      }
      break;
    case colorCMYK:
      GfxColorSetCMYK(color, this->lookup[x[0]][0], this->lookup[x[1]][1],
		     this->lookup[x[2]][2], this->lookup[x[3]][3]);
      break;
    case colorRGB:
      GfxColorSetRGB(color, this->lookup[x[0]][0], this->lookup[x[1]][1], 
		     this->lookup[x[2]][2]);
      break;
    }
  }
}


void GfxStateInit(GfxState *state, Handle gstring) {


    state->gstring = gstring;
    state->font = NULL;
    state->fontSize = 0;
    state->textMat[0] = 1; state->textMat[1] = 0;
    state->textMat[2] = 0; state->textMat[3] = 1;
    state->textMat[4] = 0; state->textMat[5] = 0;
    state->charSpace = 0;
    state->wordSpace = 0;
    state->horizScaling = 1L<<16;
    state->leading = 0;
    state->rise = 0;
    state->render = 0;
    state->lineX = state->lineY = 0;
    state->saved = NULL;
    state->pathType = PATH_NONE;

    /*
     * PDF spec default: DeviceGray, color 0 (black), until a cs/CS
     * sets something else. No XRef/Obj handy here to go through the
     * normal GfxColorSpaceInit machinery for a literal "DeviceGray"
     * Name -- these are exactly the fields GfxColorSpaceSetMode would
     * produce for it anyway, set directly.
     */
    state->fillColorSpace.mode = colorGray;
    state->fillColorSpace.indexed = gFalse;
    state->fillColorSpace.numComps = 1;
    state->fillColorSpace.lookup = NULL;
    state->fillColorSpace.ok = gTrue;
    state->fillColorSpace.tintApprox = gFalse;
    state->fillColorSpace.hasFunction = gFalse;
    state->strokeColorSpace = state->fillColorSpace;
    state->fillAlpha = IntToGdouble(1);
    state->strokeAlpha = IntToGdouble(1);
    state->fillColor.r = 0;
    state->fillColor.g = 0;
    state->fillColor.b = 0;
    state->pathRawBoundsValid = gFalse;
    state->pathSubpathStartX = state->pathSubpathStartY = 0;
    state->pathSubpathStartValid = gFalse;
}

void GfxStateFree(GfxState *state) {
//  if (fillColorSpace)
//    delete fillColorSpace;
//  if (strokeColorSpace)
//    delete strokeColorSpace;
//  gfree(lineDash);
//  delete path;

    if (state->saved) {
/*
 * Is this needed?  We save one of these for each GfxState saved...
 */
	GrRestoreState(state->gstring);

	GfxStateFree(state->saved);
	gfree(state->saved);
    }
}

// Used for copy();
void GfxStateCopy(GfxState *dest, GfxState *state) {
  memcpy(dest, state, sizeof(GfxState));

//  if (fillColorSpace)
//    fillColorSpace = state->fillColorSpace->copy();
//  if (strokeColorSpace)
//    strokeColorSpace = state->strokeColorSpace->copy();
//  if (lineDashLength > 0) {
//    lineDash = (double *)gmalloc(lineDashLength * sizeof(double));
//    memcpy(lineDash, state->lineDash, lineDashLength * sizeof(double));
//  }
//  path = state->path->copy();

    dest->saved = NULL;
}


/*
 * I wrote these such that "state" still points to the top of the chain.
 * That saves us from passing pointers to the state pointer in the
 * caller of these functions.
 */
void GfxStateSave(GfxState *state) {
  GfxState *newState;

  newState = gmalloc( sizeof(GfxState) );
  if (!newState)
    return;
  GfxStateCopy(newState, state);
  newState->saved = state->saved;
  state->saved = newState;
}

void GfxStateRestore(GfxState *state) {
  GfxState *oldState;

  if (state->saved) {
    oldState = state->saved;
    GfxStateCopy(state, oldState);
    state->saved = oldState->saved;
    oldState->saved = NULL;
    GfxStateFree(oldState);
    gfree(oldState);
  } 
}

  // Accessors.

  GfxFont *GfxStateGetFont(GfxState *state) { return state->font; }
  gdouble *GfxStateGetTextMat(GfxState *state) { return state->textMat; }
  sdword GfxStateGetCharSpace(GfxState *state) { return state->charSpace; }
  gdouble GfxStateGetWordSpace(GfxState *state) { return state->wordSpace; }
  sdword GfxStateGetHorizScaling(GfxState *state) { return state->horizScaling; }
  gdouble GfxStateGetLeading(GfxState *state) { return state->leading; }
  sdword GfxStateGetRise(GfxState *state) { return state->rise; }
  short GfxStateGetRender(GfxState *state) { return state->render; }

gdouble GfxStateGetFontSize(GfxState *state) { 
    return state->fontSize;
}

  // Change state parameters.
  void GfxStateSetFont(GfxState *state, GfxFont *font1, gdouble fontSize1)
    { state->font = font1; state->fontSize = fontSize1; }


void GfxStateSetTextMat(GfxState *state, gdouble a, gdouble b, gdouble c,
			  gdouble d, gdouble e, gdouble f) { 

    WWFixedAsDWord tm[7];
    /*
     * Restore to saved transform, then apply this one
     */
    GrRestoreTransform(state->gstring);
    GrSaveTransform(state->gstring);

    /*
     * I'm simulating a 6-element array with DWFixed as the last two 
     * elements as a 7-element array with all WWFixed elements.  So, 
     * the last two elements have to be jiggered a bit:
     *
     * frac int-low int-high=0  frac int-low int-high=0
     * ^tm[4]       ^tm[5]           ^tm[6]
     */
    tm[0] = GdoubleToWWFixed(a);
    tm[1] = GdoubleToWWFixed(b);
    tm[2] = GdoubleToWWFixed(c);
    tm[3] = GdoubleToWWFixed(d);
    tm[4] = GdoubleToWWFixed(e);
    tm[6] = GdoubleToWWFixed(f);

    tm[5] = tm[6] << 16;
    tm[6] >>= 16;

    GrApplyTransform(state->gstring, (TransMatrix*) tm);
}


  void GfxStateSetCharSpace(GfxState *state, gdouble space)
    { state->charSpace = GdoubleToWWFixed(space); }

  void GfxStateSetWordSpace(GfxState *state, gdouble space)
    { state->wordSpace = space; }

  void GfxStateSetHorizScaling(GfxState *state, gdouble scale)
    { state->horizScaling = GdoubleToWWFixed(0.01 * scale); }

  void GfxStateSetLeading(GfxState *state, gdouble leading1)
    { state->leading = leading1; }

  void GfxStateSetRise(GfxState *state, gdouble rise1)
    { state->rise = GdoubleToWWFixed(rise1); }

  void GfxStateSetRender(GfxState *state, short render1)
    { state->render = render1; }

  // Text position.
void GfxStateTextMoveTo(GfxState *state, gdouble tx, gdouble ty) { 
//    double curX, curY;
    sdword x, y;

    state->lineX = tx; 
    state->lineY = ty; 

//    textTransform(state, tx, ty, &curX, &curY); 
//    GrMoveTo(state->gstring, curX, curY);

    x = GdoubleToWWFixed(tx);
    y = GdoubleToWWFixed(ty);
    state->curTextX = x;

    GrMoveToWWFixed(state->gstring, x, y);
}

void GfxStateTextShift(GfxState *state, gdouble tx) {
//    double dx, dy;

//  textTransformDelta(state, tx, 0, &dx, &dy);
//  GrRelMoveTo(state->gstring, MakeWWFixed(dx), MakeWWFixed(dy));

  state->curTextX += GdoubleToWWFixed(tx);
  GrRelMoveTo(state->gstring, ((sdword)((dword)GdoubleToWWFixed(tx) << 5)), 0);
}
