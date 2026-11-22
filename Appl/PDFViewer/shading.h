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
 * FILE:          shading.h
 *
 * DESCRIPTION:
 *      Shadings (types 1-3) and the 2x3 matrix helpers they need.
 *      See main/shading.c.
 ***********************************************************************/

#ifndef SHADING_H
#define SHADING_H

#include "obj.h"

/*
 * 2x3 matrices in PDF order [a b c d e f] (p' = p x M), 16.16 fixed.
 */
extern void GfxMatIdentity(gdouble *m);
/* out = a x b (first a, then b); out may alias neither */
extern void GfxMatMul(const gdouble *a, const gdouble *b, gdouble *out);
extern GBool GfxMatInvert(const gdouble *m, gdouble *out);
extern void GfxMatApply(const gdouble *m, gdouble x, gdouble y,
    gdouble *ox, gdouble *oy);
/* bounding box [x0 y0 x1 y1] of box transformed by m (out may be box) */
extern void GfxMatApplyBox(const gdouble *m, const gdouble *box,
    gdouble *out);

/*
 * Draw a shading (dictionary or stream, references resolved).
 *   toPage      maps shading space to page space; the gstring's current
 *               transformation must be exactly this mapping
 *   region      page space box [x0 y0 x1 y1] that has to be covered
 *               (clip box, path bounds); the GEOS clip does the rest
 *   background  paint /Background first (pattern fills, not 'sh')
 * RETURNS: gFalse if the shading can't be drawn (malformed, types 4-7).
 */
extern GBool ShadingDraw(Gfx *gfx, Obj *shading, const gdouble *toPage,
    const gdouble *region, GBool background);

/* Shading type 1-3 (the types ShadingDraw can draw)? */
extern GBool ShadingIsSupported(Obj *shading, XRef *xref);

/*
 * A representative color of the shading (middle of the function
 * domain) for what can't be painted with it (strokes, text).
 */
extern GBool ShadingGetMidColor(Gfx *gfx, Obj *shading, GfxColor *color);

#endif /* SHADING_H */
