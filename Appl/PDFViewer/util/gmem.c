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
 * FILE:          gmem.c
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

#include <Ansi/stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ec.h>
#include "gmem.h"

static GBool gMemOutOfMemory = gFalse;

void GMemClearError(void)
{
    gMemOutOfMemory = gFalse;
}

GBool GMemHadError(void)
{
    return gMemOutOfMemory;
}

void GMemSetError(void)
{
    gMemOutOfMemory = gTrue;
}

#define PDF_SIGNED_LONG_MAX 0x7fffffffL

/********************************************************************
 *                      PdfCheckedAdd
 ********************************************************************
 * SYNOPSIS:      Add two non-negative longs with an overflow check.
 *
 * PARAMETERS:    a                     First value.
 *                b                     Second value.
 *                *result               Ptr to the sum (out).
 *
 * RETURNS:       GBool                 gFalse if a value is negative or the sum
 *                                      overflows.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
GBool PdfCheckedAdd(long a, long b, long *result)
{
    EC(ECCheckBounds(result));

    if (a < 0 || b < 0 || a > PDF_SIGNED_LONG_MAX - b)
    {
        return gFalse;
    }

    *result = a + b;
    return gTrue;
}

/********************************************************************
 *                      PdfCheckedMul
 ********************************************************************
 * SYNOPSIS:      Multiply two non-negative longs with an overflow check.
 *
 * PARAMETERS:    a                     First value.
 *                b                     Second value.
 *                *result               Ptr to the product (out).
 *
 * RETURNS:       GBool                 gFalse if a value is negative or the
 *                                      product overflows.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
GBool PdfCheckedMul(long a, long b, long *result)
{
    EC(ECCheckBounds(result));

    if (a < 0 || b < 0 || (a != 0 && b > PDF_SIGNED_LONG_MAX / a))
    {
        return gFalse;
    }

    *result = a *b;
    return gTrue;
}

/********************************************************************
 *                      gmalloc
 ********************************************************************
 * SYNOPSIS:      Allocate fixed memory and set the out-of-memory flag on
 *                failure.
 *
 * PARAMETERS:    size                  Size in bytes.
 *
 * RETURNS:       void *                Ptr to the block, NULL on failure or for
 *                                      size 0.
 *
 * STRATEGY:      malloc() memory is fixed in the GEOS heap: use it only for
 *                small or short-lived data.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void *gmalloc(long size)
{
    void *p;

    if (size == 0)
    {
        return NULL;
    }
    p = malloc(size);
    if (!p)
    {
        gMemOutOfMemory = gTrue;
        EC_WARNING(-1);
    }
    return p;
}

/********************************************************************
 *                      grealloc
 ********************************************************************
 * SYNOPSIS:      Resize a block and set the out-of-memory flag on failure.
 *
 * PARAMETERS:    *p                    Ptr to the block, NULL for a new block.
 *                size                  New size in bytes; 0 frees the block.
 *
 * RETURNS:       void *                Ptr to the block, NULL on failure (the
 *                                      old block stays valid).
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void *grealloc(void *p, long size)
{
    void *q;

    if (size == 0)
    {
        if (p)
        {
            free(p);
        }
        return NULL;
    }
    q = p ? realloc(p, size) : malloc(size);
    if (!q)
    {
        gMemOutOfMemory = gTrue;
        EC_WARNING(-1);
    }
    return q;
}

/********************************************************************
 *                      gfree
 ********************************************************************
 * SYNOPSIS:      Free a block of gmalloc() or grealloc().
 *
 * PARAMETERS:    *p                    Ptr to the block or NULL.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void gfree(void *p)
{
    if (p)
    {
        free(p);
    }
}

/********************************************************************
 *                      copyString
 ********************************************************************
 * SYNOPSIS:      Copy a string into gmalloc() memory.
 *
 * PARAMETERS:    *s                    Ptr to the string.
 *
 * RETURNS:       char *                Ptr to the copy, NULL if there is no
 *                                      memory.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
char *copyString(char *s)
{
    char *s1;

    s1 = (char *)gmalloc(strlen(s) + 1);
    if (!s1)
    {
        return NULL;
    }
    strcpy(s1, s);
    return s1;
}

