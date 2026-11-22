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
 * FILE:          main/shading.c
 *
 * DESCRIPTION:
 *      Smooth shadings (PDF 1.7, 8.7.4.5), all seven types:
 *        1 function-based, 2 axial, 3 radial,
 *        4 free-form / 5 lattice Gouraud triangle meshes,
 *        6 Coons / 7 tensor-product patch meshes.
 *
 *      The output is a GString, so every shading is broken down into
 *      flat filled polygons: bands (axial), rings (radial), cells
 *      (function-based), sub-triangles and sub-quads (meshes). The
 *      subdivision is adaptive: a piece is split while the colors of
 *      its corners/ends differ by more than SH_COLOR_TOL and it is
 *      larger than ~half a point on the page, up to a fixed depth.
 *
 *      Coordinates: the shading is drawn in its own coordinate system
 *      (the caller sets the gstring transformation to "shading space ->
 *      page"). GEOS coordinates are 16 bit integers, and shadings are
 *      often defined in a unit square scaled up by 'cm', so the points
 *      are drawn multiplied by 2^shift (inverse scale applied to the
 *      gstring). shift is chosen so the area to cover fits into
 *      +-8000.
 *
 *      Colors: function (if any) -> components of the shading's color
 *      space -> RGB through GfxColorSpaceGetColor, like fills. Constant
 *      alpha (/ca) is blended against white like other fills.
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
#include "gfxState.h"
#include "function.h"
#include "shading.h"
#include "gmem.h"

#ifdef __WATCOMC__
#pragma code_seg("Shading")
#endif

#define FIX_ONE         0x10000L
#define FIX(i)          ((gdouble)(i) << 16)
#define FIX_HALF        0x8000L

#define SH_COLOR_TOL    6       /* max channel difference per piece */
#define SH_MAX_DEPTH    8       /* axial/radial: up to 256 bands */
#define SH_MIN_DEPTH    2
#define SH_FUNC_DEPTH   5       /* function-based: up to 32 x 32 cells */
#define SH_TRI_DEPTH    4       /* Gouraud: up to 256 sub-triangles */
#define SH_PATCH_MAX    16      /* patches: up to 16 x 16 quads */
#define SH_MAX_SEGS     64      /* circle segments */
#define SH_MAX_PTS      (2 * (SH_MAX_SEGS + 1))
#define SH_COORD_MAX    8000    /* drawn area fits in +- this */
#define SH_CLAMP        16000
#define SH_MAX_EXTEND   FIX(64) /* radial extension limit (s units) */
#define SH_MAX_ROW      1000    /* lattice vertices per row */
#define SH_MAX_PIECES   20000L  /* mesh triangles / patches per shading */
#define SH_MAX_VALS     4

/***********************************************************************
 *      Matrices
 ***********************************************************************/

void GfxMatIdentity(gdouble *m)
{
    m[0] = m[3] = FIX_ONE;
    m[1] = m[2] = m[4] = m[5] = 0;
}

void GfxMatMul(const gdouble *a, const gdouble *b, gdouble *out)
{
    out[0] = GrMulWWFixed(a[0], b[0]) + GrMulWWFixed(a[1], b[2]);
    out[1] = GrMulWWFixed(a[0], b[1]) + GrMulWWFixed(a[1], b[3]);
    out[2] = GrMulWWFixed(a[2], b[0]) + GrMulWWFixed(a[3], b[2]);
    out[3] = GrMulWWFixed(a[2], b[1]) + GrMulWWFixed(a[3], b[3]);
    out[4] = GrMulWWFixed(a[4], b[0]) + GrMulWWFixed(a[5], b[2]) + b[4];
    out[5] = GrMulWWFixed(a[4], b[1]) + GrMulWWFixed(a[5], b[3]) + b[5];
}

static gdouble ShAbs(gdouble x)
{
    return (x < 0) ? -x : x;
}

/*
 * The determinant of a matrix with entries around 200 (a unit square
 * scaled to a page) overflows 16.16, so the linear part is normalized
 * by its largest entry first.
 */
GBool GfxMatInvert(const gdouble *m, gdouble *out)
{
    gdouble s, a, b, c, d, det, ds;
    word i;

    s = 0;
    for (i = 0; i < 4; ++i)
    {
        if (ShAbs(m[i]) > s)
        {
            s = ShAbs(m[i]);
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
    if (ShAbs(det) < 16)
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

void GfxMatApply(const gdouble *m, gdouble x, gdouble y, gdouble *ox,
    gdouble *oy)
{
    *ox = GrMulWWFixed(x, m[0]) + GrMulWWFixed(y, m[2]) + m[4];
    *oy = GrMulWWFixed(x, m[1]) + GrMulWWFixed(y, m[3]) + m[5];
}

void GfxMatApplyBox(const gdouble *m, const gdouble *box, gdouble *out)
{
    gdouble x, y, b[4];
    word i;

    memcpy(b, box, sizeof(b));          /* out may alias box */
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

/***********************************************************************
 *      Context
 ***********************************************************************/

typedef struct
{
    gdouble x, y;
    gdouble v[SH_MAX_VALS];     /* color components or t */
} ShVertex;

typedef struct
{
    Gfx *gfx;
    Handle gs;
    XRef *xref;
    Dict *dict;
    word type;

    GfxColorSpace cs;
    GBool csInit;
    word nComps;                /* color components of cs */
    PdfFunction *funcs[FUNC_MAX_OUT];
    word nFuncs;
    word nVals;                 /* values per color: nComps or 1 (t) */

    gdouble toPage[6];
    gdouble pageScale;          /* rough length scale shading -> page */
    gdouble region[4];          /* shading space box to cover */
    gdouble pageRegion[4];
    sword shift;

    GfxColor last;
    GBool haveLast;
    gdouble fillAlpha;

    word segs;                  /* radial: segments per circle */
    gdouble cosTab[SH_MAX_SEGS + 1];
    gdouble sinTab[SH_MAX_SEGS + 1];
    Point pts[SH_MAX_PTS];
    gdouble ringXY[2 * SH_MAX_PTS];
    dword pieces;
} ShCtx;

static word ShReadNums(Dict *dict, char *key, gdouble *out, word max,
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
            else if (isBool(&elem))
            {
                out[got++] = getBool(&elem) ? FIX_ONE : 0;
            }
            ObjFree(&elem);
        }
    }
    else if (isNum(&arr))
    {
        out[got++] = getNum(&arr);
    }
    ObjFree(&arr);
    return got;
}

/* ColorSpace and Function(s); false if the shading can't be drawn */
static GBool ShInitColors(ShCtx *c)
{
    Obj obj, elem;
    word i, n;

    DictLookup(c->dict, "ColorSpace", &obj, c->xref);
    if (isNull(&obj))
    {
        ObjFree(&obj);
        return gFalse;
    }
    GfxColorSpaceInit(&c->cs, &obj, c->xref);
    ObjFree(&obj);
    c->csInit = gTrue;
    if (!c->cs.ok)
    {
        return gFalse;
    }
    c->nComps = GfxColorSpaceGetNumPixelComps(&c->cs);
    if (c->nComps < 1 || c->nComps > SH_MAX_VALS)
    {
        return gFalse;
    }

    c->nFuncs = 0;
    DictLookup(c->dict, "Function", &obj, c->xref);
    if (isArray(&obj))
    {
        n = ObjArrayGetLength(&obj);
        if (n != c->nComps)
        {
            ObjFree(&obj);
            return gFalse;
        }
        for (i = 0; i < n; ++i)
        {
            ObjArrayGet(&obj, i, &elem, c->xref);
            c->funcs[i] = FuncParse(&elem, c->xref);
            ObjFree(&elem);
            if (!c->funcs[i] || FuncGetOutputs(c->funcs[i]) != 1)
            {
                c->nFuncs = i + 1;
                ObjFree(&obj);
                return gFalse;
            }
            c->nFuncs = i + 1;
        }
    }
    else if (!isNull(&obj))
    {
        c->funcs[0] = FuncParse(&obj, c->xref);
        if (!c->funcs[0])
        {
            ObjFree(&obj);
            return gFalse;
        }
        c->nFuncs = 1;
        if (FuncGetOutputs(c->funcs[0]) < c->nComps)
        {
            ObjFree(&obj);
            return gFalse;
        }
    }
    ObjFree(&obj);
    c->nVals = c->nFuncs ? 1 : c->nComps;
    return gTrue;
}

static void ShFreeColors(ShCtx *c)
{
    word i;

    for (i = 0; i < c->nFuncs; ++i)
    {
        FuncFree(c->funcs[i]);
    }
    c->nFuncs = 0;
    if (c->csInit)
    {
        GfxColorSpaceFree(&c->cs);
        c->csInit = gFalse;
    }
}

/* color of a value tuple: function input(s), or color components */
static void ShColor(ShCtx *c, const gdouble *in, GfxColor *col)
{
    gdouble comps[FUNC_MAX_OUT];
    short x[4];
    word i, alpha;
    sword r, g, b;
    long v;

    if (c->nFuncs == 1)
    {
        FuncEval(c->funcs[0], in, comps);
    }
    else if (c->nFuncs > 1)
    {
        for (i = 0; i < c->nFuncs; ++i)
        {
            FuncEval(c->funcs[i], in, &comps[i]);
        }
    }
    else
    {
        for (i = 0; i < c->nComps; ++i)
        {
            comps[i] = in[i];
        }
    }
    x[0] = x[1] = x[2] = x[3] = 0;
    for (i = 0; i < c->nComps; ++i)
    {
        if (c->cs.indexed)
        {
            v = comps[i] >> 16;
            x[i] = (short)((v < 0) ? 0 : (v > c->cs.indexHigh) ?
                c->cs.indexHigh : v);
        }
        else
        {
            v = (GrMulWWFixed(comps[i], FIX(255)) + FIX_HALF) >> 16;
            x[i] = (short)((v < 0) ? 0 : (v > 255) ? 255 : v);
        }
    }
    GfxColorSpaceGetColor(&c->cs, x, col);

    /* constant alpha against white, like the other fills */
    alpha = (word)((GrMulWWFixed(c->fillAlpha, FIX(255)) + FIX_HALF) >> 16);
    if (alpha < 255)
    {
        r = col->r;
        g = col->g;
        b = col->b;
        col->r = (short)(((word)r * alpha) / 255 + 255 - alpha);
        col->g = (short)(((word)g * alpha) / 255 + 255 - alpha);
        col->b = (short)(((word)b * alpha) / 255 + 255 - alpha);
    }
}

static word ShColorDiff(const GfxColor *a, const GfxColor *b)
{
    word d, m;

    m = (a->r > b->r) ? a->r - b->r : b->r - a->r;
    d = (a->g > b->g) ? a->g - b->g : b->g - a->g;
    if (d > m)
    {
        m = d;
    }
    d = (a->b > b->b) ? a->b - b->b : b->b - a->b;
    return (d > m) ? d : m;
}

/* shading space coordinate -> drawn integer coordinate */
static sword ShDev(ShCtx *c, gdouble v)
{
    long r;

    if (c->shift >= 0)
    {
        r = (v + (FIX_HALF >> c->shift)) >> (16 - c->shift);
    }
    else
    {
        r = v >> (16 - c->shift);
    }
    return (sword)((r < -SH_CLAMP) ? -SH_CLAMP : (r > SH_CLAMP) ? SH_CLAMP : r);
}

/* xy: n coordinate pairs in shading space */
static void ShFillPoly(ShCtx *c, const gdouble *xy, word n,
    const GfxColor *col)
{
    word i;

    if (n < 3 || n > SH_MAX_PTS)
    {
        return;
    }
    if (!c->haveLast || col->r != c->last.r || col->g != c->last.g ||
        col->b != c->last.b)
    {
        GrSetAreaColor(c->gs, CF_RGB, col->r, col->g, col->b);
        c->last = *col;
        c->haveLast = gTrue;
    }
    for (i = 0; i < n; ++i)
    {
        c->pts[i].P_x = ShDev(c, xy[2 * i]);
        c->pts[i].P_y = ShDev(c, xy[2 * i + 1]);
    }
    GrFillPolygon(c->gs, ODD_EVEN, c->pts, n);
}

/* length of a shading space vector on the page (rough) */
static gdouble ShPageLen(ShCtx *c, gdouble dx, gdouble dy)
{
    return GrMulWWFixed(ShAbs(dx) + ShAbs(dy), c->pageScale);
}

/*
 * Choose the drawing scale: the region (and maxCoord, e.g. mesh
 * coordinates) must fit into +-SH_COORD_MAX after scaling by 2^shift.
 */
static void ShSetShift(ShCtx *c, gdouble maxCoord)
{
    gdouble m = maxCoord;
    word i;
    long lim;

    for (i = 0; i < 4; ++i)
    {
        if (ShAbs(c->region[i]) > m)
        {
            m = ShAbs(c->region[i]);
        }
    }
    c->shift = 12;
    lim = FIX(SH_COORD_MAX);
    while (c->shift > -4)
    {
        if (c->shift >= 0)
        {
            if (m <= (lim >> c->shift))
            {
                break;
            }
        }
        else if ((m >> (-c->shift)) <= lim)
        {
            break;
        }
        --c->shift;
    }
}

/* vector length with overflow protection */
static gdouble ShLen(gdouble dx, gdouble dy)
{
    gdouble s = ShAbs(dx) > ShAbs(dy) ? ShAbs(dx) : ShAbs(dy);
    gdouble a, b;

    if (s == 0)
    {
        return 0;
    }
    a = GrSDivWWFixed(dx, s);
    b = GrSDivWWFixed(dy, s);
    return GrMulWWFixed(s, GrSqrRootWWFixed(GrMulWWFixed(a, a) +
        GrMulWWFixed(b, b)));
}

/***********************************************************************
 *      Type 2: axial
 ***********************************************************************/

typedef struct
{
    gdouble x0, y0, dx, dy;     /* axis start and vector */
    gdouble nx, ny;             /* unit normal */
    gdouble qmin, qmax;         /* perpendicular extent of the region */
    gdouble t0, t1;
    gdouble len;
} ShAxial;

static void ShAxialColor(ShCtx *c, ShAxial *a, gdouble s, GfxColor *col)
{
    gdouble t = a->t0 + GrMulWWFixed(s, a->t1 - a->t0);

    ShColor(c, &t, col);
}

static void ShAxialBand(ShCtx *c, ShAxial *a, gdouble sa, gdouble sb,
    const GfxColor *col)
{
    gdouble xy[8];
    gdouble xa = a->x0 + GrMulWWFixed(a->dx, sa);
    gdouble ya = a->y0 + GrMulWWFixed(a->dy, sa);
    gdouble xb = a->x0 + GrMulWWFixed(a->dx, sb);
    gdouble yb = a->y0 + GrMulWWFixed(a->dy, sb);
    gdouble nx0 = GrMulWWFixed(a->nx, a->qmin);
    gdouble ny0 = GrMulWWFixed(a->ny, a->qmin);
    gdouble nx1 = GrMulWWFixed(a->nx, a->qmax);
    gdouble ny1 = GrMulWWFixed(a->ny, a->qmax);

    xy[0] = xa + nx0; xy[1] = ya + ny0;
    xy[2] = xb + nx0; xy[3] = yb + ny0;
    xy[4] = xb + nx1; xy[5] = yb + ny1;
    xy[6] = xa + nx1; xy[7] = ya + ny1;
    ShFillPoly(c, xy, 4, col);
}

static void ShAxialSub(ShCtx *c, ShAxial *a, gdouble sa, gdouble sb,
    GfxColor *ca, GfxColor *cb, word depth)
{
    GfxColor cm;
    gdouble sm = sa + (sb - sa) / 2;

    ShAxialColor(c, a, sm, &cm);
    if (depth >= SH_MAX_DEPTH ||
        GrMulWWFixed(GrMulWWFixed(sb - sa, a->len), c->pageScale) < FIX_HALF ||
        (depth >= SH_MIN_DEPTH && ShColorDiff(ca, cb) <= SH_COLOR_TOL &&
         ShColorDiff(ca, &cm) <= SH_COLOR_TOL &&
         ShColorDiff(&cm, cb) <= SH_COLOR_TOL))
    {
        ShAxialBand(c, a, sa, sb, &cm);
        return;
    }
    ShAxialSub(c, a, sa, sm, ca, &cm, depth + 1);
    ShAxialSub(c, a, sm, sb, &cm, cb, depth + 1);
}

static void ShDrawAxial(ShCtx *c)
{
    ShAxial a;
    gdouble co[4], dom[2], ext[2];
    gdouble ex, ey, rx, ry, along, perp, s, smin, smax, lo, hi;
    GfxColor c0, c1;
    word i;

    if (ShReadNums(c->dict, "Coords", co, 4, c->xref) < 4)
    {
        return;
    }
    if (ShReadNums(c->dict, "Domain", dom, 2, c->xref) < 2)
    {
        dom[0] = 0;
        dom[1] = FIX_ONE;
    }
    ext[0] = ext[1] = 0;
    ShReadNums(c->dict, "Extend", ext, 2, c->xref);

    a.x0 = co[0];
    a.y0 = co[1];
    a.dx = co[2] - co[0];
    a.dy = co[3] - co[1];
    a.t0 = dom[0];
    a.t1 = dom[1];
    a.len = ShLen(a.dx, a.dy);
    if (a.len == 0)
    {
        return;
    }
    ex = GrSDivWWFixed(a.dx, a.len);
    ey = GrSDivWWFixed(a.dy, a.len);
    a.nx = -ey;
    a.ny = ex;

    /* extent of the region along and across the axis */
    smin = smax = 0;
    a.qmin = a.qmax = 0;
    for (i = 0; i < 4; ++i)
    {
        rx = c->region[(i & 1) ? 2 : 0] - a.x0;
        ry = c->region[(i & 2) ? 3 : 1] - a.y0;
        along = GrMulWWFixed(rx, ex) + GrMulWWFixed(ry, ey);
        perp = GrMulWWFixed(rx, a.nx) + GrMulWWFixed(ry, a.ny);
        s = GrSDivWWFixed(along, a.len);
        if (i == 0 || s < smin)
        {
            smin = s;
        }
        if (i == 0 || s > smax)
        {
            smax = s;
        }
        if (i == 0 || perp < a.qmin)
        {
            a.qmin = perp;
        }
        if (i == 0 || perp > a.qmax)
        {
            a.qmax = perp;
        }
    }

    lo = ext[0] ? smin : 0;
    hi = ext[1] ? smax : FIX_ONE;
    if (lo < smin)
    {
        lo = smin;
    }
    if (hi > smax)
    {
        hi = smax;
    }
    if (lo >= hi)
    {
        return;
    }
    ShAxialColor(c, &a, 0, &c0);
    ShAxialColor(c, &a, FIX_ONE, &c1);
    if (lo < 0)
    {
        ShAxialBand(c, &a, lo, (hi < 0) ? hi : 0, &c0);
    }
    if (hi > 0 && lo < FIX_ONE)
    {
        s = (lo > 0) ? lo : 0;
        smax = (hi < FIX_ONE) ? hi : FIX_ONE;
        ShAxialColor(c, &a, s, &c0);
        ShAxialColor(c, &a, smax, &c1);
        ShAxialSub(c, &a, s, smax, &c0, &c1, 0);
    }
    if (hi > FIX_ONE)
    {
        ShAxialColor(c, &a, FIX_ONE, &c1);
        ShAxialBand(c, &a, (lo > FIX_ONE) ? lo : FIX_ONE, hi, &c1);
    }
}

/***********************************************************************
 *      Type 3: radial
 ***********************************************************************/

typedef struct
{
    gdouble x0, y0, r0, dx, dy, dr;
    gdouble t0, t1;
} ShRadial;

static void ShRadialColor(ShCtx *c, ShRadial *r, gdouble s, GfxColor *col)
{
    gdouble t;

    if (s < 0)
    {
        s = 0;
    }
    if (s > FIX_ONE)
    {
        s = FIX_ONE;
    }
    t = r->t0 + GrMulWWFixed(s, r->t1 - r->t0);
    ShColor(c, &t, col);
}

/* ring between circle(sa) and circle(sb), odd-even filled */
static void ShRadialRing(ShCtx *c, ShRadial *r, gdouble sa, gdouble sb,
    const GfxColor *col)
{
    gdouble *xy = c->ringXY;
    gdouble cx, cy, rad;
    word k, n = 0, pass;
    gdouble s;

    for (pass = 0; pass < 2; ++pass)
    {
        s = pass ? sa : sb;
        cx = r->x0 + GrMulWWFixed(r->dx, s);
        cy = r->y0 + GrMulWWFixed(r->dy, s);
        rad = r->r0 + GrMulWWFixed(r->dr, s);
        if (rad < 0)
        {
            rad = 0;
        }
        for (k = 0; k <= c->segs; ++k)
        {
            word kk = pass ? c->segs - k : k;

            xy[2 * n] = cx + GrMulWWFixed(rad, c->cosTab[kk]);
            xy[2 * n + 1] = cy + GrMulWWFixed(rad, c->sinTab[kk]);
            ++n;
        }
    }
    ShFillPoly(c, xy, n, col);
}

static void ShRadialSub(ShCtx *c, ShRadial *r, gdouble sa, gdouble sb,
    GfxColor *ca, GfxColor *cb, word depth)
{
    GfxColor cm;
    gdouble sm = sa + (sb - sa) / 2;
    gdouble w;

    ShRadialColor(c, r, sm, &cm);
    w = GrMulWWFixed(sb - sa, ShAbs(r->dr) + ShAbs(r->dx) + ShAbs(r->dy));
    if (depth >= SH_MAX_DEPTH ||
        GrMulWWFixed(w, c->pageScale) < FIX_HALF ||
        (depth >= SH_MIN_DEPTH && ShColorDiff(ca, cb) <= SH_COLOR_TOL &&
         ShColorDiff(ca, &cm) <= SH_COLOR_TOL &&
         ShColorDiff(&cm, cb) <= SH_COLOR_TOL))
    {
        ShRadialRing(c, r, sa, sb, &cm);
        return;
    }
    ShRadialSub(c, r, sa, sm, ca, &cm, depth + 1);
    ShRadialSub(c, r, sm, sb, &cm, cb, depth + 1);
}

/*
 * How far (in s) the circles have to be continued beyond an end (at
 * s = end, moving in direction dir = +-1) to either shrink to radius 0
 * or grow over the whole region.
 */
static gdouble ShRadialExtent(ShCtx *c, ShRadial *r, gdouble end, sword dir)
{
    gdouble rad = r->r0 + GrMulWWFixed(r->dr, end);
    gdouble cx = r->x0 + GrMulWWFixed(r->dx, end);
    gdouble cy = r->y0 + GrMulWWFixed(r->dy, end);
    gdouble grow = (dir > 0) ? r->dr : -r->dr;   /* radius change per s */
    gdouble move = ShLen(r->dx, r->dy);
    gdouble farDist = 0, d, k;
    word i;

    if (grow < 0)
    {
        /* shrinking: until radius 0 */
        k = GrSDivWWFixed(rad, -grow);
        return (k > SH_MAX_EXTEND) ? SH_MAX_EXTEND : k;
    }
    for (i = 0; i < 4; ++i)
    {
        d = ShLen(c->region[(i & 1) ? 2 : 0] - cx,
            c->region[(i & 2) ? 3 : 1] - cy);
        if (d > farDist)
        {
            farDist = d;
        }
    }
    if (grow <= move || farDist <= rad)
    {
        return (farDist <= rad) ? 0 : SH_MAX_EXTEND;
    }
    k = GrSDivWWFixed(farDist - rad, grow - move) + FIX_ONE;
    return (k > SH_MAX_EXTEND || k < 0) ? SH_MAX_EXTEND : k;
}

static void ShDrawRadial(ShCtx *c)
{
    ShRadial r;
    gdouble co[6], dom[2], ext[2], k, rmax;
    GfxColor c0, c1;
    word i;

    if (ShReadNums(c->dict, "Coords", co, 6, c->xref) < 6)
    {
        return;
    }
    if (ShReadNums(c->dict, "Domain", dom, 2, c->xref) < 2)
    {
        dom[0] = 0;
        dom[1] = FIX_ONE;
    }
    ext[0] = ext[1] = 0;
    ShReadNums(c->dict, "Extend", ext, 2, c->xref);
    if (co[2] < 0 || co[5] < 0)
    {
        return;
    }
    r.x0 = co[0];
    r.y0 = co[1];
    r.r0 = co[2];
    r.dx = co[3] - co[0];
    r.dy = co[4] - co[1];
    r.dr = co[5] - co[2];
    r.t0 = dom[0];
    r.t1 = dom[1];

    /* segments per circle from the largest radius on the page */
    rmax = ShLen(c->region[2] - c->region[0], c->region[3] - c->region[1]);
    if (co[2] > rmax)
    {
        rmax = co[2];
    }
    if (co[5] > rmax)
    {
        rmax = co[5];
    }
    rmax = GrMulWWFixed(rmax, c->pageScale);
    c->segs = (rmax < FIX(20)) ? 16 : (rmax < FIX(100)) ? 32 : SH_MAX_SEGS;
    for (i = 0; i <= c->segs; ++i)
    {
        gdouble ang = FIX(360) / c->segs * i;

        c->cosTab[i] = GrQuickCosine(ang);
        c->sinTab[i] = GrQuickSine(ang);
    }

    ShRadialColor(c, &r, 0, &c0);
    ShRadialColor(c, &r, FIX_ONE, &c1);

    /* rings in increasing s: larger s is painted over smaller s */
    if (ext[0])
    {
        k = ShRadialExtent(c, &r, 0, -1);
        if (k > 0)
        {
            ShRadialRing(c, &r, -k, 0, &c0);
        }
    }
    ShRadialSub(c, &r, 0, FIX_ONE, &c0, &c1, 0);
    if (ext[1])
    {
        k = ShRadialExtent(c, &r, FIX_ONE, 1);
        if (k > 0)
        {
            ShRadialRing(c, &r, FIX_ONE, FIX_ONE + k, &c1);
        }
    }
}

/***********************************************************************
 *      Type 1: function-based
 ***********************************************************************/

typedef struct
{
    gdouble m[6];               /* domain -> shading space */
} ShFuncShading;

static void ShFuncColor(ShCtx *c, gdouble x, gdouble y, GfxColor *col)
{
    gdouble in[2];

    in[0] = x;
    in[1] = y;
    ShColor(c, in, col);
}

static void ShFuncCell(ShCtx *c, ShFuncShading *f, gdouble x0, gdouble x1,
    gdouble y0, gdouble y1, word depth)
{
    gdouble xy[8], xm, ym, px, py;
    GfxColor q[4], center;
    word i;
    GBool split;

    GfxMatApply(f->m, x0, y0, &xy[0], &xy[1]);
    GfxMatApply(f->m, x1, y0, &xy[2], &xy[3]);
    GfxMatApply(f->m, x1, y1, &xy[4], &xy[5]);
    GfxMatApply(f->m, x0, y1, &xy[6], &xy[7]);
    xm = x0 + (x1 - x0) / 2;
    ym = y0 + (y1 - y0) / 2;
    ShFuncColor(c, xm, ym, &center);

    split = (depth < SH_FUNC_DEPTH);
    if (split && depth >= 1)
    {
        px = ShPageLen(c, xy[4] - xy[0], xy[5] - xy[1]);
        py = ShPageLen(c, xy[6] - xy[2], xy[7] - xy[3]);
        if (px < FIX_ONE && py < FIX_ONE)
        {
            split = gFalse;
        }
        else
        {
            ShFuncColor(c, x0, y0, &q[0]);
            ShFuncColor(c, x1, y0, &q[1]);
            ShFuncColor(c, x1, y1, &q[2]);
            ShFuncColor(c, x0, y1, &q[3]);
            split = gFalse;
            for (i = 0; i < 4 && !split; ++i)
            {
                split = ShColorDiff(&q[i], &center) > SH_COLOR_TOL;
            }
        }
    }
    if (!split)
    {
        ShFillPoly(c, xy, 4, &center);
        return;
    }
    ShFuncCell(c, f, x0, xm, y0, ym, depth + 1);
    ShFuncCell(c, f, xm, x1, y0, ym, depth + 1);
    ShFuncCell(c, f, x0, xm, ym, y1, depth + 1);
    ShFuncCell(c, f, xm, x1, ym, y1, depth + 1);
}

static void ShDrawFunctionBased(ShCtx *c)
{
    ShFuncShading f;
    gdouble dom[4];

    if (c->nFuncs == 0 || FuncGetInputs(c->funcs[0]) != 2)
    {
        return;
    }
    if (ShReadNums(c->dict, "Domain", dom, 4, c->xref) < 4)
    {
        dom[0] = dom[2] = 0;
        dom[1] = dom[3] = FIX_ONE;
    }
    if (ShReadNums(c->dict, "Matrix", f.m, 6, c->xref) < 6)
    {
        GfxMatIdentity(f.m);
    }
    ShFuncCell(c, &f, dom[0], dom[1], dom[2], dom[3], 0);
}

/***********************************************************************
 *      Meshes: bit reader
 ***********************************************************************/

typedef struct
{
    Stream *str;
    word buf;
    word bits;
    GBool eof;
    word bpc, bpf, bpComp;          /* bits per coordinate/flag/component */
    gdouble decode[4 + 2 * SH_MAX_VALS];
} ShMesh;

static dword ShReadBits(ShMesh *m, word n)
{
    dword v = 0;
    word take;
    long ch;

    while (n > 0)
    {
        if (m->bits == 0)
        {
            ch = StreamGetChar(m->str);
            if (ch == EOF)
            {
                m->eof = gTrue;
                return 0;
            }
            m->buf = (word)ch;
            m->bits = 8;
        }
        take = (n < m->bits) ? n : m->bits;
        v = (v << take) | ((m->buf >> (m->bits - take)) & ((1 << take) - 1));
        m->bits -= take;
        n -= take;
    }
    return v;
}

static void ShAlign(ShMesh *m)
{
    m->bits = 0;
}

/* dmin + raw / (2^bits - 1) * (dmax - dmin) */
static gdouble ShDecode(dword raw, word bits, gdouble dmin, gdouble dmax)
{
    gdouble f, half;

    if (bits >= 16)
    {
        f = (gdouble)(raw >> (bits - 16));
        f += f >> 15;
    }
    else
    {
        f = (gdouble)((raw << 16) / ((1UL << bits) - 1));
    }
    /* the span can exceed 16.16 (e.g. -32768..32767): halve it */
    half = (dmax >> 1) - (dmin >> 1);
    return dmin + 2 * GrMulWWFixed(f, half);
}

static GBool ShReadVertex(ShCtx *c, ShMesh *m, ShVertex *v, word *flag,
    GBool withFlag)
{
    word i;

    if (withFlag)
    {
        *flag = (word)ShReadBits(m, m->bpf);
    }
    v->x = ShDecode(ShReadBits(m, m->bpc), m->bpc, m->decode[0],
        m->decode[1]);
    v->y = ShDecode(ShReadBits(m, m->bpc), m->bpc, m->decode[2],
        m->decode[3]);
    for (i = 0; i < c->nVals; ++i)
    {
        v->v[i] = ShDecode(ShReadBits(m, m->bpComp), m->bpComp,
            m->decode[4 + 2 * i], m->decode[5 + 2 * i]);
    }
    return !m->eof;
}

/* BitsPerCoordinate etc. and Decode; also widens the drawing area */
static GBool ShInitMesh(ShCtx *c, ShMesh *m, Obj *obj, GBool flags)
{
    gdouble tmp[1];
    word need, i;
    gdouble mx;

    if (!isStream(obj))
    {
        return gFalse;
    }
    m->str = getStream(obj);
    m->bits = 0;
    m->eof = gFalse;
    if (ShReadNums(c->dict, "BitsPerCoordinate", tmp, 1, c->xref) < 1)
    {
        return gFalse;
    }
    m->bpc = (word)(tmp[0] >> 16);
    if (ShReadNums(c->dict, "BitsPerComponent", tmp, 1, c->xref) < 1)
    {
        return gFalse;
    }
    m->bpComp = (word)(tmp[0] >> 16);
    m->bpf = 0;
    if (flags)
    {
        if (ShReadNums(c->dict, "BitsPerFlag", tmp, 1, c->xref) < 1)
        {
            return gFalse;
        }
        m->bpf = (word)(tmp[0] >> 16);
    }
    if (m->bpc < 1 || m->bpc > 32 || m->bpComp < 1 || m->bpComp > 16 ||
        m->bpf > 8)
    {
        return gFalse;
    }
    need = 4 + 2 * c->nVals;
    if (ShReadNums(c->dict, "Decode", m->decode, need, c->xref) < need)
    {
        return gFalse;
    }
    /* the mesh coordinates must fit as well */
    mx = 0;
    for (i = 0; i < 4; ++i)
    {
        if (ShAbs(m->decode[i]) > mx)
        {
            mx = ShAbs(m->decode[i]);
        }
    }
    ShSetShift(c, mx);
    StreamReset(m->str);
    return gTrue;
}

/***********************************************************************
 *      Types 4 and 5: Gouraud triangles
 ***********************************************************************/

static void ShMidVertex(ShCtx *c, const ShVertex *a, const ShVertex *b,
    ShVertex *m)
{
    word i;

    m->x = a->x + (b->x - a->x) / 2;
    m->y = a->y + (b->y - a->y) / 2;
    for (i = 0; i < c->nVals; ++i)
    {
        m->v[i] = a->v[i] + (b->v[i] - a->v[i]) / 2;
    }
}

static void ShMixColor(const GfxColor *a, const GfxColor *b, GfxColor *m)
{
    m->r = (a->r + b->r) / 2;
    m->g = (a->g + b->g) / 2;
    m->b = (a->b + b->b) / 2;
}

static void ShTriangle(ShCtx *c, const ShVertex *a, const ShVertex *b,
    const ShVertex *d, const GfxColor *ca, const GfxColor *cb,
    const GfxColor *cd, word depth)
{
    ShVertex ab, bd, da, ctr;
    GfxColor cab, cbd, cda, cc;
    gdouble xy[6], size;
    word i;

    size = ShPageLen(c, b->x - a->x, b->y - a->y) +
        ShPageLen(c, d->x - b->x, d->y - b->y) +
        ShPageLen(c, a->x - d->x, a->y - d->y);
    if (depth >= SH_TRI_DEPTH || size < FIX(3) ||
        (ShColorDiff(ca, cb) <= SH_COLOR_TOL &&
         ShColorDiff(cb, cd) <= SH_COLOR_TOL &&
         ShColorDiff(cd, ca) <= SH_COLOR_TOL))
    {
        if (c->nFuncs)
        {
            /* mix the corner colors (like common viewers) */
            cc.r = (ca->r + cb->r + cd->r) / 3;
            cc.g = (ca->g + cb->g + cd->g) / 3;
            cc.b = (ca->b + cb->b + cd->b) / 3;
        }
        else
        {
            for (i = 0; i < c->nVals; ++i)
            {
                ctr.v[i] = (a->v[i] + b->v[i] + d->v[i]) / 3;
            }
            ShColor(c, ctr.v, &cc);
        }
        xy[0] = a->x; xy[1] = a->y;
        xy[2] = b->x; xy[3] = b->y;
        xy[4] = d->x; xy[5] = d->y;
        ShFillPoly(c, xy, 3, &cc);
        return;
    }
    ShMidVertex(c, a, b, &ab);
    ShMidVertex(c, b, d, &bd);
    ShMidVertex(c, d, a, &da);
    if (c->nFuncs)
    {
        /* function shadings: the function only at the original corners */
        ShMixColor(ca, cb, &cab);
        ShMixColor(cb, cd, &cbd);
        ShMixColor(cd, ca, &cda);
    }
    else
    {
        ShColor(c, ab.v, &cab);
        ShColor(c, bd.v, &cbd);
        ShColor(c, da.v, &cda);
    }
    ShTriangle(c, a, &ab, &da, ca, &cab, &cda, depth + 1);
    ShTriangle(c, &ab, b, &bd, &cab, cb, &cbd, depth + 1);
    ShTriangle(c, &da, &bd, d, &cda, &cbd, cd, depth + 1);
    ShTriangle(c, &ab, &bd, &da, &cab, &cbd, &cda, depth + 1);
}

static void ShTriangleTop(ShCtx *c, const ShVertex *a, const ShVertex *b,
    const ShVertex *d)
{
    GfxColor ca, cb, cd;

    ShColor(c, a->v, &ca);
    ShColor(c, b->v, &cb);
    ShColor(c, d->v, &cd);
    ShTriangle(c, a, b, d, &ca, &cb, &cd, 0);
    ++c->pieces;
}

static void ShDrawFreeForm(ShCtx *c, Obj *obj)
{
    ShMesh m;
    ShVertex v[4];      /* a, b, c, new */
    word flag, dummy;
    GBool have = gFalse;

    if (!ShInitMesh(c, &m, obj, gTrue))
    {
        return;
    }
    while (c->pieces < SH_MAX_PIECES)
    {
        if (!ShReadVertex(c, &m, &v[3], &flag, gTrue))
        {
            break;
        }
        ShAlign(&m);
        if (flag == 0 || !have)
        {
            v[0] = v[3];
            if (!ShReadVertex(c, &m, &v[1], &dummy, gTrue))
            {
                break;
            }
            ShAlign(&m);
            if (!ShReadVertex(c, &m, &v[2], &dummy, gTrue))
            {
                break;
            }
            ShAlign(&m);
            have = gTrue;
        }
        else if (flag == 1)
        {
            v[0] = v[1];
            v[1] = v[2];
            v[2] = v[3];
        }
        else
        {
            v[1] = v[2];
            v[2] = v[3];
        }
        ShTriangleTop(c, &v[0], &v[1], &v[2]);
    }
}

static void ShDrawLattice(ShCtx *c, Obj *obj)
{
    ShMesh m;
    gdouble tmp[1];
    word perRow, i, dummy;
    MemHandle han;
    ShVertex *prev, *cur, *swap;
    GBool first = gTrue;

    if (ShReadNums(c->dict, "VerticesPerRow", tmp, 1, c->xref) < 1)
    {
        return;
    }
    perRow = (word)(tmp[0] >> 16);
    if (perRow < 2 || perRow > SH_MAX_ROW || !ShInitMesh(c, &m, obj, gFalse))
    {
        return;
    }
    han = MemAlloc(2 * perRow * sizeof(ShVertex), HF_SWAPABLE, HAF_NO_ERR);
    if (han == NullHandle)
    {
        GMemSetError();
        return;
    }
    prev = MemLock(han);
    cur = prev + perRow;
    while (c->pieces < SH_MAX_PIECES)
    {
        for (i = 0; i < perRow; ++i)
        {
            if (!ShReadVertex(c, &m, &cur[i], &dummy, gFalse))
            {
                break;
            }
            ShAlign(&m);
        }
        if (i < perRow)
        {
            break;
        }
        if (!first)
        {
            for (i = 0; i + 1 < perRow; ++i)
            {
                ShTriangleTop(c, &prev[i], &prev[i + 1], &cur[i]);
                ShTriangleTop(c, &prev[i + 1], &cur[i + 1], &cur[i]);
            }
        }
        first = gFalse;
        swap = prev;
        prev = cur;
        cur = swap;
    }
    MemUnlock(han);
    MemFree(han);
}

/***********************************************************************
 *      Types 6 and 7: patches
 ***********************************************************************/

typedef struct
{
    gdouble x[4][4], y[4][4];       /* p[i][j] */
    gdouble v[4][SH_MAX_VALS];      /* c00, c03, c33, c30 */
} ShPatch;

/* boundary points in stream order (Coons and tensor) as (i, j) */
static void ShPatchSet(ShPatch *p, word idx, gdouble x, gdouble y)
{
    /* p00 p01 p02 p03 p13 p23 p33 p32 p31 p30 p20 p10 p11 p12 p22 p21 */
    static const byte ij[16] =
    {
        0x00, 0x01, 0x02, 0x03, 0x13, 0x23, 0x33, 0x32,
        0x31, 0x30, 0x20, 0x10, 0x11, 0x12, 0x22, 0x21
    };
    byte k = ij[idx];

    p->x[k >> 4][k & 15] = x;
    p->y[k >> 4][k & 15] = y;
}

/* interior points of a Coons patch (PDF 1.7, 8.7.4.5.7) */
static gdouble ShCoonsPt(gdouble a, gdouble b1, gdouble b2, gdouble c1,
    gdouble c2, gdouble d1, gdouble d2, gdouble e)
{
    /* (-4 a + 6 (b1 + b2) - 2 (c1 + c2) + 3 (d1 + d2) - e) / 9 */
    return GrMulWWFixed(a, -29127L) +           /* -4/9 */
        GrMulWWFixed(b1 + b2, 43691L) -         /*  6/9 */
        GrMulWWFixed(c1, 14564L) - GrMulWWFixed(c2, 14564L) +
        GrMulWWFixed(d1, 21845L) + GrMulWWFixed(d2, 21845L) -
        GrMulWWFixed(e, 7282L);                 /*  1/9 */
}

static void ShCoonsInterior(gdouble p[4][4])
{
    p[1][1] = ShCoonsPt(p[0][0], p[0][1], p[1][0], p[0][3], p[3][0],
        p[3][1], p[1][3], p[3][3]);
    p[1][2] = ShCoonsPt(p[0][3], p[0][2], p[1][3], p[0][0], p[3][3],
        p[3][2], p[1][0], p[3][0]);
    p[2][1] = ShCoonsPt(p[3][0], p[3][1], p[2][0], p[3][3], p[0][0],
        p[0][1], p[2][3], p[0][3]);
    p[2][2] = ShCoonsPt(p[3][3], p[3][2], p[2][3], p[3][0], p[0][3],
        p[0][2], p[2][0], p[0][0]);
}

static void ShBernstein(gdouble u, gdouble *b)
{
    gdouble w = FIX_ONE - u;
    gdouble u2 = GrMulWWFixed(u, u), w2 = GrMulWWFixed(w, w);

    b[0] = GrMulWWFixed(w2, w);
    b[1] = 3 * GrMulWWFixed(w2, u);
    b[2] = 3 * GrMulWWFixed(w, u2);
    b[3] = GrMulWWFixed(u2, u);
}

typedef struct
{
    gdouble x[SH_PATCH_MAX + 1][SH_PATCH_MAX + 1];
    gdouble y[SH_PATCH_MAX + 1][SH_PATCH_MAX + 1];
    ShPatch cur, prev;
} ShGrid;

static void ShDrawPatch(ShCtx *c, ShPatch *p, ShGrid *g)
{
    gdouble bu[4], bv[4], sx, sy, rx, ry, u, v, minx, maxx, miny, maxy;
    gdouble vals[SH_MAX_VALS], xy[8], w[4];
    GfxColor col[4], cc;
    word n, i, j, k, l, d, maxd;

    /* grid size from the size on the page and the color range */
    minx = maxx = p->x[0][0];
    miny = maxy = p->y[0][0];
    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            if (p->x[i][j] < minx) minx = p->x[i][j];
            if (p->x[i][j] > maxx) maxx = p->x[i][j];
            if (p->y[i][j] < miny) miny = p->y[i][j];
            if (p->y[i][j] > maxy) maxy = p->y[i][j];
        }
    }
    n = (word)(ShPageLen(c, maxx - minx, maxy - miny) >> 16) / 12;
    maxd = 0;
    for (i = 0; i < 4; ++i)
    {
        ShColor(c, p->v[i], &col[i]);
    }
    for (i = 0; i < 4; ++i)
    {
        d = ShColorDiff(&col[i], &col[(i + 1) & 3]);
        if (d > maxd)
        {
            maxd = d;
        }
    }
    if (maxd / SH_COLOR_TOL > n)
    {
        n = maxd / SH_COLOR_TOL;
    }
    n = (n < 2) ? 2 : (n > SH_PATCH_MAX) ? SH_PATCH_MAX : n;

    for (k = 0; k <= n; ++k)
    {
        ShBernstein(FIX_ONE / n * k, bu);
        for (l = 0; l <= n; ++l)
        {
            ShBernstein(FIX_ONE / n * l, bv);
            sx = sy = 0;
            for (i = 0; i < 4; ++i)
            {
                rx = ry = 0;
                for (j = 0; j < 4; ++j)
                {
                    rx += GrMulWWFixed(p->x[i][j], bv[j]);
                    ry += GrMulWWFixed(p->y[i][j], bv[j]);
                }
                sx += GrMulWWFixed(rx, bu[i]);
                sy += GrMulWWFixed(ry, bu[i]);
            }
            g->x[k][l] = sx;
            g->y[k][l] = sy;
        }
    }

    for (k = 0; k < n; ++k)
    {
        for (l = 0; l < n; ++l)
        {
            /* bilinear color at the cell center: u along i, v along j */
            u = FIX_ONE / n * k + FIX_ONE / (2 * n);
            v = FIX_ONE / n * l + FIX_ONE / (2 * n);
            w[0] = GrMulWWFixed(FIX_ONE - u, FIX_ONE - v);
            w[1] = GrMulWWFixed(FIX_ONE - u, v);
            w[2] = GrMulWWFixed(u, v);
            w[3] = GrMulWWFixed(u, FIX_ONE - v);
            if (c->nFuncs)
            {
                /* function only at the corners, mix colors (like
                   common viewers) */
                cc.r = (short)((w[0] * col[0].r + w[1] * col[1].r +
                    w[2] * col[2].r + w[3] * col[3].r + FIX_HALF) >> 16);
                cc.g = (short)((w[0] * col[0].g + w[1] * col[1].g +
                    w[2] * col[2].g + w[3] * col[3].g + FIX_HALF) >> 16);
                cc.b = (short)((w[0] * col[0].b + w[1] * col[1].b +
                    w[2] * col[2].b + w[3] * col[3].b + FIX_HALF) >> 16);
            }
            else
            {
                for (i = 0; i < c->nVals; ++i)
                {
                    vals[i] = GrMulWWFixed(w[0], p->v[0][i]) +
                        GrMulWWFixed(w[1], p->v[1][i]) +
                        GrMulWWFixed(w[2], p->v[2][i]) +
                        GrMulWWFixed(w[3], p->v[3][i]);
                }
                ShColor(c, vals, &cc);
            }
            xy[0] = g->x[k][l];         xy[1] = g->y[k][l];
            xy[2] = g->x[k + 1][l];     xy[3] = g->y[k + 1][l];
            xy[4] = g->x[k + 1][l + 1]; xy[5] = g->y[k + 1][l + 1];
            xy[6] = g->x[k][l + 1];     xy[7] = g->y[k][l + 1];
            ShFillPoly(c, xy, 4, &cc);
        }
    }
}

static void ShDrawPatches(ShCtx *c, Obj *obj, GBool tensor)
{
    ShMesh m;
    ShGrid *grid;
    word flag, npts, ncol, i, j, first;
    gdouble x, y;
    GBool have = gFalse;

    if (!ShInitMesh(c, &m, obj, gTrue))
    {
        return;
    }
    grid = gmalloc(sizeof(ShGrid));
    if (!grid)
    {
        return;
    }
    while (c->pieces < SH_MAX_PIECES)
    {
        flag = (word)ShReadBits(&m, m.bpf);
        if (m.eof)
        {
            break;
        }
        if (flag != 0 && !have)
        {
            break;              /* first patch must be complete */
        }
        if (flag == 0)
        {
            first = 0;
            ncol = 4;
        }
        else
        {
            /* shared edge p00..p03 and c00, c03 from the previous patch */
            first = 4;
            ncol = 2;
            for (j = 0; j < 4; ++j)
            {
                switch (flag)
                {
                    case 1:
                        grid->cur.x[0][j] = grid->prev.x[j][3];
                        grid->cur.y[0][j] = grid->prev.y[j][3];
                        break;
                    case 2:
                        grid->cur.x[0][j] = grid->prev.x[3][3 - j];
                        grid->cur.y[0][j] = grid->prev.y[3][3 - j];
                        break;
                    default:
                        grid->cur.x[0][j] = grid->prev.x[3 - j][0];
                        grid->cur.y[0][j] = grid->prev.y[3 - j][0];
                        break;
                }
            }
            for (i = 0; i < c->nVals; ++i)
            {
                switch (flag)
                {
                    case 1:
                        grid->cur.v[0][i] = grid->prev.v[1][i];
                        grid->cur.v[1][i] = grid->prev.v[2][i];
                        break;
                    case 2:
                        grid->cur.v[0][i] = grid->prev.v[2][i];
                        grid->cur.v[1][i] = grid->prev.v[3][i];
                        break;
                    default:
                        grid->cur.v[0][i] = grid->prev.v[3][i];
                        grid->cur.v[1][i] = grid->prev.v[0][i];
                        break;
                }
            }
        }
        npts = tensor ? 16 : 12;
        for (i = first; i < npts; ++i)
        {
            x = ShDecode(ShReadBits(&m, m.bpc), m.bpc, m.decode[0],
                m.decode[1]);
            y = ShDecode(ShReadBits(&m, m.bpc), m.bpc, m.decode[2],
                m.decode[3]);
            ShPatchSet(&grid->cur, i, x, y);
        }
        for (j = 4 - ncol; j < 4; ++j)
        {
            for (i = 0; i < c->nVals; ++i)
            {
                grid->cur.v[j][i] = ShDecode(ShReadBits(&m, m.bpComp), m.bpComp,
                    m.decode[4 + 2 * i], m.decode[5 + 2 * i]);
            }
        }
        ShAlign(&m);
        if (m.eof)
        {
            break;
        }
        if (!tensor)
        {
            ShCoonsInterior(grid->cur.x);
            ShCoonsInterior(grid->cur.y);
        }
        ShDrawPatch(c, &grid->cur, grid);
        ++c->pieces;
        grid->prev = grid->cur;
        have = gTrue;
    }
    gfree(grid);
}

/***********************************************************************
 *      Public interface
 ***********************************************************************/

static Dict *ShGetDict(Obj *shading)
{
    if (isStream(shading))
    {
        return ObjStreamGetDict(shading);
    }
    return isDict(shading) ? getDict(shading) : NULL;
}

void ShadingDraw(Gfx *gfx, Obj *shading, const gdouble *toPage,
    const gdouble *region, GBool background)
{
    ShCtx *c;
    gdouble inv[6], tmp[4 + 2 * SH_MAX_VALS], bb[4], xy[8];
    gdouble scale;
    GfxColor col;
    word n, i;
    Dict *dict = ShGetDict(shading);

    if (!dict || region[0] >= region[2] || region[1] >= region[3])
    {
        return;
    }
    c = gmalloc(sizeof(ShCtx));
    if (!c)
    {
        return;
    }
    memset(c, 0, sizeof(ShCtx));
    c->gfx = gfx;
    c->gs = gfx->state->gstring;
    c->xref = gfx->xref;
    c->dict = dict;
    c->fillAlpha = gfx->state->fillAlpha;
    memcpy(c->toPage, toPage, sizeof(c->toPage));
    memcpy(c->pageRegion, region, sizeof(c->pageRegion));

    if (ShReadNums(dict, "ShadingType", tmp, 1, c->xref) < 1 ||
        !GfxMatInvert(toPage, inv) || !ShInitColors(c))
    {
        EC_WARNING(-1);
        ShFreeColors(c);
        gfree(c);
        return;
    }
    c->type = (word)(tmp[0] >> 16);
    GfxMatApplyBox(inv, region, c->region);
    scale = (ShAbs(toPage[0]) + ShAbs(toPage[1]) + ShAbs(toPage[2]) +
        ShAbs(toPage[3])) / 2;
    c->pageScale = scale ? scale : FIX_ONE;
    ShSetShift(c, 0);

    GrSaveState(c->gs);
    /* types 4-7 may change the shift (mesh coordinates): scale later */
    if (c->type < 4)
    {
        if (c->shift >= 0)
        {
            GrApplyScale(c->gs, FIX_ONE >> c->shift, FIX_ONE >> c->shift);
        }
        else
        {
            GrApplyScale(c->gs, FIX_ONE << (-c->shift),
                FIX_ONE << (-c->shift));
        }
    }

    switch (c->type)
    {
        case 4: case 5: case 6: case 7:
        {
            /* determine the shift before anything is drawn */
            ShMesh probe;

            if (isStream(shading) && ShInitMesh(c, &probe, shading,
                c->type != 5))
            {
                if (c->shift >= 0)
                {
                    GrApplyScale(c->gs, FIX_ONE >> c->shift,
                        FIX_ONE >> c->shift);
                }
                else
                {
                    GrApplyScale(c->gs, FIX_ONE << (-c->shift),
                        FIX_ONE << (-c->shift));
                }
            }
            else
            {
                c->type = 0;    /* nothing to draw */
            }
            break;
        }
    }

    /* /BBox: clip in shading space */
    if (ShReadNums(dict, "BBox", bb, 4, c->xref) == 4)
    {
        GrSetClipRect(c->gs, PCT_INTERSECTION,
            ShDev(c, (bb[0] < bb[2]) ? bb[0] : bb[2]),
            ShDev(c, (bb[1] < bb[3]) ? bb[1] : bb[3]),
            ShDev(c, (bb[0] < bb[2]) ? bb[2] : bb[0]),
            ShDev(c, (bb[1] < bb[3]) ? bb[3] : bb[1]));
    }

    /* /Background (pattern fills only): the whole region */
    if (background)
    {
        n = ShReadNums(dict, "Background", tmp, SH_MAX_VALS, c->xref);
        if (n == c->nComps && c->type != 0)
        {
            word saveFuncs = c->nFuncs;

            c->nFuncs = 0;          /* components, not function input */
            ShColor(c, tmp, &col);
            c->nFuncs = saveFuncs;
            for (i = 0; i < 4; ++i)
            {
                GfxMatApply(inv, region[(i == 1 || i == 2) ? 2 : 0],
                    region[(i >= 2) ? 3 : 1], &xy[2 * i], &xy[2 * i + 1]);
            }
            ShFillPoly(c, xy, 4, &col);
        }
    }

    switch (c->type)
    {
        case 1:
            ShDrawFunctionBased(c);
            break;
        case 2:
            ShDrawAxial(c);
            break;
        case 3:
            ShDrawRadial(c);
            break;
        case 4:
            ShDrawFreeForm(c, shading);
            break;
        case 5:
            ShDrawLattice(c, shading);
            break;
        case 6:
            ShDrawPatches(c, shading, gFalse);
            break;
        case 7:
            ShDrawPatches(c, shading, gTrue);
            break;
    }
    GrRestoreState(c->gs);

    ShFreeColors(c);
    gfree(c);
}

GBool ShadingGetMidColor(Gfx *gfx, Obj *shading, GfxColor *color)
{
    ShCtx *c;
    gdouble tmp[4], in[2];
    GBool ok = gFalse;
    Dict *dict = ShGetDict(shading);

    if (!dict)
    {
        return gFalse;
    }
    c = gmalloc(sizeof(ShCtx));
    if (!c)
    {
        return gFalse;
    }
    memset(c, 0, sizeof(ShCtx));
    c->gfx = gfx;
    c->xref = gfx->xref;
    c->dict = dict;
    c->fillAlpha = gfx->state->fillAlpha;
    if (ShInitColors(c) && c->nFuncs > 0)
    {
        if (FuncGetInputs(c->funcs[0]) == 2)
        {
            if (ShReadNums(dict, "Domain", tmp, 4, c->xref) < 4)
            {
                tmp[0] = tmp[2] = 0;
                tmp[1] = tmp[3] = FIX_ONE;
            }
            in[0] = tmp[0] + (tmp[1] - tmp[0]) / 2;
            in[1] = tmp[2] + (tmp[3] - tmp[2]) / 2;
        }
        else
        {
            if (ShReadNums(dict, "Domain", tmp, 2, c->xref) < 2)
            {
                tmp[0] = 0;
                tmp[1] = FIX_ONE;
            }
            in[0] = tmp[0] + (tmp[1] - tmp[0]) / 2;
        }
        ShColor(c, in, color);
        ok = gTrue;
    }
    ShFreeColors(c);
    gfree(c);
    return ok;
}
