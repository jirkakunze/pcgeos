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
 * FILE:          gstr.h
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

#ifndef GSTR_H
#define GSTR_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

/*
 * GooString
 */

extern void GStrInit(GooString *gstr);

/* Initialize string. */
extern void GStrInitString(GooString *gstr, char *s1);

/* Initialize gs. */
extern void GStrInitGS(GooString *gstr, GooString *str);

/* Release storage owned by a string. */
extern void GStrFree(GooString *gstr);

/* Append a single character. */
extern void GStrAppendChar(GooString *gstr, char c);

/* Append a NUL-terminated C string. */
extern void GStrAppendString(GooString *gstr, char *str);

/* Append length1 bytes from str, which need not be NUL-terminated. */
extern void GStrAppendStringN(GooString *gstr, char *str, long length1);

/* Get length. */
extern long GStrGetLength(GooString *gstr);

/* Get cstring. */
extern char *GStrGetCString(GooString *gstr);

/* Compare against a C string, per strcmp(). */
extern long GStrCmpString(GooString *gstr, char *s1);

#endif  /* GSTR_H */

