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
 * FILE:          array.h
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

#ifndef _ARRAY_H
#define _ARRAY_H

#ifdef __GNUC__
#pragma interface
#endif


/* Initialize an empty array. */
extern void ArrayInit(Array *arr);

/* Release all elements and storage owned by an array. */
extern void ArrayFree(Array *arr);

/* Increment the reference count, failing if it would overflow. */
#define ArrayIncRef(arr)  (((arr)->ref == (word)0xffff) ? \
                          (GMemSetError(), (word)0) : (word)++((arr)->ref))

/* Decrement the reference count. */
#define ArrayDecRef(arr) ((word)--((arr)->ref))

/* Get the number of elements in the array. */
extern word ArrayGetLength(Array *arr);

/* Append an element to the end of the array. */
extern void ArrayAdd(Array *arr, Obj *elem);

/* Fetch and dereference an array element by index. */
extern void ArrayGet(Array *arr, word i, Obj *obj, XRef *xref);

/* Fetch an array element without resolving indirect references. */
extern void ArrayGetNF(Array *arr, word i, Obj *obj);


#endif  /* _ARRAY_H */
