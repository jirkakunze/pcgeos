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

#if gmemTrlSize==8
#define gMemDeadVal 0xdeadbeefdeadbeef
#else
#define gMemDeadVal 0xdeadbeef

/* round data size so trailer will be aligned */
#define gMemDataSize(size) \
    ((((size) + gMemTrlSize - 1) / gMemTrlSize) * gMemTrlSize)

#endif

static GMemHdr *gMemList = NULL;
static long gMemIndex = 0;
static long gMemAlloc = 0;
#endif

static GBool gMemOutOfMemory = gFalse;

/*
 * GMem 
 */

/***********************************************************************
 *      GMemClearError
 ***********************************************************************
 * SYNOPSIS:        Clear error.
 * PARAMETERS:      none
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Clear the flag directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GMemClearError(void)
{
    gMemOutOfMemory = gFalse;
}

/***********************************************************************
 *      GMemHadError
 ***********************************************************************
 * SYNOPSIS:        Process gmem had error.
 * PARAMETERS:      none
 *
 * RETURNS:         success flag
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
GBool GMemHadError(void)
{
    return gMemOutOfMemory;
}

/***********************************************************************
 *      GMemSetError
 ***********************************************************************
 * SYNOPSIS:        Set error.
 * PARAMETERS:      none
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
void GMemSetError(void)
{
    gMemOutOfMemory = gTrue;
}

#define PDF_SIGNED_LONG_MAX 0x7fffffffL

/***********************************************************************
 *      PdfCheckedAdd
 ***********************************************************************
 * SYNOPSIS:        Add.
 * PARAMETERS:      long a    a
 *                  long b    b
 *                  long *result    result
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Reject negatives, then check a > MAX - b before adding, which is
 *      overflow-safe since b is already known non-negative.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
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

/***********************************************************************
 *      PdfCheckedMul
 ***********************************************************************
 * SYNOPSIS:        Process pdf checked mul.
 * PARAMETERS:      long a    a
 *                  long b    b
 *                  long *result    result
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Reject negatives, then check b > MAX / a before multiplying (a
 *      == 0 skips the check since 0 * b never overflows);...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
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

/***********************************************************************
 *      gmalloc
 ***********************************************************************
 * SYNOPSIS:        Process gmalloc.
 * PARAMETERS:      long size    size
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Plain build: malloc() directly, setting the sticky error flag on
 *      failure. DEBUG_MEM build: wraps the block in a...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void *gmalloc(long size)
{
#if DEBUG_MEM
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

/***********************************************************************
 *      grealloc
 ***********************************************************************
 * SYNOPSIS:        Process grealloc.
 * PARAMETERS:      void *p    pointer
 *                  long size    size
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Plain build: realloc() directly (malloc() if p is NULL), setting
 *      the sticky error flag on failure. DEBUG_MEM build:...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void *grealloc(void *p, long size)
{
#if DEBUG_MEM
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

/***********************************************************************
 *      gfree
 ***********************************************************************
 * SYNOPSIS:        Process gfree.
 * PARAMETERS:      void *p    pointer
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Plain build: free() directly, NULL-tolerant. DEBUG_MEM build:
 *      unlinks the block from gMemList, checks its trailer...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
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
                fprintf(stderr,
                    "Overwrite past end of block %d at address %p\n",
                    hdr->index, p);
            }
            for (clr = (long *)hdr; clr <= trl; ++clr)
            {
                *clr = gMemDeadVal;
            }
            free(hdr);
        }
        else
        {
            fprintf(stderr, "Attempted to free bad address %p\n", p);
        }
    }
#else
    if (p)
    {
        free(p);
    }
#endif
}

/***********************************************************************
 *      copyString
 ***********************************************************************
 * SYNOPSIS:        Copy string.
 * PARAMETERS:      char *s    s
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      gmalloc() strlen(s)+1 bytes, then strcpy().
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
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

