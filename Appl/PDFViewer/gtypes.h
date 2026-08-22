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
 * FILE:          gtypes.h
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
 *      Port of Derek Noonburg's gtypes.h from xpdf 0.8.
 ***********************************************************************/

#ifndef GTYPES_H
#define GTYPES_H

#include <geos.h>

/*
 * These have stupid names to avoid conflicts with some (but not all) C++
 * compilers which define them.
 */
typedef int GBool;
#define gTrue 1
#define gFalse 0

/*
 * Part of my plan to convert doubles to WWFixedAsDWord for enhanced
 * performance:
 */
#define USE_NATIVE_FLOAT_TYPE

#ifdef USE_NATIVE_FLOAT_TYPE

typedef sdword gdouble;
#define IntToGdouble(x) MakeWWFixed(x)
#define GdoubleToWWFixed(x) (x)
#define GdoubleToWord(x) ((x) >> 16)

#else

typedef double gdouble;
#define IntToGdouble(x) ((double)(x))
#define GdoubleToWWFixed(x) MakeWWFixed(x)
#define GdoubleToWord(x) ((word)(x))

#endif

/*
 * These have stupid names to avoid conflicts with <sys/types.h>, which on
 * various systems defines some random subset of these.
 */
typedef unsigned char Guchar;
typedef unsigned short Gushort;
typedef unsigned int Guint;
typedef unsigned long Gulong;

#endif  /* GTYPES_H */

