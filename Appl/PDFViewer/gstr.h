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

#ifndef _GSTR_H
#define _GSTR_H

#ifdef __GNUC__
#pragma interface
#endif


#include "gtypes.h"


void GStrInit(GooString *gstr);

void GStrInitString(GooString *gstr, char *s1);

void GStrInitGS(GooString *gstr, GooString *str);

void GStrFree(GooString *gstr);

void GStrAppendChar(GooString *gstr, char c);

void GStrAppendString(GooString *gstr, char *str);

void GStrAppendStringN(GooString *gstr, char *str, long length1);

long GStrGetLength(GooString *gstr);

char *GStrGetCString(GooString *gstr);

long GStrCmpString(GooString *gstr, char *s1);


#endif  /* _GSTR_H */
