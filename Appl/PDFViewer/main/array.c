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
 * FILE:          array.c
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
 *      Port of Derek Noonburg's "Array" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif


#include "pdfGeode.h"
#include "array.h"
#include "gmem.h"
#include "obj.h"


//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

void ArrayInit(Array *arr)
{
  arr->elems = NULL;
  arr->size = arr->length = 0;
  arr->ref = 1;
}

void ArrayFree(Array *arr)
{
  word i;

  for (i = 0; i < arr->length; ++i)
    ObjFree(&arr->elems[i]);
  gfree(arr->elems);
}

#define ARRAY_MAX_ELEMS ((word)(65535L / (long)sizeof(Obj)))

void ArrayAdd(Array *arr, Obj *elem)
{
    word growBy;
    word newSize;
    Obj *newElems;

    if (arr->length >= arr->size) {
        if (arr->size >= ARRAY_MAX_ELEMS) {
            GMemSetError();
            return;
        }

        growBy = arr->size >> 1;
        if (growBy < 8)
            growBy = 8;

        if (growBy > ARRAY_MAX_ELEMS - arr->size)
            growBy = ARRAY_MAX_ELEMS - arr->size;

        newSize = arr->size + growBy;
        newElems = (Obj *)grealloc(arr->elems, (long)newSize * sizeof(Obj));
        if (!newElems)
            return;

        arr->elems = newElems;
        arr->size = newSize;
    }

    ObjCopy(&arr->elems[arr->length], elem);
    if (GMemHadError())
        return;

    ++arr->length;
}

void ArrayGet(Array *arr, word i, Obj *obj, XRef *xref)
{
  ObjFetch(obj, &arr->elems[i], xref);
}

void ArrayGetNF(Array *arr, word i, Obj *obj)
{
  ObjCopy(obj, &arr->elems[i]);
}

// Get number of elements.
word ArrayGetLength(Array *arr) 
{
  return arr->length; 
}
