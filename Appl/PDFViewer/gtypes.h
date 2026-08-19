/***********************************************************************
 *
 * gtypes.h
 *
 * Copyright 1996 Derek B. Noonburg
 * Modifications Copyright 2026 Jirka Kunze/FreeGEOS Project
 *
 * This file is derived from the original Xpdf source code and has been
 * modified for use in the PC/GEOS PDF Viewer.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***********************************************************************/

#ifndef _GTYPES_H
#define _GTYPES_H


#include <geos.h>

/*
 * These have stupid names to avoid conflicts with some (but not all)
 * C++ compilers which define them.
 */
typedef int GBool;
#define gTrue 1
#define gFalse 0

/*
 * Part of my plan to convert doubles to WWFixedAsDWord for enhanced performance:
 */
#define USE_NATIVE_FLOAT_TYPE

#ifdef USE_NATIVE_FLOAT_TYPE

typedef sdword gdouble;
#define IntToGdouble(x) MakeWWFixed(x)
#define GdoubleToWWFixed(x) (x)
#define GdoubleToWord(x) ((x) >> 16)

#else

typedef double gdouble;
#define IntToGdouble(x) ((double) (x))
#define GdoubleToWWFixed(x) MakeWWFixed(x)
#define GdoubleToWord(x) ((word) (x))

#endif

/*
 * These have stupid names to avoid conflicts with <sys/types.h>,
 * which on various systems defines some random subset of these.
 */
typedef unsigned char Guchar;
typedef unsigned short Gushort;
typedef unsigned int Guint;
typedef unsigned long Gulong;

#endif  /* _GTYPES_H */
