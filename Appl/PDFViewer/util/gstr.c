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
 * FILE:          gstr.c
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
 *      Port of Derek Noonburg's "GString" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include <Ansi/string.h>
#include "pdfGeode.h"
#include "gstr.h"
#include "gmem.h"

static char gEmptyString[] = "";

/*
 * GooString
 */

/***********************************************************************
 *      size
 ***********************************************************************
 * SYNOPSIS:        Process size.
 * PARAMETERS:      long len    length
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Round len+1 up to the next multiple of 8 for short strings
 *      (<256), or 256 for longer ones, via a bitmask on the...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
long size(long len)
{
    long delta;

    delta = len < 256 ? 7 : 255;
    return((len + 1) + delta) & ~delta;
}

/***********************************************************************
 *      resize
 ***********************************************************************
 * SYNOPSIS:        Process resize.
 * PARAMETERS:      GooString *gstr    string
 *                  long length1    length1
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static GBool resize(GooString *gstr, long length1)
{
    char *newString;

    if (!gstr->s)
    {
        newString = gmalloc(size(length1));
    }
    else if (size(length1) != size(gstr->length))
    {
        newString = grealloc(gstr->s, size(length1));
    }
    else
    {
        return gTrue;
    }
    if (!newString)
    {
        return gFalse;
    }
    gstr->s = newString;
    return gTrue;
}

/***********************************************************************
 *      GStrInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      GooString *gstr    string
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Allocate the minimum block via resize() and write a leading NUL.
 *      On allocation failure, gstr is left with s == NULL...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrInit(GooString *gstr)
{
    gstr->s = NULL;
    if (!resize(gstr, (gstr->length = 0)))
    {
        return;
    }
    gstr->s[0] = '\0';
}

/***********************************************************************
 *      GStrInitString
 ***********************************************************************
 * SYNOPSIS:        Initialize string.
 * PARAMETERS:      GooString *gstr    string
 *                  char *s1    s1
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Measure s1, allocate via resize(), then copy including the NUL.
 *      On allocation failure, gstr->length is reset to 0.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrInitString(GooString *gstr, char *s1)
{
    long n = strlen(s1);

    gstr->s = NULL;
    if (!resize(gstr, gstr->length = n))
    {
        gstr->length = 0;
        return;
    }
    memcpy(gstr->s, s1, n + 1);
}

/***********************************************************************
 *      GStrInitGS
 ***********************************************************************
 * SYNOPSIS:        Initialize gs.
 * PARAMETERS:      GooString *gstr    string
 *                  GooString *str    string
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Allocate via resize() to str's length, then copy including the
 *      NUL. On allocation failure, gstr->length is reset to 0.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrInitGS(GooString *gstr, GooString *str)
{
    gstr->s = NULL;
    if (!resize(gstr, gstr->length = GStrGetLength(str)))
    {
        gstr->length = 0;
        return;
    }
    memcpy(gstr->s, GStrGetCString(str), gstr->length + 1);
}

/***********************************************************************
 *      GStrFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      GooString *gstr    string
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
void GStrFree(GooString *gstr)
{
    gfree(gstr->s);
    gstr->s = NULL;
    gstr->length = 0;
}

/***********************************************************************
 *      GStrAppendChar
 ***********************************************************************
 * SYNOPSIS:        Append char.
 * PARAMETERS:      GooString *gstr    string
 *                  char c    c
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Grow via resize() if needed, then write c and a new terminating
 *      NUL.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrAppendChar(GooString *gstr, char c)
{
    if (!resize(gstr, gstr->length + 1))
    {
        return;
    }
    gstr->s[gstr->length++] = c;
    gstr->s[gstr->length] = '\0';
}

/***********************************************************************
 *      GStrAppendString
 ***********************************************************************
 * SYNOPSIS:        Append string.
 * PARAMETERS:      GooString *gstr    string
 *                  char *str    string
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Grow via resize() if needed, then copy str including its NUL.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrAppendString(GooString *gstr, char *str)
{
    long n = strlen(str);

    if (!resize(gstr, gstr->length + n))
    {
        return;
    }
    memcpy(gstr->s + gstr->length, str, n + 1);
    gstr->length += n;
}

/***********************************************************************
 *      GStrAppendStringN
 ***********************************************************************
 * SYNOPSIS:        Append string n.
 * PARAMETERS:      GooString *gstr    string
 *                  char *str    string
 *                  long length1    length1
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Grow via resize() if needed, copy exactly length1 bytes, then
 *      write a terminating NUL after them.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void GStrAppendStringN(GooString *gstr, char *str, long length1)
{
    if (!resize(gstr, gstr->length + length1))
    {
        return;
    }
    memcpy(gstr->s + gstr->length, str, length1);
    gstr->length += length1;
    gstr->s[gstr->length] = '\0';
}

/***********************************************************************
 *      GStrGetLength
 ***********************************************************************
 * SYNOPSIS:        Get length.
 * PARAMETERS:      GooString *gstr    string
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
long GStrGetLength(GooString *gstr)
{
    return gstr->length;
}

/***********************************************************************
 *      GStrGetCString
 ***********************************************************************
 * SYNOPSIS:        Get cstring.
 * PARAMETERS:      GooString *gstr    string
 *
 * RETURNS:         result pointer
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
char *GStrGetCString(GooString *gstr)
{
    return gstr->s ? gstr->s : gEmptyString;
}

/***********************************************************************
 *      GStrCmpString
 ***********************************************************************
 * SYNOPSIS:        Process gstr cmp string.
 * PARAMETERS:      GooString *gstr    string
 *                  char *s1    s1
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Delegate to strcmp() via GStrGetCString().
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
long GStrCmpString(GooString *gstr, char *s1)
{
    return strcmp(GStrGetCString(gstr), s1);
}

