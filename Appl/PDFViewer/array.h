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
 *
 ***********************************************************************/

#ifndef _ARRAY_H
#define _ARRAY_H

#ifdef __GNUC__
#pragma interface
#endif


/* Constructor */
extern void
  ArrayInit(Array *arr);

/* Destructor */
extern void
  ArrayFree(Array *arr);

#define ArrayIncRef(arr) \
  (((arr)->ref == (word)0xffff) ? \
   (GMemSetError(), (word)0) : (word)++((arr)->ref))
#define ArrayDecRef(arr) ((word)--((arr)->ref))

/* Get number of elements */
extern word
  ArrayGetLength(Array *arr);

/* Add an element */
extern void
  ArrayAdd(Array *arr, Obj *elem);

/* Accessors */
extern void
  ArrayGet(Array *arr, word i, Obj *obj, XRef *xref);

extern void
  ArrayGetNF(Array *arr, word i, Obj *obj);


#endif  /* _ARRAY_H */
