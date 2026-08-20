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
 * FILE:          gfx.h
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
 *      Port of Derek Noonburg's "Gfx" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef _GFX_H
#define _GFX_H

#ifdef __GNUC__
#pragma interface
#endif


#include "gtypes.h"
#include "pdfGeode.h"


#define maxArgs 8


/***********************************************************************
 *    Gfx
 ***********************************************************************/

typedef enum TchkType {
    tchkBool,   /* boolean */
    tchkInt,    /* integer */
    tchkNum,    /* number (integer or real) */
    tchkString, /* string */
    tchkName,   /* name */
    tchkArray,  /* array */
    tchkProps,  /* properties (dictionary or name) */
    tchkSCN,    /* scn/SCN args (number or name) */
    tchkNone    /* used to avoid empty initializer lists */
} TchkType;

typedef void GfxOperatorFunc(Gfx *state, Obj args[], int numArgs);

typedef struct Operator {
    char name[4];
    int numArgs;
    TchkType tchk[maxArgs];
    GfxOperatorFunc _near *func;
} Operator;


/* Construct a content-stream interpreter bound to a page/form's gstring and resources. */
extern void GfxInit(Gfx *this, Handle gstring, Dict *resDict, XRef *xref, VMFileHandle vmFile1);

/* Release storage owned by a Gfx interpreter. */
extern void GfxFree(Gfx *this);

/* Push a new resource-dictionary scope onto the resource chain. */
extern void GfxResourcesInit(GfxResources *this, GfxResources *next1);

/* Release storage owned by a resource-dictionary scope. */
extern void GfxResourcesFree(GfxResources *this);

/* Interpret a content stream object, drawing into the bound gstring. */
extern GBool GfxDisplay(Gfx *this, Obj *obj);

/* Draw a page's annotation appearance streams. */
extern void GfxDrawAnnotations(Gfx *gfx, Obj *annotsObj);


#endif  /* _GFX_H */
