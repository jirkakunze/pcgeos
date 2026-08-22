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
 *      Port of Derek Noonburg's "GMem" from xpdf 0.8.
 *
 ***********************************************************************/

#ifndef GMEM_H
#define GMEM_H

#include <Ansi/stdio.h>
#include "gtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * GMem
     */

    /*
     * Same contract as malloc: NULL on OOM (sets the sticky error flag), never
     * sleeps or retries.
     */
    extern void *gmalloc(long size);

    extern void *grealloc(void *p, long size);

    /* Clear the sticky out-of-memory error flag. */
    extern void GMemClearError(void);

    /*
     * Check whether an allocation has failed since the last GMemClearError().
     */
    extern GBool GMemHadError(void);

    extern void GMemSetError(void);

    /* Same as free, but tolerates a NULL pointer. */
    extern void gfree(void *p);

    /*
     * Checked addition for non-negative PDF-controlled sizes; gFalse (result
     * untouched) on overflow.
     */
    extern GBool PdfCheckedAdd(long a, long b, long *result);

    /*
     * Checked multiplication for non-negative PDF-controlled sizes; gFalse
     * (result untouched) on overflow.
     */
    extern GBool PdfCheckedMul(long a, long b, long *result);

    /* Allocate memory and copy a NUL-terminated string into it. */
    extern char *copyString(char *s);

#ifdef __cplusplus
}
#endif

#endif  /* GMEM_H */

