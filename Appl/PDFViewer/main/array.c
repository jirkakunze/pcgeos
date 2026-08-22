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
 *
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include "pdfGeode.h"
#include "array.h"
#include "gmem.h"
#include "obj.h"

/*
 * Array
 */

#define ARRAY_MAX_ELEMS ((word)(65535L / (long)sizeof(Obj)))

/***********************************************************************
 *      ArrayInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      Array *arr    array
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Clear the element pointer and size/length counters, and set the
 *      reference count to 1.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void ArrayInit(Array *arr)
{
    arr->elems = NULL;
    arr->size = arr->length = 0;
    arr->ref = 1;
}

/***********************************************************************
 *      ArrayFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      Array *arr    array
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
void ArrayFree(Array *arr)
{
    word i;

    for (i = 0; i < arr->length; ++i)
    {
        ObjFree(&arr->elems[i]);
    }
    gfree(arr->elems);
}

/***********************************************************************
 *      ArrayAdd
 ***********************************************************************
 * SYNOPSIS:        Add.
 * PARAMETERS:      Array *arr    array
 *                  Obj *elem    elem
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Grow the backing storage by 50% (minimum 8, capped at
 *      ARRAY_MAX_ELEMS) via grealloc() if the array is full, then...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void ArrayAdd(Array *arr, Obj *elem)
{
    word growBy;
    word newSize;
    Obj *newElems;

    if (arr->length >= arr->size)
    {
        if (arr->size >= ARRAY_MAX_ELEMS)
        {
            GMemSetError();
            return;
        }

        growBy = arr->size >> 1;
        if (growBy < 8)
        {
            growBy = 8;
        }

        if (growBy > ARRAY_MAX_ELEMS - arr->size)
        {
            growBy = ARRAY_MAX_ELEMS - arr->size;
        }

        newSize = arr->size + growBy;
        newElems = (Obj *)grealloc(arr->elems, (long)newSize * sizeof(Obj));
        if (!newElems)
        {
            return;
        }

        arr->elems = newElems;
        arr->size = newSize;
    }

    ObjCopy(&arr->elems[arr->length], elem);
    if (GMemHadError())
    {
        return;
    }

    ++arr->length;
}

/***********************************************************************
 *      ArrayGet
 ***********************************************************************
 * SYNOPSIS:        Get.
 * PARAMETERS:      Array *arr    array
 *                  word i    index
 *                  Obj *obj    object
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Delegate to ObjFetch() on the stored element.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void ArrayGet(Array *arr, word i, Obj *obj, XRef *xref)
{
    ObjFetch(obj, &arr->elems[i], xref);
}

/***********************************************************************
 *      ArrayGetNF
 ***********************************************************************
 * SYNOPSIS:        Get nf.
 * PARAMETERS:      Array *arr    array
 *                  word i    index
 *                  Obj *obj    object
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
void ArrayGetNF(Array *arr, word i, Obj *obj)
{
    ObjCopy(obj, &arr->elems[i]);
}

/***********************************************************************
 *      ArrayGetLength
 ***********************************************************************
 * SYNOPSIS:        Get length.
 * PARAMETERS:      Array *arr    array
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
word ArrayGetLength(Array *arr)
{
    return arr->length;
}

