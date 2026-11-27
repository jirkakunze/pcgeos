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
 *      Port of Derek Noonburg's "GString" class from xpdf 0.8:
 *      a simple variable-length string.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include <Ansi/string.h>
#include "pdfGeode.h"
#include "gstr.h"
#include "gmem.h"

static char gEmptyString[] = "";

/********************************************************************
 *                      GStrRoundSize
 ********************************************************************
 * SYNOPSIS:      Return the buffer size for a string length.
 *
 * PARAMETERS:    len                   String length without the NUL.
 *
 * RETURNS:       long                  len + 1, rounded up to 8 bytes (short
 *                                      strings) or 256 bytes.
 *
 * STRATEGY:      Rounding avoids a grealloc() for each appended character.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static long GStrRoundSize(long len)
{
    long delta;

    delta = len < 256 ? 7 : 255;
    return((len + 1) + delta) & ~delta;
}

/********************************************************************
 *                      resize
 ********************************************************************
 * SYNOPSIS:      Make the buffer large enough for a new length.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                length1               New string length.
 *
 * RETURNS:       GBool                 gFalse if there is no memory; the old
 *                                      buffer stays.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static GBool resize(GooString *gstr, long length1)
{
    char *newString;

    if (!gstr->s)
    {
        newString = gmalloc(GStrRoundSize(length1));
    }
    else if (GStrRoundSize(length1) != GStrRoundSize(gstr->length))
    {
        newString = grealloc(gstr->s, GStrRoundSize(length1));
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

/********************************************************************
 *                      GStrInit
 ********************************************************************
 * SYNOPSIS:      Initialize an empty string.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: s stays NULL; GStrGetCString() then returns "".
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void GStrInit(GooString *gstr)
{
    gstr->s = NULL;
    if (!resize(gstr, (gstr->length = 0)))
    {
        return;
    }
    gstr->s[0] = '\0';
}

/********************************************************************
 *                      GStrInitString
 ********************************************************************
 * SYNOPSIS:      Initialize a string with a copy of a C string.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                *s1                   Ptr to the C string.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: the length is 0.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      GStrInitGS
 ********************************************************************
 * SYNOPSIS:      Initialize a string with a copy of a string.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                *str                  Ptr to the string to copy.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: the length is 0.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      GStrFree
 ********************************************************************
 * SYNOPSIS:      Free the buffer of a string.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
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
void GStrFree(GooString *gstr)
{
    gfree(gstr->s);
    gstr->s = NULL;
    gstr->length = 0;
}

/********************************************************************
 *                      GStrAppendChar
 ********************************************************************
 * SYNOPSIS:      Append one character.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                c                     Character.
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
void GStrAppendChar(GooString *gstr, char c)
{
    if (!resize(gstr, gstr->length + 1))
    {
        return;
    }
    gstr->s[gstr->length++] = c;
    gstr->s[gstr->length] = '\0';
}

/********************************************************************
 *                      GStrAppendString
 ********************************************************************
 * SYNOPSIS:      Append a C string.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                *str                  Ptr to the C string.
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

/********************************************************************
 *                      GStrAppendStringN
 ********************************************************************
 * SYNOPSIS:      Append bytes that need not end with NUL.
 *
 * PARAMETERS:    *gstr                 Ptr to the string.
 *                *str                  Ptr to the bytes.
 *                length1               Number of bytes.
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

long GStrGetLength(GooString *gstr)
{
    return gstr->length;
}

char *GStrGetCString(GooString *gstr)
{
    return gstr->s ? gstr->s : gEmptyString;
}

