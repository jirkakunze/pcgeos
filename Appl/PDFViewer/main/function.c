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
 *      interpolation), 2 (exponential), 3 (stitching) and 4 (PostScript
 *      calculator). Used by the shadings (main/shading.c).
 *
 *      Everything is 16.16 fixed point (gdouble = WWFixed), like the rest
 *      of the renderer: function values are colors and parameters, the
 *      precision is plenty. exp/ln are computed here (range reduction +
 *      short series), sqrt/sin/cos come from the graphics system.
 *
 *      Memory: the function tree is small (gmalloc); sample tables and
 *      compiled PostScript code live in movable blocks that are locked
 *      while the function exists -- functions only live while one
 *      shading is drawn.
 *
 *      Type 4 operators are recognized by a numeric key (first four
 *      characters and the length) instead of a name table, so no table
 *      ends up in dgroup.
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
#define FIX_LN10        150902L         /* 2.302585 */
#define FIX_MAX         0x7FFFFFFFL

/* largest sample table (16 bit entries) and PostScript program */
#define FUNC_MAX_SAMPLES 16384
#define PS_MAX_OPS      4000
#define PS_STACK_SIZE   100
#define FUNC_MAX_STITCH 32

/***********************************************************************
 *      Data structures
 ***********************************************************************/

typedef struct
{
    byte op;
    sdword val;
} PsOp;

typedef struct
{
    sdword v;
    byte t;         /* PS_T_xxx */
} PsVal;

#define PS_T_INT  0
#define PS_T_REAL 1
#define PS_T_BOOL 2

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
        struct
        {
            MemHandle han;
            PsOp *code;             /* locked */
            word len;
            PsVal *stack;
        } ps;
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
gdouble FuncFixedLn(gdouble x)
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
gdouble FuncFixedExp(gdouble x)
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
static gdouble FAtan(gdouble num, gdouble den)
{
    gdouble a = (den < 0) ? -den : den;
    gdouble b = (num < 0) ? -num : num;
    gdouble z, ang;

    if (a == 0 && b == 0)
    {
        return 0;
    }
    /* atan(z), 0 <= z <= 1: 45 z - z (z - 1)(14.02 + 3.79 z), degrees */
    z = (b <= a) ? GrSDivWWFixed(b, a) : GrSDivWWFixed(a, b);
    ang = 45 * z - GrMulWWFixed(GrMulWWFixed(z, z - FIX_ONE),
        918815L + GrMulWWFixed(248381L, z));
    if (b > a)
    {
        ang = FIX(90) - ang;
    }
    if (den < 0)
    {
        ang = (num < 0) ? FIX(180) + ang : FIX(180) - ang;
    }
    else if (num < 0)
    {
        ang = FIX(360) - ang;
    }
    return ang;
}

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
 *      Type 4 (PostScript calculator)
 ***********************************************************************/

enum
{
    PS_PUSH_INT, PS_PUSH_REAL, PS_JZ, PS_JMP,
    PS_ABS, PS_ADD, PS_AND, PS_ATAN, PS_BITSHIFT, PS_CEILING, PS_COPY,
    PS_COS, PS_CVI, PS_CVR, PS_DIV, PS_DUP, PS_EQ, PS_EXCH, PS_EXP,
    PS_FALSE, PS_FLOOR, PS_GE, PS_GT, PS_IDIV, PS_INDEX, PS_LE, PS_LN,
    PS_LOG, PS_LT, PS_MOD, PS_MUL, PS_NE, PS_NEG, PS_NOT, PS_OR, PS_POP,
    PS_ROLL, PS_ROUND, PS_SIN, PS_SQRT, PS_SUB, PS_TRUE, PS_TRUNCATE,
    PS_XOR, PS_IF, PS_IFELSE,
    PS_BAD
};

#define K4(a, b, c, d, len) (((dword)(a) << 24) | ((dword)(b) << 16) | \
                             ((dword)(c) << 8) | (dword)(d) | ((dword)(len) << 29))

static byte PsLookupOp(char *name)
{
    word len = strlen(name);
    dword key;

    if (len < 2 || len > 8)
    {
        return PS_BAD;
    }
    key = K4(name[0], name[1], (len > 2) ? name[2] : 0,
        (len > 3) ? name[3] : 0, len);
    switch (key)
    {
        case K4('a','b','s',0,3):         return PS_ABS;
        case K4('a','d','d',0,3):         return PS_ADD;
        case K4('a','n','d',0,3):         return PS_AND;
        case K4('a','t','a','n',4):       return PS_ATAN;
        case K4('b','i','t','s',8):       return PS_BITSHIFT;
        case K4('c','e','i','l',7):       return PS_CEILING;
        case K4('c','o','p','y',4):       return PS_COPY;
        case K4('c','o','s',0,3):         return PS_COS;
        case K4('c','v','i',0,3):         return PS_CVI;
        case K4('c','v','r',0,3):         return PS_CVR;
        case K4('d','i','v',0,3):         return PS_DIV;
        case K4('d','u','p',0,3):         return PS_DUP;
        case K4('e','q',0,0,2):           return PS_EQ;
        case K4('e','x','c','h',4):       return PS_EXCH;
        case K4('e','x','p',0,3):         return PS_EXP;
        case K4('f','a','l','s',5):       return PS_FALSE;
        case K4('f','l','o','o',5):       return PS_FLOOR;
        case K4('g','e',0,0,2):           return PS_GE;
        case K4('g','t',0,0,2):           return PS_GT;
        case K4('i','d','i','v',4):       return PS_IDIV;
        case K4('i','n','d','e',5):       return PS_INDEX;
        case K4('l','e',0,0,2):           return PS_LE;
        case K4('l','n',0,0,2):           return PS_LN;
        case K4('l','o','g',0,3):         return PS_LOG;
        case K4('l','t',0,0,2):           return PS_LT;
        case K4('m','o','d',0,3):         return PS_MOD;
        case K4('m','u','l',0,3):         return PS_MUL;
        case K4('n','e',0,0,2):           return PS_NE;
        case K4('n','e','g',0,3):         return PS_NEG;
        case K4('n','o','t',0,3):         return PS_NOT;
        case K4('o','r',0,0,2):           return PS_OR;
        case K4('p','o','p',0,3):         return PS_POP;
        case K4('r','o','l','l',4):       return PS_ROLL;
        case K4('r','o','u','n',5):       return PS_ROUND;
        case K4('s','i','n',0,3):         return PS_SIN;
        case K4('s','q','r','t',4):       return PS_SQRT;
        case K4('s','u','b',0,3):         return PS_SUB;
        case K4('t','r','u','e',4):       return PS_TRUE;
        case K4('t','r','u','n',8):       return PS_TRUNCATE;
        case K4('x','o','r',0,3):         return PS_XOR;
        case K4('i','f',0,0,2):           return PS_IF;
        case K4('i','f','e','l',6):       return PS_IFELSE;
    }
    return PS_BAD;
}

typedef struct
{
    Stream *str;
    long look;              /* look-ahead char, -2 = none */
    PdfFuncImpl *f;
    word alloc;
    char tok[16];
} PsCompiler;

#define PS_TOK_NONE  0
#define PS_TOK_OPEN  1
#define PS_TOK_CLOSE 2
#define PS_TOK_NUM   3
#define PS_TOK_NAME  4
#define PS_TOK_EOF   5

static long PsGetc(PsCompiler *c)
{
    long ch;

    if (c->look != -2)
    {
        ch = c->look;
        c->look = -2;
        return ch;
    }
    return StreamGetChar(c->str);
}

static byte PsNextToken(PsCompiler *c)
{
    long ch;
    word n = 0;

    for (;;)
    {
        ch = PsGetc(c);
        if (ch == EOF)
        {
            return PS_TOK_EOF;
        }
        if (ch == '%')
        {
            while ((ch = PsGetc(c)) != EOF && ch != '\n' && ch != '\r')
            {
            }
            continue;
        }
        if (ch > ' ')
        {
            break;
        }
    }
    if (ch == '{')
    {
        return PS_TOK_OPEN;
    }
    if (ch == '}')
    {
        return PS_TOK_CLOSE;
    }
    while (ch != EOF && ch > ' ' && ch != '{' && ch != '}' && ch != '%')
    {
        if (n < sizeof(c->tok) - 1)
        {
            c->tok[n++] = (char)ch;
        }
        ch = PsGetc(c);
    }
    c->look = ch;
    c->tok[n] = '\0';
    return ((c->tok[0] >= '0' && c->tok[0] <= '9') || c->tok[0] == '-' ||
            c->tok[0] == '+' || c->tok[0] == '.') ? PS_TOK_NUM : PS_TOK_NAME;
}

/* "12", "-0.5", "3.25" -> fixed; sets *isInt */
static gdouble PsParseNum(char *s, GBool *isInt)
{
    GBool neg = gFalse;
    gdouble ip = 0, fp = 0, scale = FIX_ONE;

    *isInt = gTrue;
    if (*s == '-' || *s == '+')
    {
        neg = (*s == '-');
        ++s;
    }
    while (*s >= '0' && *s <= '9')
    {
        if (ip < 32000)
        {
            ip = ip * 10 + (*s - '0');
        }
        ++s;
    }
    if (*s == '.')
    {
        *isInt = gFalse;
        ++s;
        while (*s >= '0' && *s <= '9')
        {
            scale /= 10;
            fp += scale * (*s - '0');
            ++s;
        }
    }
    if (*s == 'e' || *s == 'E')
    {
        *isInt = gFalse;        /* exponents: ignored (not used in practice) */
    }
    ip = FIX(ip) + fp;
    return neg ? -ip : ip;
}

static PsOp *PsEmit(PsCompiler *c, byte op, sdword val)
{
    {
        PdfFuncImpl *f = c->f;
        PsOp *p;

        if (f->u.ps.len >= PS_MAX_OPS)
        {
            return NULL;
        }
        if (f->u.ps.len == c->alloc)
        {
            c->alloc += 256;
            if (f->u.ps.han == NullHandle)
            {
                f->u.ps.han = MemAlloc(c->alloc * sizeof(PsOp), HF_SWAPABLE,
                    HAF_NO_ERR);
            }
            else
            {
                MemUnlock(f->u.ps.han);
                if (!MemReAlloc(f->u.ps.han, c->alloc * sizeof(PsOp),
                    HAF_NO_ERR))
                {
                    f->u.ps.code = MemLock(f->u.ps.han);
                    GMemSetError();
                    return NULL;
                }
            }
            if (f->u.ps.han == NullHandle)
            {
                GMemSetError();
                return NULL;
            }
            f->u.ps.code = MemLock(f->u.ps.han);
        }
        p = &f->u.ps.code[f->u.ps.len++];
        p->op = op;
        p->val = val;
        return p;
    }
}

/* Compile up to the closing '}' of the current block. */
static GBool PsCompileBlock(PsCompiler *c, word depth)
{
    byte tok, op;
    word jz, jmp;
    GBool isInt;
    gdouble v;

    if (depth > 16)
    {
        return gFalse;
    }
    for (;;)
    {
        tok = PsNextToken(c);
        switch (tok)
        {
            case PS_TOK_CLOSE:
                return gTrue;
            case PS_TOK_EOF:
                return gFalse;
            case PS_TOK_NUM:
                v = PsParseNum(c->tok, &isInt);
                if (!PsEmit(c, isInt ? PS_PUSH_INT : PS_PUSH_REAL, v))
                {
                    return gFalse;
                }
                break;
            case PS_TOK_NAME:
                op = PsLookupOp(c->tok);
                if (op == PS_BAD || op == PS_IF || op == PS_IFELSE)
                {
                    return gFalse;
                }
                if (!PsEmit(c, op, 0))
                {
                    return gFalse;
                }
                break;
            case PS_TOK_OPEN:
                /* cond { then } if   |   cond { then } { else } ifelse */
                jz = c->f->u.ps.len;
                if (!PsEmit(c, PS_JZ, 0) || !PsCompileBlock(c, depth + 1))
                {
                    return gFalse;
                }
                tok = PsNextToken(c);
                if (tok == PS_TOK_OPEN)
                {
                    jmp = c->f->u.ps.len;
                    if (!PsEmit(c, PS_JMP, 0))
                    {
                        return gFalse;
                    }
                    c->f->u.ps.code[jz].val = c->f->u.ps.len;
                    if (!PsCompileBlock(c, depth + 1))
                    {
                        return gFalse;
                    }
                    c->f->u.ps.code[jmp].val = c->f->u.ps.len;
                    if (PsNextToken(c) != PS_TOK_NAME ||
                        PsLookupOp(c->tok) != PS_IFELSE)
                    {
                        return gFalse;
                    }
                }
                else if (tok == PS_TOK_NAME && PsLookupOp(c->tok) == PS_IF)
                {
                    c->f->u.ps.code[jz].val = c->f->u.ps.len;
                }
                else
                {
                    return gFalse;
                }
                break;
        }
    }
}

static GBool FuncInitPostScript(PdfFuncImpl *f, Obj *obj)
{
    PsCompiler c;

    if (!isStream(obj) || !f->hasRange)
    {
        return gFalse;
    }
    f->u.ps.han = NullHandle;
    f->u.ps.code = NULL;
    f->u.ps.len = 0;
    f->u.ps.stack = gmalloc(PS_STACK_SIZE * sizeof(PsVal));
    if (!f->u.ps.stack)
    {
        return gFalse;
    }
    c.str = getStream(obj);
    c.look = -2;
    c.f = f;
    c.alloc = 0;
    StreamReset(c.str);
    if (PsNextToken(&c) != PS_TOK_OPEN || !PsCompileBlock(&c, 0) ||
        f->u.ps.len == 0)
    {
        EC_WARNING(-1);
        return gFalse;
    }
    return gTrue;
}

static void FuncEvalPostScript(PdfFuncImpl *f, const gdouble *in, gdouble *out)
{
    PsVal *st = f->u.ps.stack;
    PsOp *code = f->u.ps.code;
    sword sp = 0, i, n, j, k;
    word pc = 0, steps = 0;
    PsVal a, b;
    sdword x, y;

#define NEED(cnt) if (sp < (cnt)) goto bad
#define ROOM(cnt) if (sp + (cnt) > PS_STACK_SIZE) goto bad
#define ISINT2   (st[sp - 2].t == PS_T_INT && st[sp - 1].t == PS_T_INT)

    for (i = 0; i < f->m; ++i)
    {
        st[sp].v = FClip(in[i], f->domain[2 * i], f->domain[2 * i + 1]);
        st[sp].t = PS_T_REAL;
        ++sp;
    }

    while (pc < f->u.ps.len)
    {
        if (++steps > 20000)
        {
            goto bad;           /* runaway program */
        }
        switch (code[pc].op)
        {
            case PS_PUSH_INT:
            case PS_PUSH_REAL:
                ROOM(1);
                st[sp].v = code[pc].val;
                st[sp].t = (code[pc].op == PS_PUSH_INT) ? PS_T_INT : PS_T_REAL;
                ++sp;
                break;
            case PS_TRUE:
            case PS_FALSE:
                ROOM(1);
                st[sp].v = (code[pc].op == PS_TRUE);
                st[sp].t = PS_T_BOOL;
                ++sp;
                break;
            case PS_JZ:
                NEED(1);
                if (!st[--sp].v)
                {
                    pc = (word)code[pc].val;
                    continue;
                }
                break;
            case PS_JMP:
                pc = (word)code[pc].val;
                continue;
            case PS_ABS:
                NEED(1);
                if (st[sp - 1].v < 0)
                {
                    st[sp - 1].v = -st[sp - 1].v;
                }
                break;
            case PS_NEG:
                NEED(1);
                st[sp - 1].v = -st[sp - 1].v;
                break;
            case PS_ADD:
            case PS_SUB:
            case PS_MUL:
            case PS_DIV:
            case PS_IDIV:
            case PS_MOD:
                NEED(2);
                x = st[sp - 2].v;
                y = st[sp - 1].v;
                k = ISINT2;
                switch (code[pc].op)
                {
                    case PS_ADD: x += y; break;
                    case PS_SUB: x -= y; break;
                    case PS_MUL: x = GrMulWWFixed(x, y); break;
                    case PS_DIV:
                        x = (y == 0) ? 0 : GrSDivWWFixed(x, y);
                        k = 0;
                        break;
                    case PS_IDIV:
                        x = (y >> 16) ? FIX((sword)(x >> 16) / (sword)(y >> 16))
                            : 0;
                        break;
                    case PS_MOD:
                        x = (y >> 16) ? FIX((sword)(x >> 16) % (sword)(y >> 16))
                            : 0;
                        break;
                }
                --sp;
                st[sp - 1].v = x;
                st[sp - 1].t = k ? PS_T_INT : PS_T_REAL;
                break;
            case PS_ATAN:
                NEED(2);
                x = FAtan(st[sp - 2].v, st[sp - 1].v);
                --sp;
                st[sp - 1].v = x;
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_EXP:
                NEED(2);
                x = FPow(st[sp - 2].v, st[sp - 1].v);
                --sp;
                st[sp - 1].v = x;
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_CEILING:
                NEED(1);
                st[sp - 1].v = (st[sp - 1].v + 0xFFFFL) & ~0xFFFFL;
                break;
            case PS_FLOOR:
                NEED(1);
                st[sp - 1].v &= ~0xFFFFL;
                break;
            case PS_ROUND:
                NEED(1);
                st[sp - 1].v = (st[sp - 1].v + 0x8000L) & ~0xFFFFL;
                break;
            case PS_TRUNCATE:
            case PS_CVI:
                NEED(1);
                x = st[sp - 1].v;
                st[sp - 1].v = (x < 0) ? -((-x) & ~0xFFFFL) : (x & ~0xFFFFL);
                if (code[pc].op == PS_CVI)
                {
                    st[sp - 1].t = PS_T_INT;
                }
                break;
            case PS_CVR:
                NEED(1);
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_SQRT:
                NEED(1);
                st[sp - 1].v = (st[sp - 1].v > 0) ?
                    GrSqrRootWWFixed(st[sp - 1].v) : 0;
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_SIN:
                NEED(1);
                st[sp - 1].v = GrQuickSine(st[sp - 1].v);
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_COS:
                NEED(1);
                st[sp - 1].v = GrQuickCosine(st[sp - 1].v);
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_LN:
            case PS_LOG:
                NEED(1);
                x = FuncFixedLn(st[sp - 1].v);
                if (code[pc].op == PS_LOG)
                {
                    x = GrSDivWWFixed(x, FIX_LN10);
                }
                st[sp - 1].v = x;
                st[sp - 1].t = PS_T_REAL;
                break;
            case PS_EQ:
            case PS_NE:
            case PS_GT:
            case PS_GE:
            case PS_LT:
            case PS_LE:
                NEED(2);
                x = st[sp - 2].v;
                y = st[sp - 1].v;
                switch (code[pc].op)
                {
                    case PS_EQ: x = (x == y); break;
                    case PS_NE: x = (x != y); break;
                    case PS_GT: x = (x > y); break;
                    case PS_GE: x = (x >= y); break;
                    case PS_LT: x = (x < y); break;
                    case PS_LE: x = (x <= y); break;
                }
                --sp;
                st[sp - 1].v = x;
                st[sp - 1].t = PS_T_BOOL;
                break;
            case PS_AND:
            case PS_OR:
            case PS_XOR:
                NEED(2);
                a = st[sp - 2];
                b = st[sp - 1];
                if (a.t == PS_T_BOOL)
                {
                    x = (code[pc].op == PS_AND) ? (a.v && b.v) :
                        (code[pc].op == PS_OR) ? (a.v || b.v) :
                        ((a.v != 0) != (b.v != 0));
                }
                else
                {
                    x = a.v >> 16;
                    y = b.v >> 16;
                    x = (code[pc].op == PS_AND) ? (x & y) :
                        (code[pc].op == PS_OR) ? (x | y) : (x ^ y);
                    x = FIX(x);
                }
                --sp;
                st[sp - 1].v = x;
                break;
            case PS_NOT:
                NEED(1);
                if (st[sp - 1].t == PS_T_BOOL)
                {
                    st[sp - 1].v = !st[sp - 1].v;
                }
                else
                {
                    st[sp - 1].v = FIX(~(sword)(st[sp - 1].v >> 16));
                }
                break;
            case PS_BITSHIFT:
                NEED(2);
                x = st[sp - 2].v >> 16;
                n = (sword)(st[sp - 1].v >> 16);
                x = (n >= 0) ? (x << n) : (x >> (-n));
                --sp;
                st[sp - 1].v = FIX((sword)x);
                st[sp - 1].t = PS_T_INT;
                break;
            case PS_DUP:
                NEED(1);
                ROOM(1);
                st[sp] = st[sp - 1];
                ++sp;
                break;
            case PS_EXCH:
                NEED(2);
                a = st[sp - 1];
                st[sp - 1] = st[sp - 2];
                st[sp - 2] = a;
                break;
            case PS_POP:
                NEED(1);
                --sp;
                break;
            case PS_COPY:
                NEED(1);
                n = (sword)(st[--sp].v >> 16);
                if (n < 0)
                {
                    goto bad;
                }
                NEED(n);
                ROOM(n);
                for (i = 0; i < n; ++i)
                {
                    st[sp + i] = st[sp - n + i];
                }
                sp += n;
                break;
            case PS_INDEX:
                NEED(1);
                n = (sword)(st[--sp].v >> 16);
                if (n < 0)
                {
                    goto bad;
                }
                NEED(n + 1);
                ROOM(1);
                st[sp] = st[sp - 1 - n];
                ++sp;
                break;
            case PS_ROLL:
                NEED(2);
                j = (sword)(st[--sp].v >> 16);
                n = (sword)(st[--sp].v >> 16);
                if (n < 0)
                {
                    goto bad;
                }
                NEED(n);
                if (n > 0)
                {
                    j %= n;
                    if (j < 0)
                    {
                        j += n;
                    }
                    /* rotate the top n elements up by j */
                    for (k = 0; k < j; ++k)
                    {
                        a = st[sp - 1];
                        for (i = sp - 1; i > sp - n; --i)
                        {
                            st[i] = st[i - 1];
                        }
                        st[sp - n] = a;
                    }
                }
                break;
            default:
                goto bad;
        }
        ++pc;
    }

    if (sp < f->n)
    {
        goto bad;
    }
    for (j = 0; j < f->n; ++j)
    {
        out[j] = st[sp - f->n + j].v;
    }
    return;

bad:
    for (j = 0; j < f->n; ++j)
    {
        out[j] = f->range[2 * j];
    }
#undef NEED
#undef ROOM
#undef ISINT2
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
    f->type = (f->type <= 4) ? f->type : 0xFF;

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
            case 4:
                ok = FuncInitPostScript(f, obj);
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
        case 4:
            if (f->u.ps.han)
            {
                MemUnlock(f->u.ps.han);
                MemFree(f->u.ps.han);
            }
            gfree(f->u.ps.stack);
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
        case 4:
            FuncEvalPostScript(f, in, out);
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
