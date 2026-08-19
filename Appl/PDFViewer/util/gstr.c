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

long size(long len) 
{
  long  delta;

  delta = len < 256 ? 7 : 255;
  return ((len + 1) + delta) & ~delta;
}

static GBool resize(GooString *gstr, long length1)
{
  char  *newString;

  if (!gstr->s)
    newString = gmalloc(size(length1));
  else if (size(length1) != size(gstr->length))
    newString = grealloc(gstr->s, size(length1));
  else
    return gTrue;
  if (!newString)
    return gFalse;
  gstr->s = newString;
  return gTrue;
}

void GStrInit(GooString *gstr)
{
  gstr->s = NULL;
  if (!resize(gstr, (gstr->length = 0)))
    return;
  gstr->s[0] = '\0';
}

void GStrInitString(GooString *gstr, char *s1)
{
  long n = strlen(s1);

  gstr->s = NULL;
  if (!resize(gstr, gstr->length = n)) { gstr->length = 0; return; }
  memcpy(gstr->s, s1, n + 1);
}

void GStrInitGS(GooString *gstr, GooString *str)
{
  gstr->s = NULL;
  if (!resize(gstr, gstr->length = GStrGetLength(str))) { gstr->length = 0; return; }
  memcpy(gstr->s, GStrGetCString(str), gstr->length + 1);
}

void GStrFree(GooString *gstr)
{
  gfree(gstr->s);
  gstr->s = NULL;
  gstr->length = 0;
}

void GStrAppendChar(GooString *gstr, char c)
{
  if (!resize(gstr, gstr->length + 1)) return;
  gstr->s[gstr->length++] = c;
  gstr->s[gstr->length] = '\0';
}

void GStrAppendString(GooString *gstr, char *str) {
  long n = strlen(str);

  if (!resize(gstr, gstr->length + n)) return;
  memcpy(gstr->s + gstr->length, str, n + 1);
  gstr->length += n;
}

void GStrAppendStringN(GooString *gstr, char *str, long length1)
{
  if (!resize(gstr, gstr->length + length1)) return;
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


long GStrCmpString(GooString *gstr, char *s1) 
{ 
	return strcmp(GStrGetCString(gstr), s1); 
}
