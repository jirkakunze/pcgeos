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
 * FILE:          main/function.c
 *
 * DESCRIPTION:
 *      PDF functions (PDF 1.7, 7.10): type 0 (sampled, multilinear
 *      interpolation), 2 (exponential) and 3 (stitching). Used by the
 *      shadings (main/shading.c). Type 4 (PostScript calculator) is left
 *      out (code size): FuncParse fails, the shading is not drawn.
 *
 *      Everything is 16.16 fixed point (gdouble = WWFixed), like the rest
 *      of the renderer: function values are colors and parameters, the
 *      precision is plenty. exp/ln are computed here (range reduction +
 *      short series), sqrt/sin/cos come from the graphics system.
 *
 *      Memory: the function tree is small (gmalloc); sample tables live
 *      in movable blocks that are locked while the function exists --
 *      functions only live while one shading is drawn.
 ***********************************************************************/

#include <geos.h>
#include <heap.h>
#include <ec.h>
#include <graphics.h>
#include <Ansi/string.h>
#include "obj.h"
#include "array.h"
#include "dict.h"
#include "stream.h"
#include "function.h"
#include "gmem.h"

#ifdef __WATCOMC__
#pragma code_seg("PdfFunc")
#endif

#define FIX_ONE         0x10000L
#define FIX(i)          ((gdouble)(i) << 16)
#define FIX_LN2         45426L          /* 0.693147 */
#define FIX_MAX         0x7FFFFFFFL

/* largest sample table (16 bit entries) and PostScript program */
#define FUNC_MAX_SAMPLES 16384
#define FUNC_MAX_STITCH 32

/***********************************************************************
 *      Data structures
 ***********************************************************************/

typedef struct PdfFuncImpl PdfFuncImpl;

struct PdfFuncImpl
{
    byte type;
    byte m;                         /* inputs */
    byte n;                         /* outputs */
    GBool hasRange;
    gdouble domain[2 * FUNC_MAX_IN];
    gdouble range[2 * FUNC_MAX_OUT];
    union
    {
        struct
        {
            gdouble c0[FUNC_MAX_OUT];
            gdouble c1[FUNC_MAX_OUT];
            gdouble N;
        } ex;
        struct
        {
            word k;
            PdfFuncImpl **funcs;
            gdouble *bounds;        /* k - 1 */
            gdouble *encode;        /* 2 k */
        } st;
        struct
        {
            MemHandle han;
            word *samples;          /* locked, normalized to 0..65535 */
            word size[FUNC_MAX_IN];
            gdouble encode[2 * FUNC_MAX_IN];
            gdouble decode[2 * FUNC_MAX_OUT];
        } sm;
    } u;
};

/***********************************************************************
 *      Fixed point helpers
 ***********************************************************************/

static gdouble FClip(gdouble x, gdouble lo, gdouble hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

/* y0 + (x - x0) * (y1 - y0) / (x1 - x0) */
static gdouble FInterp(gdouble x, gdouble x0, gdouble x1, gdouble y0,
    gdouble y1)
{
    if (x1 == x0)
    {
        return y0;
    }
    return y0 + GrMulWWFixed(GrSDivWWFixed(x - x0, x1 - x0), y1 - y0);
}

/* natural logarithm, x > 0 */
static gdouble FuncFixedLn(gdouble x)
{
    sword k = 0;
    gdouble y, y2, s, t;

    if (x <= 0)
    {
        return -FIX(32000);
    }
    while (x >= FIX(2))
    {
        x >>= 1;
        ++k;
    }
    while (x < FIX_ONE)
    {
        x <<= 1;
        --k;
    }
    /* ln m = 2 (y + y^3/3 + y^5/5 + y^7/7), y = (m-1)/(m+1) <= 1/3 */
    y = GrSDivWWFixed(x - FIX_ONE, x + FIX_ONE);
    y2 = GrMulWWFixed(y, y);
    t = y;
    s = y;
    t = GrMulWWFixed(t, y2);
    s += t / 3;
    t = GrMulWWFixed(t, y2);
    s += t / 5;
    t = GrMulWWFixed(t, y2);
    s += t / 7;
    return 2 * s + (gdouble)k * FIX_LN2;
}

/* e^x, saturating */
static gdouble FuncFixedExp(gdouble x)
{
    sword k;
    gdouble r, t, s;

    if (x > 681000L)            /* ~10.39: e^x > 32767 */
    {
        return FIX_MAX;
    }
    if (x < -727000L)           /* ~-11.1: below 1/65536 */
    {
        return 0;
    }
    k = (sword)(x / FIX_LN2);
    if (x < 0 && (gdouble)k * FIX_LN2 != x)
    {
        --k;
    }
    r = x - (gdouble)k * FIX_LN2;        /* 0 <= r < ln 2 */
    s = FIX_ONE + r;
    t = GrMulWWFixed(r, r) / 2;
    s += t;
    t = GrMulWWFixed(t, r) / 3;
    s += t;
    t = GrMulWWFixed(t, r) / 4;
    s += t;
    t = GrMulWWFixed(t, r) / 5;
    s += t;
    if (k >= 0)
    {
        while (k-- > 0)
        {
            if (s > FIX_MAX / 2)
            {
                return FIX_MAX;
            }
            s <<= 1;
        }
        return s;
    }
    return (k <= -31) ? 0 : (s >> (-k));
}

/* base ^ e */
static gdouble FPow(gdouble base, gdouble e)
{
    sword n, i;
    gdouble r;
    GBool neg;

    if ((e & 0xFFFFL) == 0 && e >= -FIX(16) && e <= FIX(16))
    {
        n = (sword)(e >> 16);
        neg = (n < 0);
        if (neg)
        {
            n = -n;
        }
        r = FIX_ONE;
        for (i = 0; i < n; ++i)
        {
            r = GrMulWWFixed(r, base);
        }
        if (neg)
        {
            r = (r == 0) ? FIX_MAX : GrSDivWWFixed(FIX_ONE, r);
        }
        return r;
    }
    if (base <= 0)
    {
        return 0;
    }
    return FuncFixedExp(GrMulWWFixed(e, FuncFixedLn(base)));
}

/* atan2 in degrees, 0 <= result < 360 (PostScript atan num den) */
/***********************************************************************
 *      Reading arrays from the dictionary
 ***********************************************************************/

static word FuncReadArray(Dict *dict, char *key, gdouble *out, word max,
    XRef *xref)
{
    Obj arr, elem;
    word n, i, got = 0;

    DictLookup(dict, key, &arr, xref);
    if (isArray(&arr))
    {
        n = ObjArrayGetLength(&arr);
        for (i = 0; i < n && got < max; ++i)
        {
            ObjArrayGet(&arr, i, &elem, xref);
            if (isNum(&elem))
            {
                out[got++] = getNum(&elem);
            }
            ObjFree(&elem);
        }
    }
    else if (isNum(&arr) && max > 0)
    {
        out[got++] = getNum(&arr);      /* /FunctionType 2, /N 1, ... */
    }
    ObjFree(&arr);
    return got;
}

/***********************************************************************
 *      Type 0 (sampled)
 ***********************************************************************/

static GBool FuncInitSampled(PdfFuncImpl *f, Obj *obj, Dict *dict,
    XRef *xref)
{
    gdouble tmp[FUNC_MAX_IN];
    word i, bps, count;
    dword total, maxVal, v;
    long c;
    Stream *str;
    word bitBuf = 0;
    sword bits = 0;
    word j, b;

    if (!isStream(obj) || !f->hasRange)
    {
        return gFalse;
    }
    if (FuncReadArray(dict, "Size", tmp, FUNC_MAX_IN, xref) < f->m)
    {
        return gFalse;
    }
    total = f->n;
    for (i = 0; i < f->m; ++i)
    {
        f->u.sm.size[i] = (word)(tmp[i] >> 16);
        if (f->u.sm.size[i] < 1)
        {
            return gFalse;
        }
        total *= f->u.sm.size[i];
        if (total > FUNC_MAX_SAMPLES)
        {
            EC_WARNING(-1);
            return gFalse;
        }
    }
    if (FuncReadArray(dict, "BitsPerSample", tmp, 1, xref) < 1)
    {
        return gFalse;
    }
    bps = (word)(tmp[0] >> 16);
    if (bps != 1 && bps != 2 && bps != 4 && bps != 8 && bps != 12 &&
        bps != 16 && bps != 24 && bps != 32)
    {
        return gFalse;
    }
    if (FuncReadArray(dict, "Encode", f->u.sm.encode, 2 * f->m, xref) <
        2 * f->m)
    {
        for (i = 0; i < f->m; ++i)
        {
            f->u.sm.encode[2 * i] = 0;
            f->u.sm.encode[2 * i + 1] = FIX(f->u.sm.size[i] - 1);
        }
    }
    if (FuncReadArray(dict, "Decode", f->u.sm.decode, 2 * f->n, xref) <
        2 * f->n)
    {
        memcpy(f->u.sm.decode, f->range, 2 * f->n * sizeof(gdouble));
    }

    f->u.sm.han = MemAlloc((word)total * sizeof(word), HF_SWAPABLE,
        HAF_NO_ERR);
    if (f->u.sm.han == NullHandle)
    {
        GMemSetError();
        return gFalse;
    }
    f->u.sm.samples = MemLock(f->u.sm.han);

    /* samples, big-endian bit packing, normalized to 0..65535 */
    str = getStream(obj);
    StreamReset(str);
    maxVal = (bps >= 32) ? 0xFFFFFFFFUL : ((1UL << bps) - 1);
    count = (word)total;
    for (j = 0; j < count; ++j)
    {
        v = 0;
        for (b = bps; b > 0; )
        {
            if (bits == 0)
            {
                c = StreamGetChar(str);
                bitBuf = (c == EOF) ? 0 : (word)c;
                bits = 8;
            }
            if (b >= (word)bits)
            {
                v = (v << bits) | (bitBuf & ((1 << bits) - 1));
                b -= bits;
                bits = 0;
            }
            else
            {
                v = (v << b) | ((bitBuf >> (bits - b)) & ((1 << b) - 1));
                bits -= b;
                b = 0;
            }
        }
        if (bps > 16)
        {
            f->u.sm.samples[j] = (word)(v >> (bps - 16));
        }
        else if (bps == 16)
        {
            f->u.sm.samples[j] = (word)v;
        }
        else
        {
            f->u.sm.samples[j] = (word)((v * 65535UL) / maxVal);
        }
    }
    return gTrue;
}

static void FuncEvalSampled(PdfFuncImpl *f, const gdouble *in, gdouble *out)
{
    word idx[FUNC_MAX_IN];
    gdouble frac[FUNC_MAX_IN];
    dword stride[FUNC_MAX_IN];
    gdouble e, w, acc;
    word i, j, corner, corners;
    dword base, off;
    word s;

    stride[0] = f->n;
    for (i = 1; i < f->m; ++i)
    {
        stride[i] = stride[i - 1] * f->u.sm.size[i - 1];
    }
    base = 0;
    for (i = 0; i < f->m; ++i)
    {
        e = FClip(in[i], f->domain[2 * i], f->domain[2 * i + 1]);
        e = FInterp(e, f->domain[2 * i], f->domain[2 * i + 1],
            f->u.sm.encode[2 * i], f->u.sm.encode[2 * i + 1]);
        e = FClip(e, 0, FIX(f->u.sm.size[i] - 1));
        idx[i] = (word)(e >> 16);
        frac[i] = e & 0xFFFFL;
        if (idx[i] >= f->u.sm.size[i] - 1)
        {
            idx[i] = f->u.sm.size[i] - 1;
            frac[i] = 0;
        }
        base += idx[i] * stride[i];
    }

    corners = 1 << f->m;
    for (j = 0; j < f->n; ++j)
    {
        acc = 0;
        for (corner = 0; corner < corners; ++corner)
        {
            w = FIX_ONE;
            off = base + j;
            for (i = 0; i < f->m && w; ++i)
            {
                if (corner & (1 << i))
                {
                    w = GrMulWWFixed(w, frac[i]);
                    off += stride[i];
                }
                else
                {
                    w = GrMulWWFixed(w, FIX_ONE - frac[i]);
                }
            }
            if (w)
            {
                s = f->u.sm.samples[off];
                acc += GrMulWWFixed(w, (gdouble)s + (s >> 15));
            }
        }
        out[j] = f->u.sm.decode[2 * j] +
            GrMulWWFixed(acc, f->u.sm.decode[2 * j + 1] - f->u.sm.decode[2 * j]);
    }
}

/***********************************************************************
 *      Type 2 (exponential)
 ***********************************************************************/

static GBool FuncInitExp(PdfFuncImpl *f, Dict *dict, XRef *xref)
{
    word n0, n1;
    gdouble tmp[1];

    if (f->m != 1)
    {
        return gFalse;
    }
    n0 = FuncReadArray(dict, "C0", f->u.ex.c0, FUNC_MAX_OUT, xref);
    n1 = FuncReadArray(dict, "C1", f->u.ex.c1, FUNC_MAX_OUT, xref);
    if (n0 == 0 && n1 == 0)
    {
        n0 = n1 = 1;
        f->u.ex.c0[0] = 0;
        f->u.ex.c1[0] = FIX_ONE;
    }
    if (n0 != n1)
    {
        return gFalse;
    }
    if (FuncReadArray(dict, "N", tmp, 1, xref) < 1)
    {
        return gFalse;
    }
    f->u.ex.N = tmp[0];
    if (f->hasRange && f->n != n0)
    {
        return gFalse;
    }
    f->n = (byte)n0;
    return gTrue;
}

static void FuncEvalExp(PdfFuncImpl *f, const gdouble *in, gdouble *out)
{
    gdouble x, xn;
    word j;

    x = FClip(in[0], f->domain[0], f->domain[1]);
    xn = FPow(x, f->u.ex.N);
    for (j = 0; j < f->n; ++j)
    {
        out[j] = f->u.ex.c0[j] + GrMulWWFixed(xn, f->u.ex.c1[j] - f->u.ex.c0[j]);
    }
}

/***********************************************************************
 *      Type 3 (stitching)
 ***********************************************************************/

static GBool FuncInitStitch(PdfFuncImpl *f, Dict *dict, XRef *xref)
{
    Obj arr, elem;
    word k, i;

    if (f->m != 1)
    {
        return gFalse;
    }
    DictLookup(dict, "Functions", &arr, xref);
    if (!isArray(&arr) || ObjArrayGetLength(&arr) < 1 ||
        ObjArrayGetLength(&arr) > FUNC_MAX_STITCH)
    {
        ObjFree(&arr);
        return gFalse;
    }
    k = ObjArrayGetLength(&arr);
    f->u.st.funcs = gmalloc(k * sizeof(PdfFuncImpl *));
    f->u.st.bounds = gmalloc((k + 1) * sizeof(gdouble));
    f->u.st.encode = gmalloc(2 * k * sizeof(gdouble));
    if (!f->u.st.funcs || !f->u.st.bounds || !f->u.st.encode)
    {
        ObjFree(&arr);
        return gFalse;
    }
    for (i = 0; i < k; ++i)
    {
        f->u.st.funcs[i] = NULL;
    }
    f->u.st.k = k;
    for (i = 0; i < k; ++i)
    {
        ObjArrayGet(&arr, i, &elem, xref);
        f->u.st.funcs[i] = FuncParse(&elem, xref);
        ObjFree(&elem);
        if (!f->u.st.funcs[i] || f->u.st.funcs[i]->m != 1 ||
            (i > 0 && f->u.st.funcs[i]->n != f->u.st.funcs[0]->n))
        {
            ObjFree(&arr);
            return gFalse;
        }
    }
    ObjFree(&arr);
    if (k > 1 &&
        FuncReadArray(dict, "Bounds", f->u.st.bounds, k - 1, xref) < k - 1)
    {
        return gFalse;
    }
    if (FuncReadArray(dict, "Encode", f->u.st.encode, 2 * k, xref) < 2 * k)
    {
        return gFalse;
    }
    if (f->hasRange && f->n != f->u.st.funcs[0]->n)
    {
        return gFalse;
    }
    f->n = f->u.st.funcs[0]->n;
    return gTrue;
}

static void FuncEvalStitch(PdfFuncImpl *f, const gdouble *in, gdouble *out)
{
    gdouble x, lo, hi;
    word i, k = f->u.st.k;

    x = FClip(in[0], f->domain[0], f->domain[1]);
    for (i = 0; i < k - 1; ++i)
    {
        if (x < f->u.st.bounds[i])
        {
            break;
        }
    }
    lo = (i == 0) ? f->domain[0] : f->u.st.bounds[i - 1];
    hi = (i == k - 1) ? f->domain[1] : f->u.st.bounds[i];
    x = FInterp(x, lo, hi, f->u.st.encode[2 * i], f->u.st.encode[2 * i + 1]);
    FuncEval(f->u.st.funcs[i], &x, out);
}

/***********************************************************************
 *      Public interface
 ***********************************************************************/

PdfFunction *FuncParse(Obj *obj, XRef *xref)
{
    Dict *dict;
    PdfFuncImpl *f;
    gdouble tmp[2 * FUNC_MAX_OUT];
    word cnt;
    GBool ok = gFalse;

    if (isStream(obj))
    {
        dict = ObjStreamGetDict(obj);
    }
    else if (isDict(obj))
    {
        dict = getDict(obj);
    }
    else
    {
        return NULL;
    }

    f = gmalloc(sizeof(PdfFuncImpl));
    if (!f)
    {
        return NULL;
    }
    memset(f, 0, sizeof(PdfFuncImpl));

    if (FuncReadArray(dict, "FunctionType", tmp, 1, xref) < 1)
    {
        gfree(f);
        return NULL;
    }
    f->type = (byte)(tmp[0] >> 16);
    f->type = (f->type <= 3) ? f->type : 0xFF;    /* no type 4 */

    cnt = FuncReadArray(dict, "Domain", f->domain, 2 * FUNC_MAX_IN, xref);
    f->m = (byte)(cnt / 2);
    cnt = FuncReadArray(dict, "Range", f->range, 2 * FUNC_MAX_OUT, xref);
    f->hasRange = (cnt >= 2);
    f->n = (byte)(cnt / 2);

    if (f->m >= 1)
    {
        switch (f->type)
        {
            case 0:
                ok = FuncInitSampled(f, obj, dict, xref);
                break;
            case 2:
                ok = FuncInitExp(f, dict, xref);
                break;
            case 3:
                ok = FuncInitStitch(f, dict, xref);
                break;
        }
    }
    if (!ok || f->n < 1 || f->n > FUNC_MAX_OUT)
    {
        EC_WARNING(-1);
        FuncFree(f);
        return NULL;
    }
    return f;
}

void FuncFree(PdfFunction *fv)
{
    PdfFuncImpl *f = fv;
    word i;

    if (!f)
    {
        return;
    }
    switch (f->type)
    {
        case 0:
            if (f->u.sm.han)
            {
                MemUnlock(f->u.sm.han);
                MemFree(f->u.sm.han);
            }
            break;
        case 3:
            if (f->u.st.funcs)
            {
                for (i = 0; i < f->u.st.k; ++i)
                {
                    FuncFree(f->u.st.funcs[i]);
                }
                gfree(f->u.st.funcs);
            }
            gfree(f->u.st.bounds);
            gfree(f->u.st.encode);
            break;
    }
    gfree(f);
}

word FuncGetInputs(PdfFunction *fv)
{
    PdfFuncImpl *f = fv;

    return f->m;
}

word FuncGetOutputs(PdfFunction *fv)
{
    PdfFuncImpl *f = fv;

    return f->n;
}

void FuncEval(PdfFunction *fv, const gdouble *in, gdouble *out)
{
    PdfFuncImpl *f = fv;
    word j;

    switch (f->type)
    {
        case 0:
            FuncEvalSampled(f, in, out);
            break;
        case 2:
            FuncEvalExp(f, in, out);
            break;
        case 3:
            FuncEvalStitch(f, in, out);
            break;
    }
    if (f->hasRange)
    {
        for (j = 0; j < f->n; ++j)
        {
            out[j] = FClip(out[j], f->range[2 * j], f->range[2 * j + 1]);
        }
    }
}
