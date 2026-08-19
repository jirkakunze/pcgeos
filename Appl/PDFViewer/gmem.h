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
 * FILE:          gmem.h
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

#ifndef _GMEM_H
#define _GMEM_H


#include <Ansi/stdio.h>
#include "gtypes.h"


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Same allocation contract as malloc: returns NULL immediately on OOM and
 * sets the sticky GMem error flag. It never sleeps or retries indefinitely.
 */
extern void *gmalloc(long size);

/*
 * Same allocation contract as realloc: returns NULL immediately on OOM,
 * leaves <p> valid, and sets the sticky GMem error flag. If <p> is NULL,
 * calls malloc instead.
 */
extern void *grealloc(void *p, long size);

/* Sticky out-of-memory status for legacy code paths. */
extern void GMemClearError(void);

extern GBool GMemHadError(void);

extern void GMemSetError(void);

/*
 * Same as free, but checks for and ignores NULL pointers.
 */
extern void gfree(void *p);

/*
 * Checked arithmetic for non-negative PDF-controlled sizes and offsets.
 * Returns gFalse without modifying *result if the operation would exceed
 * the signed 32-bit long range used throughout the viewer.
 */
extern GBool PdfCheckedAdd(long a, long b, long *result);

extern GBool PdfCheckedMul(long a, long b, long *result);


/*
 * Allocate memory and copy a string into it.
 */
extern char *copyString(char *s);


#ifdef __cplusplus
}
#endif

#endif  /* _GEMEM_H */
