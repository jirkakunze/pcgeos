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

static void GfxColorSpaceSetMode(GfxColorSpace *this, Obj *colorSpace,
    XRef *xref);
static GBool GfxParseType2Function(Obj *funcObj, XRef *xref,
    gdouble c0[4], gdouble c1[4], gdouble *n);

/*
 * GfxColor
 */

/***********************************************************************
 *      GfxColorSetCMYK
 ***********************************************************************
 * SYNOPSIS:        Set cmyk.
 * PARAMETERS:      GfxColor *this    this
 *                  short c    c
 *                  short m    m
 *                  short y    y
 *                  short k    k
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Convert via the standard r=255-(c+k) etc. formula, clamped to 0.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k)
{
    if ((this->r = 255 - (c + k)) < 0)
    {
        this->r = 0;
    }
    if ((this->g = 255 - (m + k)) < 0)
    {
        this->g = 0;
    }
    if ((this->b = 255 - (y + k)) < 0)
    {
        this->b = 0;
    }
}

/***********************************************************************
 *      GfxColorSetGray
 ***********************************************************************
 * SYNOPSIS:        Set gray.
 * PARAMETERS:      GfxColor *this    this
 *                  short gray    gray
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set rgb.
 * PARAMETERS:      GfxColor *this    this
 *                  short r1    r1
 *                  short g1    g1
 *                  short b1    b1
 *
 * RETURNS:         void
 *
 * CONTEXT:
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

/*
 * GfxColorSpace
 */

/***********************************************************************
 *      GfxParseCoordArray
 ***********************************************************************
 * SYNOPSIS:        Parse coord array.
 * PARAMETERS:      Dict *dict    dictionary
 *                  const char *key    key
 *                  XRef *xref    cross-reference table
 *                  gdouble coords[4]    coords
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void
GfxParseCoordArray(Dict *dict, const char *key, XRef *xref, gdouble coords[4])
{
    Obj arrObj, elemObj;
    word i, len;

    DictLookup(dict, key, &arrObj, xref);
    if (isArray(&arrObj))
    {
        len = ObjArrayGetLength(&arrObj);
        if (len > 4)
        {
            len = 4;
        }

        for (i = 0; i < len; ++i)
        {
            ObjArrayGet(&arrObj, i, &elemObj, xref);
            if (isNum(&elemObj))
            {
                coords[i] = getNum(&elemObj);
            }
            ObjFree(&elemObj);
        }
    }
    ObjFree(&arrObj);
}


/***********************************************************************
 *      GfxParseType2Function
 ***********************************************************************
 * SYNOPSIS:        Parse type2 function.
 * PARAMETERS:      Obj *funcObj    func obj
 *                  XRef *xref    cross-reference table
 *                  gdouble c0[4]    c0
 *                  gdouble c1[4]    c1
 *                  gdouble *n    count
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static GBool
GfxParseType2Function(Obj *funcObj, XRef *xref, gdouble c0[4], gdouble c1[4],
    gdouble *n)
{
    Obj typeObj, nObj;
    Dict *dict;
    word i;

    if (isDict(funcObj))
    {
        dict = getDict(funcObj);
    }
    else if (isStream(funcObj))
    {
        dict = ObjStreamGetDict(funcObj);
    }
    else
    {
        return gFalse;
    }

    DictLookup(dict, "FunctionType", &typeObj, xref);
    if (!isInt(&typeObj) || getInt(&typeObj) != 2)
    {
        ObjFree(&typeObj);
        return gFalse;
    }
    ObjFree(&typeObj);

    for (i = 0; i < 4; ++i)
    {
        c0[i] = 0.0;
        c1[i] = 1.0;
    }
    *n = 1.0;

    GfxParseCoordArray(dict, "C0", xref, c0);
    GfxParseCoordArray(dict, "C1", xref, c1);

    DictLookup(dict, "N", &nObj, xref);
    if (isNum(&nObj))
    {
        *n = getNum(&nObj);
    }
    ObjFree(&nObj);

    return gTrue;
}

/***********************************************************************
 *      GfxColorSpaceInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      GfxColorSpace *this    this
 *                  Obj *colorSpace    color space
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Detect Separation/DeviceN and try to resolve a real Type 2 tint-
 *      transform Function for the 1-colorant case;...
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

    ObjCopy(&csObj, colorSpace);
    if (isArray(colorSpace))
    {
        ObjArrayGet(colorSpace, 0, &obj, xref);
        if (isNameSame(&obj, "Separation") || isNameSame(&obj, "DeviceN"))
        {
            if (isNameSame(&obj, "Separation"))
            {
                tintComps = 1;
            }
            else
            {
                ObjArrayGet(colorSpace, 1, &obj2, xref);
                tintComps = isArray(&obj2) ? ObjArrayGetLength(&obj2) : 1;
                ObjFree(&obj2);
            }
            if (tintComps < 1 || tintComps > 4)
            {
                /* more colorants than our fixed 4-slot color arrays can hold */
                ObjFree(&obj);
                goto err1;
            }

            /*
             * Try the real tint-transform Function -- only meaningful for a
             * single input value, which covers every Separation and the
             * (fairly rare) 1-colorant DeviceN case.
             */
            if (tintComps == 1)
            {
                ObjArrayGet(colorSpace, 2, &altObj, xref);
                ObjArrayGet(colorSpace, 3, &funcObj, xref);
                if (GfxParseType2Function(&funcObj, xref, this->funcC0,
                    this->funcC1, &this->funcN))
                {
                    altCs.tintApprox = gFalse;
                    altCs.hasFunction = gFalse;
                    altCs.ok = gTrue;
                    GfxColorSpaceSetMode(&altCs, &altObj, xref);
                    if (altCs.ok)
                    {
                        this->altMode = altCs.mode;
                        this->altNumComps = altCs.numComps;
                        this->hasFunction = gTrue;
                    }
                }
                ObjFree(&funcObj);
                ObjFree(&altObj);
            }

            if (this->hasFunction)
            {
                this->indexed = gFalse;
                this->mode = colorGray; /* unused when hasFunction is set */
                this->numComps = tintComps;
            }
            else
            {
                /*
                 * Fallback: DeviceN with >1 colorant, or a Function we can't
                 * evaluate.
                 */
                ObjFree(&csObj);
                initName(&csObj, "DeviceGray");
            }
        }
        ObjFree(&obj);
    }

    if (!this->hasFunction)
    {
        this->indexed = gFalse;
        if (isName(&csObj))
        {
            GfxColorSpaceSetMode(this, &csObj, xref);
        }
        else if (isArray(&csObj))
        {
            ObjArrayGet(&csObj, 0, &obj, xref);
            if (isNameSame(&obj, "Indexed") || isNameSame(&obj, "I"))
            {
                this->indexed = gTrue;
                ObjArrayGet(&csObj, 1, &obj2, xref);
                GfxColorSpaceSetMode(this, &obj2, xref);
                ObjFree(&obj2);
            }
            else
            {
                GfxColorSpaceSetMode(this, &csObj, xref);
            }
            ObjFree(&obj);
        }
        else
        {
            goto err1;
        }
        if (!this->ok)
        {
            ObjFree(&csObj);
            return;
        }
        if (tintComps > 0)
        {
            /*
             * override the numComps GfxColorSpaceSetMode just set for our
             * placeholder DeviceGray (1) with the real colorant count
             */
            this->numComps = tintComps;
            this->tintApprox = gTrue;
        }
    }

    if (this->indexed)
    {
        ObjArrayGet(&csObj, 2, &obj, xref);
        if (!isInt(&obj))
        {
            goto err2;
        }
        this->indexHigh = getInt(&obj);
        ObjFree(&obj);
        this->lookup =
            (Guchar(*)[4])gmalloc((this->indexHigh + 1) * 4 * sizeof(Guchar));
        if (!this->lookup)
        {
            goto err1;
        }
        ObjArrayGet(&csObj, 3, &obj, xref);
        if (isStream(&obj))
        {
            ObjStreamReset(&obj);
            for (i = 0; i <= this->indexHigh; ++i)
            {
                for (j = 0; j < this->numComps; ++j)
                {
                    if ((x = ObjStreamGetChar(&obj)) == EOF)
                    {
                        goto err2;
                    }
                    this->lookup[i][j] = (Guchar)x;
                }
            }
        }
        else if (isString(&obj))
        {
            s = GStrGetCString(getString(&obj));
            for (i = 0; i <= this->indexHigh; ++i)
            {
                for (j = 0; j < this->numComps; ++j)
                {
                    this->lookup[i][j] = (Guchar)* s++;
                }
            }
        }
        else
        {
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
 * SYNOPSIS:        Release.
 * PARAMETERS:      GfxColorSpace *this    this
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
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

/***********************************************************************
 *      GfxColorSpaceSetMode
 ***********************************************************************
 * SYNOPSIS:        Set mode.
 * PARAMETERS:      GfxColorSpace *this    this
 *                  Obj *colorSpace    color space
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void GfxColorSpaceSetMode(GfxColorSpace *this, Obj *colorSpace,
    XRef *xref)
{
    Obj obj;

    if (isNameSame(colorSpace, "DeviceGray") || isNameSame(colorSpace, "G"))
    {
        this->mode = colorGray;
        this->numComps = 1;
    }
    else if (isNameSame(colorSpace, "DeviceRGB") || isNameSame(colorSpace,
        "RGB"))
    {
        this->mode = colorRGB;
        this->numComps = 3;
    }
    else if (isNameSame(colorSpace, "DeviceCMYK") || isNameSame(colorSpace,
        "CMYK"))
    {
        this->mode = colorCMYK;
        this->numComps = 4;
    }
    else if (isArray(colorSpace))
    {
        ObjArrayGet(colorSpace, 0, &obj, xref);
        if (isNameSame(&obj, "CalGray"))
        {
            this->mode = colorGray;
            this->numComps = 1;
        }
        else if (isNameSame(&obj, "CalRGB"))
        {
            this->mode = colorRGB;
            this->numComps = 3;
        }
        else if (isNameSame(&obj, "CalCMYK"))
        {
            this->mode = colorCMYK;
            this->numComps = 4;
        }
        else if (isNameSame(&obj, "ICCBased"))
        {
            Obj streamObj, nObj;
            ObjArrayGet(colorSpace, 1, &streamObj, xref);
            if (isStream(&streamObj))
            {
                DictLookup(ObjStreamGetDict(&streamObj), "N", &nObj, xref);
                if (isInt(&nObj))
                {
                    switch (getInt(&nObj))
                    {
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
                            /*
                             * /N is required to be 1, 3, or 4 -- anything else
                             * means a malformed file.
                             */
                            this->ok = gFalse;
                            break;
                    }
                }
                else
                {
                    this->ok = gFalse;
                }
                ObjFree(&nObj);
            }
            else
            {
                this->ok = gFalse;
            }
            ObjFree(&streamObj);
        }
        else
        {
            this->ok = gFalse;
        }
        ObjFree(&obj);
    }
    else
    {
        this->ok = gFalse;
    }
}

/***********************************************************************
 *      GfxEvalType2Function
 ***********************************************************************
 * SYNOPSIS:        Process gfx eval type2 function.
 * PARAMETERS:      GfxColorSpace *this    this
 *                  short tint    tint
 *                  short out[4]    out
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
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

    /* tint (0-255) as a WWFixed fraction of 1.0. */
    t = GdoubleToWWFixed(IntToGdouble((word)tint)) / 255;

    tp = t;
    nInt = (word)GdoubleToWord(this->funcN);
    if (nInt >= 2 && nInt <= 8 && this->funcN == IntToGdouble(nInt))
    {
        for (j = 1; j < nInt; ++j)
        {
            tp = GrMulWWFixed(tp, t);
        }
    }

    for (j = 0; j < this->altNumComps; ++j)
    {
        span =
            GdoubleToWWFixed(this->funcC1[j]) -
            GdoubleToWWFixed(this->funcC0[j]);
        v = GdoubleToWWFixed(this->funcC0[j]) + GrMulWWFixed(tp, span);
        v = GrMulWWFixed(v, twoFiveFive);
        out[j] = (short)GdoubleToWord(v);
        if (out[j] < 0)
        {
            out[j] = 0;
        }
        else if (out[j] > 255)
        {
            out[j] = 255;
        }
    }
}

/***********************************************************************
 *      GfxColorSpaceGetColor
 ***********************************************************************
 * SYNOPSIS:        Get color.
 * PARAMETERS:      GfxColorSpace *this    this
 *                  short x[4]    x
 *                  GfxColor *color    color
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      If a real tint-transform Function is available, evaluate it.
 *      Otherwise pass components through (Indexed via palette...
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

    if (this->hasFunction)
    {
        GfxEvalType2Function(this, x[0], y);
        switch (this->altMode)
        {
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

    if (this->indexed)
    {
        p = this->lookup[y[0]];
        switch (this->mode)
        {
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
    }
    else
    {
        switch (this->mode)
        {
            case colorGray:
                if (this->tintApprox)
                {
                    /* Average DeviceN tints and invert for gray fallback. */
                    word sum = 0;
                    short k;
                    for (k = 0; k < this->numComps; ++k)
                    {
                        sum += (word)y[k];
                    }
                    GfxColorSetGray(color,
                        255 - (short)(sum / this->numComps));
                }
                else
                {
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
 * SYNOPSIS:        Get number pixel comps.
 * PARAMETERS:      GfxColorSpace *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
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

/*
 * GfxImageColorMap
 */

/***********************************************************************
 *      GfxImageColorMapInit
 ***********************************************************************
 * SYNOPSIS:        Map init.
 * PARAMETERS:      GfxImageColorMap *this    this
 *                  short bits1    bits1
 *                  Obj *decode    decode
 *                  GfxColorSpace *colorSpace1    color space1
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Resolve /Decode (or spec defaults), detect the identity mapping
 *      fast path (8-bit, non-Indexed, default decode, no...
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

    if (isNull(decode))
    {
        if (this->colorSpace->indexed)
        {
            this->indexed = gTrue;
            this->numComps = 1;
            this->decodeLow[0] = 0;
            this->decodeRange[0] = IntToGdouble(maxPixel);
        }
        else
        {
            this->indexed = gFalse;
            this->numComps = GfxColorSpaceGetNumPixelComps(this->colorSpace);
            EC_ERROR_IF(this->numComps < 1 || this->numComps > 4, -1);
            for (i = 0; i < this->numComps; ++i)
            {
                this->decodeLow[i] = 0;
                this->decodeRange[i] = IntToGdouble(1);
            }
        }
    }
    else if (isArray(decode))
    {
        this->numComps = ObjArrayGetLength(decode) >> 1;
        if (this->numComps < 1 || this->numComps > 4)
        {
            goto err1;
        }
        if (this->numComps != GfxColorSpaceGetNumPixelComps(this->colorSpace))
        {
            goto err1;
        }
        this->indexed = this->colorSpace->indexed;
        for (i = 0; i < this->numComps; ++i)
        {
            ObjArrayGet(decode, 2 * i, &obj, xref);
            if (!isNum(&obj))
            {
                goto err2;
            }
            this->decodeLow[i] = getNum(&obj);
            ObjFree(&obj);
            ObjArrayGet(decode, 2 * i + 1, &obj, xref);
            if (!isNum(&obj))
            {
                goto err2;
            }
            this->decodeRange[i] = getNum(&obj) - this->decodeLow[i];
            ObjFree(&obj);
        }
    }
    else
    {
        goto err1;
    }

    /*
     * Identity fast-path check: must run after bits/indexed/numComps/
     * decodeLow/decodeRange are all finalized, and before the lookup table is
     * built below .
     */
    this->isIdentity = !this->indexed && this->bits == 8 &&
        !this->colorSpace->hasFunction &&
        !this->colorSpace->tintApprox;
    if (this->isIdentity)
    {
        for (i = 0; i < this->numComps; ++i)
        {
            if (this->decodeLow[i] != 0
                || this->decodeRange[i] != IntToGdouble(1))
            {
                this->isIdentity = gFalse;
                break;
            }
        }
    }

    /* construct lookup table */
    this->lookup = (short(*)[4])gmalloc((maxPixel + 1) * 4 * sizeof(short));
    if (!this->lookup)
    {
        this->ok = gFalse;
        return;
    }
    if (this->indexed)
    {
        for (i = 0; i <= maxPixel; ++i)
        {
            x[0] = i;
            GfxColorSpaceGetColor(this->colorSpace, x, &color);
            this->lookup[i][0] = color.r;
            this->lookup[i][1] = color.g;
            this->lookup[i][2] = color.b;
        }
    }
    else
    {
        for (i = 0; i <= maxPixel; ++i)
        {
            for (j = 0; j < this->numComps; ++j)
            {
                this->lookup[i][j] = GdoubleToWord(255 * (this->decodeLow[j] +
                    (i *this->decodeRange[j]) / maxPixel));
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
 * SYNOPSIS:        Map free.
 * PARAMETERS:      GfxImageColorMap *this    this
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Map get number pixel comps.
 * PARAMETERS:      GfxImageColorMap *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Map get bits.
 * PARAMETERS:      GfxImageColorMap *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Map get color.
 * PARAMETERS:      GfxImageColorMap *this    this
 *                  Guchar x[4]    x
 *                  GfxColor *color    color
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Take the isIdentity fast path when possible (raw sample used
 *      directly, no lookup). Otherwise evaluate a tint...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4],
    GfxColor *color)
{
    short *p;
    short y[4];

    if (this->isIdentity)
    {
        switch (this->mode)
        {
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

    if (this->colorSpace->hasFunction)
    {
        /*
         * real tint value, through its (usually identity) decode curve first,
         * same as every other component below
         */
        GfxEvalType2Function(this->colorSpace, this->lookup[x[0]][0], y);
        switch (this->colorSpace->altMode)
        {
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

    if (this->indexed)
    {
        p = this->lookup[x[0]];
        GfxColorSetRGB(color, p[0], p[1], p[2]);
    }
    else
    {
        switch (this->mode)
        {
            case colorGray:
                if (this->colorSpace->tintApprox)
                {
                    /* Use decoded colorants for the gray fallback. */
                    word sum = 0;
                    short k;
                    for (k = 0; k < this->numComps; ++k)
                    {
                        sum += (word)this->lookup[x[k]][k];
                    }
                    GfxColorSetGray(color,
                        255 - (short)(sum / this->numComps));
                }
                else
                {
                    GfxColorSetGray(color, this->lookup[x[0]][0]);
                }
                break;
            case colorCMYK:
                GfxColorSetCMYK(color, this->lookup[x[0]][0],
                    this->lookup[x[1]][1],
                    this->lookup[x[2]][2], this->lookup[x[3]][3]);
                break;
            case colorRGB:
                GfxColorSetRGB(color, this->lookup[x[0]][0],
                    this->lookup[x[1]][1],
                    this->lookup[x[2]][2]);
                break;
        }
    }
}

/*
 * GfxState
 */

/***********************************************************************
 *      GfxStateInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      GfxState *state    graphics state
 *                  Handle gstring    gstring
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Zero/default all text and path parameters to PDF spec defaults,
 *      including an identity text matrix. Fill/stroke...
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

    /*
     * PDF spec default: DeviceGray, color 0 (black), until a cs/CS sets
     * something else.
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

/***********************************************************************
 *      GfxStateFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      If a saved state exists, restore the GEOS transform stack via
 *      GrRestoreState() and recursively free it. (Open...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GfxStateFree(GfxState *state)
{
    if (state->saved)
    {
        GrRestoreState(state->gstring);

        GfxStateFree(state->saved);
        gfree(state->saved);
    }
}

/***********************************************************************
 *      GfxStateCopy
 ***********************************************************************
 * SYNOPSIS:        Copy.
 * PARAMETERS:      GfxState *dest    destination
 *                  GfxState *state    graphics state
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Save.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Allocate a copy of the current state and link it in as
 *      state->saved, so state itself always remains the top of the...
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
    if (!newState)
    {
        return;
    }
    GfxStateCopy(newState, state);
    newState->saved = state->saved;
    state->saved = newState;
}

/***********************************************************************
 *      GfxStateRestore
 ***********************************************************************
 * SYNOPSIS:        Process gfx state restore.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
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

    if (state->saved)
    {
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
 * SYNOPSIS:        Get font.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get font size.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get text mat.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get char space.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get word space.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get horiz scaling.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get leading.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get rise.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Get render.
 * PARAMETERS:      GfxState *state    graphics state
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
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
 * SYNOPSIS:        Set font.
 * PARAMETERS:      GfxState *state    graphics state
 *                  GfxFont *font1    font1
 *                  gdouble fontSize1    font size1
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set text mat.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble a    a
 *                  gdouble b    b
 *                  gdouble c    c
 *                  gdouble d    d
 *                  gdouble e    e
 *                  gdouble f    f
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Restore the GEOS transform stack to the pre-text-matrix state,
 *      then apply the new matrix. GEOS's TransMatrix packs...
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
    TransMatrix tm;
    sdword tmp;

    GrRestoreTransform(state->gstring);
    GrSaveTransform(state->gstring);

    tmp = GdoubleToWWFixed(a);
    tm.TM_e11.WWF_int = IntegerOf(tmp);
    tm.TM_e11.WWF_frac = FractionOf(tmp);

    tmp = GdoubleToWWFixed(b);
    tm.TM_e12.WWF_int = IntegerOf(tmp);
    tm.TM_e12.WWF_frac = FractionOf(tmp);

    tmp = GdoubleToWWFixed(c);
    tm.TM_e21.WWF_int = IntegerOf(tmp);
    tm.TM_e21.WWF_frac = FractionOf(tmp);

    tmp = GdoubleToWWFixed(d);
    tm.TM_e22.WWF_int = IntegerOf(tmp);
    tm.TM_e22.WWF_frac = FractionOf(tmp);

    tmp = GdoubleToWWFixed(e);
    tm.TM_e31.DWF_int = (sword)IntegerOf(tmp);
    tm.TM_e31.DWF_frac = FractionOf(tmp);

    tmp = GdoubleToWWFixed(f);
    tm.TM_e32.DWF_int = (sword)IntegerOf(tmp);
    tm.TM_e32.DWF_frac = FractionOf(tmp);

    GrApplyTransform(state->gstring, &tm);
}

/***********************************************************************
 *      GfxStateSetCharSpace
 ***********************************************************************
 * SYNOPSIS:        Set char space.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble space    space
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set word space.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble space    space
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set horiz scaling.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble scale    scale
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set leading.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble leading1    leading1
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set rise.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble rise1    rise1
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Set render.
 * PARAMETERS:      GfxState *state    graphics state
 *                  short render1    render1
 *
 * RETURNS:         void
 *
 * CONTEXT:
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
 * SYNOPSIS:        Move to.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble tx    tx
 *                  gdouble ty    ty
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Store the text-space coordinates, convert to WWFixed, and move
 *      the GEOS drawing position via GrMoveToWWFixed().
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
 * SYNOPSIS:        Process gfx state text shift.
 * PARAMETERS:      GfxState *state    graphics state
 *                  gdouble tx    tx
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Track the shift in curTextX and move the GEOS drawing position
 *      by the same amount, scaled from WWFixed to GEOS's...
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

