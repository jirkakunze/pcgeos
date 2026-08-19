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


/***********************************************************************
 *    Array
 ***********************************************************************/

#define ARRAY_MAX_ELEMS ((word)(65535L / (long)sizeof(Obj)))

/***********************************************************************
 *      ArrayInit
 ***********************************************************************
 * SYNOPSIS:        initialize an empty array
 * PARAMETERS:      Array *arr    array to initialize
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called once before an Array is used, to bring it into a
 *      well-defined, empty state with a single owning reference.
 *
 * STRATEGY:
 *      Clear the element pointer and size/length counters, and set
 *      the reference count to 1.
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
 * SYNOPSIS:        release all elements and storage owned by an array
 * PARAMETERS:      Array *arr    array to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when the last reference to an Array goes away, to
 *      release the Obj elements it holds and its backing storage.
 *
 * STRATEGY:
 *      Free each stored Obj in turn, then free the element buffer
 *      itself.
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

    for (i = 0; i < arr->length; ++i) {
        ObjFree(&arr->elems[i]);
    }
    gfree(arr->elems);
}

/***********************************************************************
 *      ArrayAdd
 ***********************************************************************
 * SYNOPSIS:        append an element to the end of an array
 * PARAMETERS:      Array *arr    array to append to
 *                  Obj *elem     element to copy into the array
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called whenever a new element needs to be appended, e.g.
 *      while parsing a PDF array object.
 *
 * STRATEGY:
 *      Grow the backing storage by 50% (minimum 8, capped at
 *      ARRAY_MAX_ELEMS) via grealloc() if the array is full, then
 *      copy the element into the new slot and bump the length. Bails
 *      out silently (leaving the array unchanged) on allocation
 *      failure, size-limit overflow, or a copy error.
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

    if (arr->length >= arr->size) {
        if (arr->size >= ARRAY_MAX_ELEMS) {
            GMemSetError();
            return;
        }

        growBy = arr->size >> 1;
        if (growBy < 8) {
            growBy = 8;
        }

        if (growBy > ARRAY_MAX_ELEMS - arr->size) {
            growBy = ARRAY_MAX_ELEMS - arr->size;
        }

        newSize = arr->size + growBy;
        newElems = (Obj *)grealloc(arr->elems, (long)newSize * sizeof(Obj));
        if (!newElems) {
            return;
        }

        arr->elems = newElems;
        arr->size = newSize;
    }

    ObjCopy(&arr->elems[arr->length], elem);
    if (GMemHadError()) {
        return;
    }

    ++arr->length;
}

/***********************************************************************
 *      ArrayGet
 ***********************************************************************
 * SYNOPSIS:        fetch and dereference an array element by index
 * PARAMETERS:      Array *arr    array to read from
 *                  word i        zero-based element index
 *                  Obj *obj      receives the fetched object
 *                  XRef *xref    cross-reference table for indirect
 *                                object resolution
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by callers that need the fully resolved value of an
 *      array element, following indirect references if present.
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
 * SYNOPSIS:        fetch an array element without resolving references
 * PARAMETERS:      Array *arr    array to read from
 *                  word i        zero-based element index
 *                  Obj *obj      receives a copy of the raw element
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when the caller wants the element as stored, e.g. to
 *      inspect whether it is itself an indirect reference, without
 *      the fetch/resolve overhead of ArrayGet().
 *
 * STRATEGY:
 *      Copy the stored Obj directly via ObjCopy(), bypassing
 *      indirect reference resolution.
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
 * SYNOPSIS:        get the number of elements in an array
 * PARAMETERS:      Array *arr    array to query
 *
 * RETURNS:         word          current element count
 *
 * CONTEXT:
 *      Called by callers needing to bound iteration over an array's
 *      elements.
 *
 * STRATEGY:
 *      Return the length field directly.
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
