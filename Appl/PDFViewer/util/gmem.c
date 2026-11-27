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

#ifdef DEBUG_MEM
typedef struct _GMemHdr {
    long size;
    long index;
    struct _GMemHdr *next;
} GMemHdr;

#define gMemHdrSize ((sizeof(GMemHdr)+ 7) & ~7)
#define gMemTrlSize (sizeof(long))

/* the trailer is a long, which is 4 bytes on GEOS */
#define gMemDeadVal 0xdeadbeefL

/* round data size so trailer will be aligned */
#define gMemDataSize(size) \
    ((((size) + gMemTrlSize - 1) / gMemTrlSize) * gMemTrlSize)

static GMemHdr *gMemList = NULL;
static long gMemIndex = 0;
static long gMemAlloc = 0;
#endif

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
 *                small or short-lived data. DEBUG_MEM: header and trailer for
 *                checks.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void *gmalloc(long size)
{
#ifdef DEBUG_MEM
    long size1;
    char *mem;
    GMemHdr *hdr;
    void *data;
    long *trl, *p;

    if (size == 0)
    {
        return NULL;
    }
    size1 = gMemDataSize(size);
    if (!(mem = (char *)malloc(size1 + gMemHdrSize + gMemTrlSize)))
    {
        gMemOutOfMemory = gTrue;
        EC_WARNING(-1);
        return NULL;
    }
    hdr = (GMemHdr *)mem;
    data = (void *)(mem + gMemHdrSize);
    trl = (long *)(mem + gMemHdrSize + size1);
    hdr->size = size;
    hdr->index = gMemIndex++;
    hdr->next = gMemList;
    gMemList = hdr;
    ++gMemAlloc;
    /* Fill data and trailer with a pattern to find overruns */
    for (p = (long *)data; p <= trl; ++p)
    {
        *p = gMemDeadVal;
    }
    return data;
#else
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
#endif
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
#ifdef DEBUG_MEM
    GMemHdr *hdr;
    void *q;
    long oldSize;

    if (size == 0)
    {
        if (p)
        {
            gfree(p);
        }
        return NULL;
    }
    if (p)
    {
        hdr = (GMemHdr *)((char *)p - gMemHdrSize);
        oldSize = hdr->size;
        q = gmalloc(size);
        if (!q)
        {
            return NULL;
        }
        memcpy(q, p, size < oldSize ? size : oldSize);
        gfree(p);
    }
    else
    {
        q = gmalloc(size);
    }
    return q;
#else
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
#endif
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
 * STRATEGY:      DEBUG_MEM: check the trailer and the block list.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void gfree(void *p)
{
#ifdef DEBUG_MEM
    long size;
    GMemHdr *hdr;
    GMemHdr *prevHdr, *q;
    long *trl, *clr;

    if (p)
    {
        hdr = (GMemHdr *)((char *)p - gMemHdrSize);
        for (prevHdr = NULL, q = gMemList; q; prevHdr = q, q = q->next)
        {
            if (q == hdr)
            {
                break;
            }
        }
        if (q)
        {
            if (prevHdr)
            {
                prevHdr->next = hdr->next;
            }
            else
            {
                gMemList = hdr->next;
            }
            --gMemAlloc;
            size = gMemDataSize(hdr->size);
            trl = (long *)((char *)hdr + gMemHdrSize + size);
            if (*trl != gMemDeadVal)
            {
                /* Block overrun: the trailer is changed */
                EC_WARNING(-1);
            }
            /* Destroy the block to find use after free */
            for (clr = (long *)hdr; clr <= trl; ++clr)
            {
                *clr = gMemDeadVal;
            }
            free(hdr);
        }
        else
        {
            /* Not a gmalloc() block */
            EC_WARNING(-1);
        }
    }
#else
    if (p)
    {
        free(p);
    }
#endif
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

