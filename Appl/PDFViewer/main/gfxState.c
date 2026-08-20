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


/***********************************************************************
 *    GfxColor
 ***********************************************************************/

/***********************************************************************
 *      GfxColorSetCMYK
 ***********************************************************************
 * SYNOPSIS:        set a color from CMYK components
 * PARAMETERS:      GfxColor *this  color to set
 *                  short c, m, y, k  CMYK components (0-255)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by colorspace/color-map code once raw CMYK components
 *      have been resolved.
 *
 * STRATEGY:
 *      Convert via the standard r=255-(c+k) etc. formula, clamped
 *      to 0.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k)
{
    if ((this->r = 255 - (c + k)) < 0) {
        this->r = 0;
    }
    if ((this->g = 255 - (m + k)) < 0) {
        this->g = 0;
    }
    if ((this->b = 255 - (y + k)) < 0) {
        this->b = 0;
    }
}

/***********************************************************************
 *      GfxColorSetGray
 ***********************************************************************
 * SYNOPSIS:        set a color from a single gray component
 * PARAMETERS:      GfxColor *this  color to set
 *                  short gray      gray level (0-255)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by colorspace/color-map code for DeviceGray output.
 *
 * STRATEGY:
 *      Assign gray to all three RGB channels.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSetGray(GfxColor *this, short gray)
{
    this->r = this->g = this->b = gray;
}

/***********************************************************************
 *      GfxColorSetRGB
 ***********************************************************************
 * SYNOPSIS:        set a color from RGB components
 * PARAMETERS:      GfxColor *this  color to set
 *                  short r1, g1, b1  RGB components (0-255)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by colorspace/color-map code for DeviceRGB output.
 *
 * STRATEGY:
 *      Assign each component directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSetRGB(GfxColor *this, short r1, short g1, short b1)
{
    this->r = r1;
    this->g = g1;
    this->b = b1;
}


/***********************************************************************
 *    GfxColorSpace
 ***********************************************************************/

/*
 * Parse a Function dict's numeric array entry (e.g. /C0, /C1) into up
 * to 4 coordinates; non-numeric or missing elements keep the caller's
 * defaults in coords.
 */
static void
GfxParseCoordArray(Dict *dict, const char *key, XRef *xref, gdouble coords[4])
{
    Obj arrObj, elemObj;
    word i, len;

    DictLookup(dict, key, &arrObj, xref);
    if (isArray(&arrObj)) {
        len = ObjArrayGetLength(&arrObj);
        if (len > 4) {
            len = 4;
        }

        for (i = 0; i < len; ++i) {
            ObjArrayGet(&arrObj, i, &elemObj, xref);
            if (isNum(&elemObj)) {
                coords[i] = getNum(&elemObj);
            }
            ObjFree(&elemObj);
        }
    }
    ObjFree(&arrObj);
}

/*
 * Parse a Function dict as Type 2 (Exponential Interpolation):
 * out[j] = C0[j] + x^N * (C1[j]-C0[j]) -- the only Function type this
 * port evaluates. Returns gFalse (caller falls back to tintApprox) for
 * any other type or a malformed dict.
 */
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

/***********************************************************************
 *      GfxColorSpaceInit
 ***********************************************************************
 * SYNOPSIS:        parse a PDF colorspace object
 * PARAMETERS:      GfxColorSpace *this  colorspace to initialize
 *                  Obj *colorSpace      the /ColorSpace object (name
 *                                       or array form)
 *                  XRef *xref           cross-reference table
 *
 * RETURNS:         void (this->ok reflects success)
 *
 * CONTEXT:
 *      Called for every colorspace referenced by page content,
 *      images, and Indexed base colorspaces.
 *
 * STRATEGY:
 *      Detect Separation/DeviceN and try to resolve a real Type 2
 *      tint-transform Function for the 1-colorant case; otherwise
 *      fall back to an averaged/inverted-gray approximation. Resolve
 *      the base mode via GfxColorSpaceSetMode() and, if Indexed,
 *      build the palette lookup table from the stream or string data.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSpaceInit(GfxColorSpace *this, Obj *colorSpace, XRef *xref)
{
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

    /* check for Separation/DeviceN colorspace */
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
             * (fairly rare) 1-colorant DeviceN case.
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
                 * the generic "get mode" step below. numComps stays the
                 * real 1-byte-per-pixel count, NOT the alternate space's
                 * component count. */
                this->indexed = gFalse;
                this->mode = colorGray; /* unused when hasFunction is set */
                this->numComps = tintComps;
            } else {
                /* Fallback: DeviceN with >1 colorant, or a Function we
                 * can't evaluate. Approximate as averaged/inverted gray --
                 * see GfxImageColorMapGetColor. */
                ObjFree(&csObj);
                initName(&csObj, "DeviceGray");
            }
        }
        ObjFree(&obj);
    }

    /* get mode */
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

    /* get lookup table for indexed colorspace */
    if (this->indexed) {
        ObjArrayGet(&csObj, 2, &obj, xref);
        if (!isInt(&obj)) {
            goto err2;
        }
        this->indexHigh = getInt(&obj);
        ObjFree(&obj);
        this->lookup = (Guchar (*)[4])gmalloc((this->indexHigh + 1) * 4 * sizeof(Guchar));
        if (!this->lookup) {
            goto err1;
        }
        ObjArrayGet(&csObj, 3, &obj, xref);
        if (isStream(&obj)) {
            ObjStreamReset(&obj);
            for (i = 0; i <= this->indexHigh; ++i) {
                for (j = 0; j < this->numComps; ++j) {
                    if ((x = ObjStreamGetChar(&obj)) == EOF) {
                        goto err2;
                    }
                    this->lookup[i][j] = (Guchar)x;
                }
            }
        } else if (isString(&obj)) {
            s = GStrGetCString(getString(&obj));
            for (i = 0; i <= this->indexHigh; ++i) {
                for (j = 0; j < this->numComps; ++j) {
                    this->lookup[i][j] = (Guchar)*s++;
                }
            }
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

/***********************************************************************
 *      GfxColorSpaceFree
 ***********************************************************************
 * SYNOPSIS:        release storage owned by a colorspace
 * PARAMETERS:      GfxColorSpace *this  colorspace to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when a colorspace is no longer needed (e.g. page or
 *      image color map teardown).
 *
 * STRATEGY:
 *      Free the Indexed palette lookup table, if any.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSpaceFree(GfxColorSpace *this)
{
    gfree(this->lookup);
}

/*
 * Set colorspace to a mode + component count. For ICCBased we don't 
 * interpret the profile; per spec we fall back to a Device space picked 
 * by the stream's /N (1/3/4).
 */
static void GfxColorSpaceSetMode(GfxColorSpace *this, Obj *colorSpace, XRef *xref)
{
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
                        /* /N is required to be 1, 3, or 4 -- anything else
                         * means a malformed file. */
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

/*
 * Evaluate a parsed Type 2 function for one tint byte:
 * out[j] = C0[j] + (tint/255)^N * (C1[j]-C0[j]). gdouble is a 16.16
 * WWFixed here, not IEEE double, so this goes through GrMulWWFixed;
 * N is exact for small positive integers, else falls back to N==1.
 */
static void
GfxEvalType2Function(GfxColorSpace *this, short tint, short out[4])
{
    gdouble t, tp, span, v;
    gdouble twoFiveFive;
    word nInt;
    short j;

    twoFiveFive = GdoubleToWWFixed(IntToGdouble(255));

    /* tint (0-255) as a WWFixed fraction of 1.0. 255 is a plain integer
     * constant here, not itself a WWFixed value, so plain division is
     * correct and safe. */
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

/***********************************************************************
 *      GfxColorSpaceGetColor
 ***********************************************************************
 * SYNOPSIS:        convert a raw component tuple to a device color
 * PARAMETERS:      GfxColorSpace *this  colorspace to convert through
 *                  short x[4]           raw component values
 *                  GfxColor *color      out: resulting device color
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for each Indexed palette entry and by
 *      GfxImageColorMapInit() while building its lookup table.
 *
 * STRATEGY:
 *      If a real tint-transform Function is available, evaluate it.
 *      Otherwise pass components through (Indexed via palette lookup,
 *      device modes directly); a tintApprox DeviceN falls back to an
 *      averaged, inverted gray.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSpaceGetColor(GfxColorSpace *this, short x[4], GfxColor *color)
{
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

    y[0] = x[0];
    y[1] = x[1];
    y[2] = x[2];
    y[3] = x[3];

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
                /* DeviceN approximation: average the real colorant tints
                 * and invert (tint 1.0 = full ink = dark, DeviceGray
                 * 1.0 = white). numComps is 1-4, guaranteed by
                 * GfxColorSpaceInit. */
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

/***********************************************************************
 *      GfxColorSpaceGetNumPixelComps
 ***********************************************************************
 * SYNOPSIS:        get the number of components per pixel sample
 * PARAMETERS:      GfxColorSpace *this  colorspace to query
 *
 * RETURNS:         short  1 for Indexed, else this->numComps
 *
 * CONTEXT:
 *      Called to size per-pixel sample buffers before decoding.
 *
 * STRATEGY:
 *      Indexed colorspaces always store a single palette index per
 *      pixel regardless of the palette's own component count.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
short GfxColorSpaceGetNumPixelComps(GfxColorSpace *this)
{
    return this->indexed ? 1 : this->numComps;
}


/***********************************************************************
 *    GfxImageColorMap
 ***********************************************************************/

/***********************************************************************
 *      GfxImageColorMapInit
 ***********************************************************************
 * SYNOPSIS:        build a lookup table mapping image samples to
 *                   device colors
 * PARAMETERS:      GfxImageColorMap *this  color map to initialize
 *                  short bits1             bits per component
 *                  Obj *decode             /Decode array, or null for
 *                                          the colorspace default
 *                  GfxColorSpace *colorSpace1  the image's colorspace
 *                  XRef *xref              cross-reference table
 *
 * RETURNS:         void (this->ok reflects success)
 *
 * CONTEXT:
 *      Called once per image XObject before decoding its samples.
 *
 * STRATEGY:
 *      Resolve /Decode (or spec defaults), detect the identity
 *      mapping fast path (8-bit, non-Indexed, default decode, no
 *      tint function/approximation), then build a maxPixel+1 entry
 *      lookup table via GfxColorSpaceGetColor() or linear scaling.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxImageColorMapInit(GfxImageColorMap *this, short bits1, Obj *decode,
                           GfxColorSpace *colorSpace1, XRef *xref)
{
    GfxColor color;
    short x[4];
    short maxPixel;
    Obj obj;
    short i, j;

    this->ok = gTrue;

    /* bits per component and colorspace */
    this->bits = bits1;
    maxPixel = (1 << this->bits) - 1;
    this->colorSpace = colorSpace1;
    this->mode = colorSpace1->mode;

    /* get decode map */
    if (isNull(decode)) {
        if (this->colorSpace->indexed) {
            this->indexed = gTrue;
            this->numComps = 1;
            this->decodeLow[0] = 0;
            this->decodeRange[0] = IntToGdouble(maxPixel);
        } else {
            this->indexed = gFalse;
            this->numComps = GfxColorSpaceGetNumPixelComps(this->colorSpace);
            EC_ERROR_IF(this->numComps < 1 || this->numComps > 4, -1);
            for (i = 0; i < this->numComps; ++i) {
                this->decodeLow[i] = 0;
                this->decodeRange[i] = IntToGdouble(1);
            }
        }
    } else if (isArray(decode)) {
        this->numComps = ObjArrayGetLength(decode) >> 1;
        if (this->numComps < 1 || this->numComps > 4) {
            goto err1;
        }
        if (this->numComps != GfxColorSpaceGetNumPixelComps(this->colorSpace)) {
            goto err1;
        }
        this->indexed = this->colorSpace->indexed;
        for (i = 0; i < this->numComps; ++i) {
            ObjArrayGet(decode, 2 * i, &obj, xref);
            if (!isNum(&obj)) {
                goto err2;
            }
            this->decodeLow[i] = getNum(&obj);
            ObjFree(&obj);
            ObjArrayGet(decode, 2 * i + 1, &obj, xref);
            if (!isNum(&obj)) {
                goto err2;
            }
            this->decodeRange[i] = getNum(&obj) - this->decodeLow[i];
            ObjFree(&obj);
        }
    } else {
        goto err1;
    }

    /*
     * Identity fast-path check: must run after bits/indexed/numComps/
     * decodeLow/decodeRange are all finalized, and before the lookup
     * table is built below (still built regardless -- needed for the
     * non-fast-path cases, e.g. a different image on the same
     * colorspace with a real custom /Decode array).
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

    /* construct lookup table */
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
        for (i = 0; i <= maxPixel; ++i) {
            for (j = 0; j < this->numComps; ++j) {
                this->lookup[i][j] = GdoubleToWord(255 * (this->decodeLow[j] +
                                     (i * this->decodeRange[j]) / maxPixel));
            }
        }
    }

    return;

err2:
    ObjFree(&obj);
err1:
    this->ok = gFalse;
}

/***********************************************************************
 *      GfxImageColorMapFree
 ***********************************************************************
 * SYNOPSIS:        release storage owned by an image color map
 * PARAMETERS:      GfxImageColorMap *this  color map to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when an image XObject's color map is no longer needed.
 *
 * STRATEGY:
 *      Free the underlying colorspace and the lookup table.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxImageColorMapFree(GfxImageColorMap *this)
{
    GfxColorSpaceFree(this->colorSpace);
    gfree(this->lookup);
}

/***********************************************************************
 *      GfxImageColorMapGetNumPixelComps
 ***********************************************************************
 * SYNOPSIS:        get the number of components per pixel sample
 * PARAMETERS:      GfxImageColorMap *this  color map to query
 *
 * RETURNS:         short  this->numComps
 *
 * CONTEXT:
 *      Called by image decoding code to size per-pixel sample reads.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
short GfxImageColorMapGetNumPixelComps(GfxImageColorMap *this)
{
    return this->numComps;
}

/***********************************************************************
 *      GfxImageColorMapGetBits
 ***********************************************************************
 * SYNOPSIS:        get the number of bits per component
 * PARAMETERS:      GfxImageColorMap *this  color map to query
 *
 * RETURNS:         short  this->bits
 *
 * CONTEXT:
 *      Called by image decoding code to size per-component reads.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
short GfxImageColorMapGetBits(GfxImageColorMap *this)
{
    return this->bits;
}

/***********************************************************************
 *      GfxImageColorMapGetColor
 ***********************************************************************
 * SYNOPSIS:        convert a raw image sample to a device color
 * PARAMETERS:      GfxImageColorMap *this  color map to convert through
 *                  Guchar x[4]             raw sample components
 *                  GfxColor *color         out: resulting device color
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called once per pixel while rendering an image XObject.
 *
 * STRATEGY:
 *      Take the isIdentity fast path when possible (raw sample used
 *      directly, no lookup). Otherwise evaluate a tint Function if
 *      present, or index into this->lookup, applying the same
 *      DeviceN averaging approximation as GfxColorSpaceGetColor().
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4], GfxColor *color)
{
    short *p;
    short y[4];

    /*
     * isIdentity is only ever set when using the raw sample directly is
     * mathematically exact (see GfxImageColorMapInit); Guchar widens to
     * short without value loss.
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
         * first, same as every other component below */
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
                /* DeviceN/Separation approximation, same idea as
                 * GfxColorSpaceGetColor: average each colorant's own
                 * decode-curve lookup and invert. */
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


/***********************************************************************
 *    GfxState
 ***********************************************************************/

/***********************************************************************
 *      GfxStateInit
 ***********************************************************************
 * SYNOPSIS:        construct a default GfxState
 * PARAMETERS:      GfxState *state  state to initialize
 *                  Handle gstring   GEOS gstring this state draws into
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called once when a page's graphics state is set up.
 *
 * STRATEGY:
 *      Zero/default all text and path parameters to PDF spec
 *      defaults, including an identity text matrix. Fill/stroke
 *      colorspace default to DeviceGray/black without going through
 *      GfxColorSpaceInit(), since no XRef/Obj is available here.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateInit(GfxState *state, Handle gstring)
{
    state->gstring = gstring;
    state->font = NULL;
    state->fontSize = 0;
    state->textMat[0] = 1;
    state->textMat[1] = 0;
    state->textMat[2] = 0;
    state->textMat[3] = 1;
    state->textMat[4] = 0;
    state->textMat[5] = 0;
    state->charSpace = 0;
    state->wordSpace = 0;
    state->horizScaling = 1L << 16;
    state->leading = 0;
    state->rise = 0;
    state->render = 0;
    state->lineX = state->lineY = 0;
    state->saved = NULL;
    state->pathType = PATH_NONE;

    /* PDF spec default: DeviceGray, color 0 (black), until a cs/CS sets
     * something else. */
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

/***********************************************************************
 *      GfxStateFree
 ***********************************************************************
 * SYNOPSIS:        release the saved-state chain owned by a GfxState
 * PARAMETERS:      GfxState *state  state to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when a page's graphics state is torn down, and
 *      recursively by GfxStateRestore() for the popped state.
 *
 * STRATEGY:
 *      If a saved state exists, restore the GEOS transform stack via
 *      GrRestoreState() and recursively free it. (Open question:
 *      whether GrRestoreState() here is still needed given each
 *      GfxStateSave() also does its own GrSaveTransform().)
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateFree(GfxState *state)
{
    if (state->saved) {
        GrRestoreState(state->gstring);

        GfxStateFree(state->saved);
        gfree(state->saved);
    }
}

/***********************************************************************
 *      GfxStateCopy
 ***********************************************************************
 * SYNOPSIS:        copy all fields of one GfxState into another
 * PARAMETERS:      GfxState *dest   destination state
 *                  GfxState *state  source state
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by GfxStateSave() and GfxStateRestore() to snapshot or
 *      restore a state.
 *
 * STRATEGY:
 *      memcpy() the whole struct, then clear dest->saved so the copy
 *      doesn't inherit the source's saved-state chain.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateCopy(GfxState *dest, GfxState *state)
{
    memcpy(dest, state, sizeof(GfxState));
    dest->saved = NULL;
}

/***********************************************************************
 *      GfxStateSave
 ***********************************************************************
 * SYNOPSIS:        push the current state onto its own saved-state
 *                   stack
 * PARAMETERS:      GfxState *state  state to save (q operator)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "q" operator.
 *
 * STRATEGY:
 *      Allocate a copy of the current state and link it in as
 *      state->saved, so state itself always remains the top of the
 *      chain -- callers never need a pointer-to-pointer.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSave(GfxState *state)
{
    GfxState *newState;

    newState = gmalloc(sizeof(GfxState));
    if (!newState) {
        return;
    }
    GfxStateCopy(newState, state);
    newState->saved = state->saved;
    state->saved = newState;
}

/***********************************************************************
 *      GfxStateRestore
 ***********************************************************************
 * SYNOPSIS:        pop and restore the most recently saved state
 * PARAMETERS:      GfxState *state  state to restore into (Q operator)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Q" operator. No-op if
 *      there is nothing saved.
 *
 * STRATEGY:
 *      Copy the saved state back into state (keeping state as the
 *      top of the chain, see GfxStateSave()), then free the popped
 *      node.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateRestore(GfxState *state)
{
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

/***********************************************************************
 *      GfxStateGetFont
 ***********************************************************************
 * SYNOPSIS:        get the current font
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         GfxFont *  state->font
 *
 * CONTEXT:
 *      Called by text-rendering code to select the active font.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
GfxFont *GfxStateGetFont(GfxState *state)
{
    return state->font;
}

/***********************************************************************
 *      GfxStateGetFontSize
 ***********************************************************************
 * SYNOPSIS:        get the current font size
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         gdouble  state->fontSize
 *
 * CONTEXT:
 *      Called by text-rendering code to scale glyph metrics.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
gdouble GfxStateGetFontSize(GfxState *state)
{
    return state->fontSize;
}

/***********************************************************************
 *      GfxStateGetTextMat
 ***********************************************************************
 * SYNOPSIS:        get the current text matrix
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         gdouble *  state->textMat, a 6-element PDF matrix
 *
 * CONTEXT:
 *      Called by text-rendering code needing the raw matrix, e.g.
 *      for glyph-space to device-space transforms.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
gdouble *GfxStateGetTextMat(GfxState *state)
{
    return state->textMat;
}

/***********************************************************************
 *      GfxStateGetCharSpace
 ***********************************************************************
 * SYNOPSIS:        get the current character spacing
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         sdword  state->charSpace, as WWFixed
 *
 * CONTEXT:
 *      Called by text-rendering code to advance the pen after each
 *      glyph.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
sdword GfxStateGetCharSpace(GfxState *state)
{
    return state->charSpace;
}

/***********************************************************************
 *      GfxStateGetWordSpace
 ***********************************************************************
 * SYNOPSIS:        get the current word spacing
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         gdouble  state->wordSpace
 *
 * CONTEXT:
 *      Called by text-rendering code to add extra advance after
 *      space characters.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
gdouble GfxStateGetWordSpace(GfxState *state)
{
    return state->wordSpace;
}

/***********************************************************************
 *      GfxStateGetHorizScaling
 ***********************************************************************
 * SYNOPSIS:        get the current horizontal scaling
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         sdword  state->horizScaling, as WWFixed
 *
 * CONTEXT:
 *      Called by text-rendering code to stretch/compress glyph
 *      advances horizontally.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
sdword GfxStateGetHorizScaling(GfxState *state)
{
    return state->horizScaling;
}

/***********************************************************************
 *      GfxStateGetLeading
 ***********************************************************************
 * SYNOPSIS:        get the current leading
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         gdouble  state->leading
 *
 * CONTEXT:
 *      Called by text-rendering code between text lines (T/TD).
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
gdouble GfxStateGetLeading(GfxState *state)
{
    return state->leading;
}

/***********************************************************************
 *      GfxStateGetRise
 ***********************************************************************
 * SYNOPSIS:        get the current text rise
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         sdword  state->rise, as WWFixed
 *
 * CONTEXT:
 *      Called by text-rendering code for superscript/subscript
 *      baseline offsets.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
sdword GfxStateGetRise(GfxState *state)
{
    return state->rise;
}

/***********************************************************************
 *      GfxStateGetRender
 ***********************************************************************
 * SYNOPSIS:        get the current text rendering mode
 * PARAMETERS:      GfxState *state  state to query
 *
 * RETURNS:         short  state->render
 *
 * CONTEXT:
 *      Called by text-rendering code to decide fill/stroke/clip
 *      behavior for glyphs.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
short GfxStateGetRender(GfxState *state)
{
    return state->render;
}

/***********************************************************************
 *      GfxStateSetFont
 ***********************************************************************
 * SYNOPSIS:        set the current font and size
 * PARAMETERS:      GfxState *state    state to modify
 *                  GfxFont *font1     font to select
 *                  gdouble fontSize1  font size in text space units
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tf" operator.
 *
 * STRATEGY:
 *      Assign both fields directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetFont(GfxState *state, GfxFont *font1, gdouble fontSize1)
{
    state->font = font1;
    state->fontSize = fontSize1;
}

/***********************************************************************
 *      GfxStateSetTextMat
 ***********************************************************************
 * SYNOPSIS:        set the text matrix
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble a, b, c, d, e, f  6-element PDF text matrix
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tm" operator.
 *
 * STRATEGY:
 *      Restore the GEOS transform stack to the pre-text-matrix state,
 *      then apply the new matrix. GEOS's TransMatrix packs the last
 *      two elements as DWFixed; we simulate that with a 7-element
 *      WWFixed array, splitting f's high/low words into tm[5]/tm[6].
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetTextMat(GfxState *state, gdouble a, gdouble b, gdouble c,
                         gdouble d, gdouble e, gdouble f)
{
    WWFixedAsDWord tm[7];

    GrRestoreTransform(state->gstring);
    GrSaveTransform(state->gstring);

    tm[0] = GdoubleToWWFixed(a);
    tm[1] = GdoubleToWWFixed(b);
    tm[2] = GdoubleToWWFixed(c);
    tm[3] = GdoubleToWWFixed(d);
    tm[4] = GdoubleToWWFixed(e);
    tm[6] = GdoubleToWWFixed(f);

    tm[5] = tm[6] << 16;
    tm[6] >>= 16;

    GrApplyTransform(state->gstring, (TransMatrix *) tm);
}

/***********************************************************************
 *      GfxStateSetCharSpace
 ***********************************************************************
 * SYNOPSIS:        set the character spacing
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble space    character spacing, text space units
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tc" operator.
 *
 * STRATEGY:
 *      Convert to WWFixed and assign.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetCharSpace(GfxState *state, gdouble space)
{
    state->charSpace = GdoubleToWWFixed(space);
}

/***********************************************************************
 *      GfxStateSetWordSpace
 ***********************************************************************
 * SYNOPSIS:        set the word spacing
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble space    word spacing, text space units
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tw" operator.
 *
 * STRATEGY:
 *      Assign directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetWordSpace(GfxState *state, gdouble space)
{
    state->wordSpace = space;
}

/***********************************************************************
 *      GfxStateSetHorizScaling
 ***********************************************************************
 * SYNOPSIS:        set the horizontal scaling
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble scale    scaling as a percentage (PDF /Tz)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tz" operator.
 *
 * STRATEGY:
 *      Convert the percentage to a 0-1 WWFixed fraction and assign.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetHorizScaling(GfxState *state, gdouble scale)
{
    state->horizScaling = GdoubleToWWFixed(0.01 * scale);
}

/***********************************************************************
 *      GfxStateSetLeading
 ***********************************************************************
 * SYNOPSIS:        set the leading
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble leading1  leading, text space units
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "TL" operator.
 *
 * STRATEGY:
 *      Assign directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetLeading(GfxState *state, gdouble leading1)
{
    state->leading = leading1;
}

/***********************************************************************
 *      GfxStateSetRise
 ***********************************************************************
 * SYNOPSIS:        set the text rise
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble rise1    rise, text space units
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Ts" operator.
 *
 * STRATEGY:
 *      Convert to WWFixed and assign.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetRise(GfxState *state, gdouble rise1)
{
    state->rise = GdoubleToWWFixed(rise1);
}

/***********************************************************************
 *      GfxStateSetRender
 ***********************************************************************
 * SYNOPSIS:        set the text rendering mode
 * PARAMETERS:      GfxState *state  state to modify
 *                  short render1    rendering mode (PDF /Tr)
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Tr" operator.
 *
 * STRATEGY:
 *      Assign directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateSetRender(GfxState *state, short render1)
{
    state->render = render1;
}

/***********************************************************************
 *      GfxStateTextMoveTo
 ***********************************************************************
 * SYNOPSIS:        move the text line origin
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble tx, ty   new text line origin, text space
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for the PDF content stream "Td"/"TD"/"T*" operators.
 *
 * STRATEGY:
 *      Store the text-space coordinates, convert to WWFixed, and
 *      move the GEOS drawing position via GrMoveToWWFixed().
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateTextMoveTo(GfxState *state, gdouble tx, gdouble ty)
{
    sdword x, y;

    state->lineX = tx;
    state->lineY = ty;

    x = GdoubleToWWFixed(tx);
    y = GdoubleToWWFixed(ty);
    state->curTextX = x;

    GrMoveToWWFixed(state->gstring, x, y);
}

/***********************************************************************
 *      GfxStateTextShift
 ***********************************************************************
 * SYNOPSIS:        shift the text position horizontally
 * PARAMETERS:      GfxState *state  state to modify
 *                  gdouble tx       horizontal shift, text space
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called between glyphs to advance the pen position (e.g. after
 *      each character or word-space adjustment).
 *
 * STRATEGY:
 *      Track the shift in curTextX and move the GEOS drawing position
 *      by the same amount, scaled from WWFixed to GEOS's native unit.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateTextShift(GfxState *state, gdouble tx)
{
    state->curTextX += GdoubleToWWFixed(tx);
    GrRelMoveTo(state->gstring, ((sdword)((dword)GdoubleToWWFixed(tx) << 5)), 0);
}
