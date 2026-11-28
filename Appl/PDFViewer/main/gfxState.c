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
GBool GfxParseType2Function(Obj *funcObj, XRef *xref,
    gdouble c0[4], gdouble c1[4], gdouble *n);

/********************************************************************
 *                      GfxColorSetCMYK
 ********************************************************************
 * SYNOPSIS:      Set a color from CMYK values.
 *
 * PARAMETERS:    *this                 Ptr to the color.
 *                c                     Cyan 0-255.
 *                m                     Magenta 0-255.
 *                y                     Yellow 0-255.
 *                k                     Black 0-255.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Simple conversion: r = 255 - (c + k) and so on, not below 0.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static void GfxColorSetCMYK(GfxColor *this, short c, short m, short y, short k)
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

static void GfxColorSetGray(GfxColor *this, short gray)
{
    this->r = this->g = this->b = gray;
}

/********************************************************************
 *                      GfxColorSetRGB
 ********************************************************************
 * SYNOPSIS:      Set a color from RGB values.
 *
 * PARAMETERS:    *this                 Ptr to the color.
 *                r1                    Red 0-255.
 *                g1                    Green 0-255.
 *                b1                    Blue 0-255.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static void GfxColorSetRGB(GfxColor *this, short r1, short g1, short b1)
{
    this->r = r1;
    this->g = g1;
    this->b = b1;
}

/********************************************************************
 *                      GfxParseCoordArray
 ********************************************************************
 * SYNOPSIS:      Read up to four numbers of an array entry.
 *
 * PARAMETERS:    *dict                 Ptr to the dictionary.
 *                *key                  Ptr to the key.
 *                *xref                 Ptr to the cross-reference table.
 *                *coords               Ptr to the values (out); missing values
 *                                      stay unchanged.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void
GfxParseCoordArray(Dict *dict, const char *key, XRef *xref, gdouble coords[4])
{
    Obj arrObj, elemObj;
    word i, len;

    DictLookup(dict, (char *)key, &arrObj, xref);
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

/********************************************************************
 *                      GfxParseType2Function
 ********************************************************************
 * SYNOPSIS:      Read a type 2 (exponential) PDF function.
 *
 * PARAMETERS:    *funcObj              Ptr to the function dictionary or
 *                                      stream.
 *                *xref                 Ptr to the cross-reference table.
 *                *c0                   Ptr to C0 (out, default 0).
 *                *c1                   Ptr to C1 (out, default 1).
 *                *n                    Ptr to N (out).
 *
 * RETURNS:       GBool                 gFalse if it is not a type 2 function.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
GBool
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

/********************************************************************
 *                      GfxColorSpaceInit
 ********************************************************************
 * SYNOPSIS:      Initialize a color space from a PDF color space object.
 *
 * PARAMETERS:    *this                 Ptr to the color space.
 *                *colorSpace           Ptr to the color space object.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Separation and 1-colorant DeviceN use their tint function if
 *                it is type 2; other DeviceN spaces become a gray
 *                approximation. Indexed spaces read their lookup table. On
 *                error this->ok is gFalse.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxColorSpaceInit(GfxColorSpace *this, Obj *colorSpace, XRef *xref)
{
    Obj csObj;
    Obj obj, obj2;
    char *s;
    short x;
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

            /* Real tint function only for one input value */
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
                /* Fallback: gray approximation */
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
            /* The placeholder DeviceGray has 1 comp: use the colorant count */
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

void GfxColorSpaceFree(GfxColorSpace *this)
{
    gfree(this->lookup);
}

/********************************************************************
 *                      GfxColorSpaceSetMode
 ********************************************************************
 * SYNOPSIS:      Set the mode and the component count of a base color space.
 *
 * PARAMETERS:    *this                 Ptr to the color space.
 *                *colorSpace           Ptr to the color space object.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      CalGray, CalRGB, CalCMYK and ICCBased map to the device spaces
 *                (ICCBased by /N). Unknown spaces set this->ok to gFalse.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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
                            /* /N must be 1, 3 or 4 */
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

/********************************************************************
 *                      GfxEvalType2Function
 ********************************************************************
 * SYNOPSIS:      Evaluate the type 2 tint function of a color space.
 *
 * PARAMETERS:    *this                 Ptr to the color space.
 *                tint                  Tint 0-255.
 *                *out                  Ptr to the components of the alternate
 *                                      space (out, 0-255).
 *
 * RETURNS:       void
 *
 * STRATEGY:      WWFixed arithmetic; integer exponents 2-8 are computed, other
 *                exponents count as 1.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      GfxColorSpaceGetColor
 ********************************************************************
 * SYNOPSIS:      Convert color components to an RGB color.
 *
 * PARAMETERS:    *this                 Ptr to the color space.
 *                *x                    Ptr to the components (0-255, index for
 *                                      Indexed).
 *                *color                Ptr to the color (out).
 *
 * RETURNS:       void
 *
 * STRATEGY:      Tint function, Indexed lookup or device conversion. The gray
 *                approximation of DeviceN uses the inverted average.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

short GfxColorSpaceGetNumPixelComps(GfxColorSpace *this)
{
    return this->indexed ? 1 : this->numComps;
}

/********************************************************************
 *                      GfxImageColorMapInit
 ********************************************************************
 * SYNOPSIS:      Initialize the color map of an image.
 *
 * PARAMETERS:    *this                 Ptr to the image color map.
 *                bits1                 Bits per component.
 *                *decode               Ptr to the /Decode array or null.
 *                *colorSpace1          Ptr to the color space; the map owns it.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Make a lookup table from sample value to color component, and
 *                mark the map as identity if the samples can be used directly.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxImageColorMapInit(GfxImageColorMap *this, short bits1, Obj *decode,
    GfxColorSpace *colorSpace1, XRef *xref)
{
    GfxColor color;
    short x[4];
    short maxPixel;
    Obj obj;
    short i, j;

    this->ok = gTrue;
    /* NULL until allocated: GfxImageColorMapFree also runs after errors */
    this->lookup = NULL;

    this->bits = bits1;
    /* StreamGetImagePixel delivers 16-bit samples as their high byte */
    maxPixel = (1 << ((bits1 > 8) ? 8 : bits1)) - 1;
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

    /* Identity check: after the decode values, before the lookup table */
    this->isIdentity = !this->indexed && this->bits >= 8 &&
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

    /*
     * An identity map needs no table: GfxImageColorMapGetColor returns
     * the samples, and GfxImageRowToRGB uses the table only for indexed
     * images. This saves 2 KB and 1024 computations for each image.
     */
    if (this->isIdentity)
    {
        return;
    }

    /* Lookup table: sample value to color component (0-255) */
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
    else if (this->colorSpace->hasFunction)
    {
        /*
         * Tint function: one input value (Separation), at most 256 sample
         * values. Evaluate the function here once per sample value, not
         * for each pixel: the table holds the components of the alternate
         * space (0-255).
         */
        EC_ERROR_IF(this->numComps != 1, -1);
        for (i = 0; i <= maxPixel; ++i)
        {
            GfxEvalType2Function(this->colorSpace,
                (short)GdoubleToWord(255 * (this->decodeLow[0] +
                (i * this->decodeRange[0]) / maxPixel)),
                this->lookup[i]);
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

/********************************************************************
 *                      GfxImageColorMapFree
 ********************************************************************
 * SYNOPSIS:      Free the color map and its color space.
 *
 * PARAMETERS:    *this                 Ptr to the image color map.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxImageColorMapFree(GfxImageColorMap *this)
{
    GfxColorSpaceFree(this->colorSpace);
    gfree(this->lookup);
}

short GfxImageColorMapGetNumPixelComps(GfxImageColorMap *this)
{
    return this->numComps;
}

short GfxImageColorMapGetBits(GfxImageColorMap *this)
{
    return this->bits;
}

/********************************************************************
 *                      GfxImageColorMapGetColor
 ********************************************************************
 * SYNOPSIS:      Convert the components of one pixel to an RGB color.
 *
 * PARAMETERS:    *this                 Ptr to the image color map.
 *                *x                    Ptr to the sample values.
 *                *color                Ptr to the color (out).
 *
 * RETURNS:       void
 *
 * STRATEGY:      Identity: the samples. Otherwise from the lookup table; for
 *                a tint function the table holds the function result.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *      11/28/26  Claude/jkunze  Tint function from the lookup table
 *******************************************************************/
void GfxImageColorMapGetColor(GfxImageColorMap *this, Guchar x[4],
    GfxColor *color)
{
    short *p;

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
        /* Tint function result from the table (GfxImageColorMapInit) */
        p = this->lookup[x[0]];
        switch (this->colorSpace->altMode)
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

/********************************************************************
 *                      GfxStateInit
 ********************************************************************
 * SYNOPSIS:      Initialize a graphics state with the PDF defaults.
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *                gstring               Handle of the GString to draw into.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

    /* PDF default: DeviceGray, black */
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

    /* identity; the page box is set by GfxSetPageBox */
    state->ctm[0] = state->ctm[3] = 1L << 16;
    state->ctm[1] = state->ctm[2] = state->ctm[4] = state->ctm[5] = 0;
    state->clipBox[0] = state->clipBox[1] = -(2000L << 16);
    state->clipBox[2] = state->clipBox[3] = 2000L << 16;

    /* PDF default line width is 1.0 (same as the GEOS default) */
    state->lineWidth = IntToGdouble(1);
    state->geosLineWidth = GFX_LINE_WIDTH_UNKNOWN;
}

/********************************************************************
 *                      GfxStateFree
 ********************************************************************
 * SYNOPSIS:      Free the saved states of unclosed q levels.
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *
 * RETURNS:       void
 *
 * STRATEGY:      A loop, not a recursion: the stack is small.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxStateFree(GfxState *state)
{
    GfxState *saved, *next;

    for (saved = state->saved; saved != NULL; saved = next)
    {
        GrRestoreState(state->gstring);
        next = saved->saved;
        gfree(saved);
    }
    state->saved = NULL;
}

/********************************************************************
 *                      GfxStateCopy
 ********************************************************************
 * SYNOPSIS:      Copy a graphics state without its save chain.
 *
 * PARAMETERS:    *dest                 Ptr to the copy.
 *                *state                Ptr to the graphics state.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static void GfxStateCopy(GfxState *dest, GfxState *state)
{
    memcpy(dest, state, sizeof(GfxState));
    dest->saved = NULL;
}

/********************************************************************
 *                      GfxStateSave
 ********************************************************************
 * SYNOPSIS:      Save the graphics state (q).
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The copy goes into a heap block at the head of the save chain.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      GfxStateRestore
 ********************************************************************
 * SYNOPSIS:      Restore the last saved graphics state (Q).
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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
    else
    {
        /*
         * No saved state (GfxStateSave had no memory): GrRestoreState of
         * the caller still restores the GEOS line width. Thus the width
         * that we remember is not known to be correct.
         */
        state->geosLineWidth = GFX_LINE_WIDTH_UNKNOWN;
    }
}

GfxFont *GfxStateGetFont(GfxState *state)
{
    return state->font;
}

gdouble GfxStateGetFontSize(GfxState *state)
{
    return state->fontSize;
}

sdword GfxStateGetCharSpace(GfxState *state)
{
    return state->charSpace;
}

gdouble GfxStateGetWordSpace(GfxState *state)
{
    return state->wordSpace;
}

sdword GfxStateGetHorizScaling(GfxState *state)
{
    return state->horizScaling;
}

gdouble GfxStateGetLeading(GfxState *state)
{
    return state->leading;
}

sdword GfxStateGetRise(GfxState *state)
{
    return state->rise;
}

short GfxStateGetRender(GfxState *state)
{
    return state->render;
}

/********************************************************************
 *                      GfxStateSetFont
 ********************************************************************
 * SYNOPSIS:      Set the font and the font size.
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *                *font1                Ptr to the font.
 *                fontSize1             Font size.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxStateSetFont(GfxState *state, GfxFont *font1, gdouble fontSize1)
{
    state->font = font1;
    state->fontSize = fontSize1;
}

/********************************************************************
 *                      GfxStateSetTextMat
 ********************************************************************
 * SYNOPSIS:      Set the text matrix (Tm).
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *                a                     Matrix a.
 *                b                     Matrix b.
 *                c                     Matrix c.
 *                d                     Matrix d.
 *                e                     Matrix e (x).
 *                f                     Matrix f (y).
 *
 * RETURNS:       void
 *
 * STRATEGY:      Go back to the saved transform of the text object and apply
 *                the new matrix to the GString.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxStateSetTextMat(GfxState *state, gdouble a, gdouble b, gdouble c,
    gdouble d, gdouble e, gdouble f)
{
    TransMatrix tm;
    sdword tmp;

    /* Back to the transform of the text object, then the new matrix */
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

void GfxStateSetCharSpace(GfxState *state, gdouble space)
{
    state->charSpace = GdoubleToWWFixed(space);
}

void GfxStateSetWordSpace(GfxState *state, gdouble space)
{
    state->wordSpace = space;
}

void GfxStateSetHorizScaling(GfxState *state, gdouble scale)
{
    state->horizScaling = GdoubleToWWFixed(0.01 * scale);
}

void GfxStateSetLeading(GfxState *state, gdouble leading1)
{
    state->leading = leading1;
}

void GfxStateSetRise(GfxState *state, gdouble rise1)
{
    state->rise = GdoubleToWWFixed(rise1);
}

void GfxStateSetRender(GfxState *state, short render1)
{
    state->render = render1;
}

/********************************************************************
 *                      GfxStateTextMoveTo
 ********************************************************************
 * SYNOPSIS:      Move to the start of a text line.
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *                tx                    x in text space.
 *                ty                    y in text space.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      GfxStateTextShift
 ********************************************************************
 * SYNOPSIS:      Move the text position to the right.
 *
 * PARAMETERS:    *state                Ptr to the graphics state.
 *                tx                    Distance in text space.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GfxStateTextShift(GfxState *state, gdouble tx)
{
    state->curTextX += GdoubleToWWFixed(tx);
    GrRelMoveTo(state->gstring, ((sdword)((dword)GdoubleToWWFixed(tx) << 5)), 0);
}

/***********************************************************************
 *      Matrices
 ***********************************************************************
 * 2x3 matrices in PDF order [a b c d e f] (p' = p x M), 16.16 fixed.
 * Used to follow the CTM (clip box, replacement fill of 'sh').
 * (Moved here from main/shading.c, which was removed in I1.)
 ***********************************************************************/

/********************************************************************
 *                      GfxMatMul
 ********************************************************************
 * SYNOPSIS:      Multiply two matrices (first a, then b).
 *
 * PARAMETERS:    *a                    Ptr to the first matrix.
 *                *b                    Ptr to the second matrix.
 *                *out                  Ptr to the result; must not be a or b.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void GfxMatMul(const gdouble *a, const gdouble *b, gdouble *out)
{
    out[0] = GrMulWWFixed(a[0], b[0]) + GrMulWWFixed(a[1], b[2]);
    out[1] = GrMulWWFixed(a[0], b[1]) + GrMulWWFixed(a[1], b[3]);
    out[2] = GrMulWWFixed(a[2], b[0]) + GrMulWWFixed(a[3], b[2]);
    out[3] = GrMulWWFixed(a[2], b[1]) + GrMulWWFixed(a[3], b[3]);
    out[4] = GrMulWWFixed(a[4], b[0]) + GrMulWWFixed(a[5], b[2]) + b[4];
    out[5] = GrMulWWFixed(a[4], b[1]) + GrMulWWFixed(a[5], b[3]) + b[5];
}

static gdouble GfxMatAbs(gdouble x)
{
    return (x < 0) ? -x : x;
}

/********************************************************************
 *                      GfxMatInvert
 ********************************************************************
 * SYNOPSIS:      Invert a matrix.
 *
 * PARAMETERS:    *m                    Ptr to the matrix.
 *                *out                  Ptr to the inverse (out).
 *
 * RETURNS:       GBool                 gFalse if the matrix is degenerate.
 *
 * STRATEGY:      The determinant of entries around 200 overflows 16.16:
 *                normalize the linear part by its largest entry first.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
GBool GfxMatInvert(const gdouble *m, gdouble *out)
{
    gdouble s, a, b, c, d, det, ds;
    word i;

    s = 0;
    for (i = 0; i < 4; ++i)
    {
        if (GfxMatAbs(m[i]) > s)
        {
            s = GfxMatAbs(m[i]);
        }
    }
    if (s == 0)
    {
        return gFalse;
    }
    a = GrSDivWWFixed(m[0], s);
    b = GrSDivWWFixed(m[1], s);
    c = GrSDivWWFixed(m[2], s);
    d = GrSDivWWFixed(m[3], s);
    det = GrMulWWFixed(a, d) - GrMulWWFixed(b, c);
    if (GfxMatAbs(det) < 16)
    {
        return gFalse;          /* degenerate */
    }
    ds = GrMulWWFixed(det, s);
    if (ds == 0)
    {
        return gFalse;
    }
    out[0] = GrSDivWWFixed(d, ds);
    out[1] = -GrSDivWWFixed(b, ds);
    out[2] = -GrSDivWWFixed(c, ds);
    out[3] = GrSDivWWFixed(a, ds);
    out[4] = -(GrMulWWFixed(m[4], out[0]) + GrMulWWFixed(m[5], out[2]));
    out[5] = -(GrMulWWFixed(m[4], out[1]) + GrMulWWFixed(m[5], out[3]));
    return gTrue;
}

/********************************************************************
 *                      GfxMatApply
 ********************************************************************
 * SYNOPSIS:      Transform a point with a matrix.
 *
 * PARAMETERS:    *m                    Ptr to the matrix.
 *                x                     x.
 *                y                     y.
 *                *ox                   Ptr to the new x (out).
 *                *oy                   Ptr to the new y (out).
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void GfxMatApply(const gdouble *m, gdouble x, gdouble y, gdouble *ox,
    gdouble *oy)
{
    *ox = GrMulWWFixed(x, m[0]) + GrMulWWFixed(y, m[2]) + m[4];
    *oy = GrMulWWFixed(x, m[1]) + GrMulWWFixed(y, m[3]) + m[5];
}

/********************************************************************
 *                      GfxMatApplyBox
 ********************************************************************
 * SYNOPSIS:      Return the bounding box of a transformed box.
 *
 * PARAMETERS:    *m                    Ptr to the matrix.
 *                *box                  Ptr to the box [x0 y0 x1 y1].
 *                *out                  Ptr to the result (out); may be box.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void GfxMatApplyBox(const gdouble *m, const gdouble *box, gdouble *out)
{
    gdouble x, y, b[4];
    word i;

    memcpy(b, box, sizeof(b));
    for (i = 0; i < 4; ++i)
    {
        GfxMatApply(m, b[(i & 1) ? 2 : 0], b[(i & 2) ? 3 : 1], &x, &y);
        if (i == 0 || x < out[0])
        {
            out[0] = x;
        }
        if (i == 0 || x > out[2])
        {
            out[2] = x;
        }
        if (i == 0 || y < out[1])
        {
            out[1] = y;
        }
        if (i == 0 || y > out[3])
        {
            out[3] = y;
        }
    }
}
