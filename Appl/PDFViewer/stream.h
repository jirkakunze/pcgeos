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
 * FILE:          stream.h
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
 *      Port of Derek Noonburg's "Stream" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef STREAM_H
#define STREAM_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

/* Constructor. */
extern
void StreamInit(Stream *stream);

/* Destructor. */
extern
void StreamFree(Stream *stream);

/* Reference counting. */
#define StreamIncRef(stream) ((word)++((stream)->ref))
#define StreamDecRef(stream) ((word)--((stream)->ref))

/* Get file. */
extern
FileHandle StreamGetFile(Stream *str);

/* Get char. */
extern
short StreamGetChar(Stream *str);

/* Read. */
extern
word StreamRead(Stream *str, Guchar *buf, word size);

/* Process stream look char. */
extern
short StreamLookChar(Stream *str);

/* Read the byte that StreamLookChar returned last (fast for the lexer). */
extern
void StreamSkipLookedChar(Stream *str);

/* Get dictionary. */
extern
Dict *StreamGetDict(Stream *str);

/* Reset. */
extern
void StreamReset(Stream *str);

/* Get length. */
extern
long StreamGetLength(Stream *str);

/* Get position. */
extern
long StreamGetPos(Stream *str);

extern
void StreamSetPos(Stream *str, long pos1);

/* Add filters. */
extern
Stream *StreamAddFilters(Stream *str1, Obj *dict, XRef *xref);

/* Get line. */
extern
void StreamGetLine(Stream *str, char *buf, word size);

/* Reset image. */
extern
GBool StreamResetImage(Stream *stream, word width1, short nComps1,
    short nBits1);

/* Get image pixel. */
extern
GBool StreamGetImagePixel(Stream *this, Guchar *pix);

void FStreamInit(Stream *this, FileHandle f1, long start1, long length1,
    Obj *dict1);

/* Read cache. */
void FStreamInvalidateReadCache(FileHandle file);

/* Get char. */
short FStreamGetChar(Stream *str);

/* Get position. */
long FStreamGetPos(Stream *str);

void SubStreamInit(Stream *str, Stream *str1, Obj *dict1);

/* DCTStream (main/dctStream.c, IJG JPEG library) */

extern
void DCTStreamInit(Stream *str, Stream *str1);
/* decode scale (1, 2, 4, 8) of a JPEG of this size, see GfxDrawImage */
word DCTStreamScaleFor(long w, long h);
extern
void DCTStreamFree(Stream *str);
extern
void DCTStreamReset(Stream *str);
extern
short DCTStreamGetChar(Stream *str);
extern
short DCTStreamLookChar(Stream *str);
extern
word DCTStreamRead(Stream *str, Guchar *buf, word size);

/* Initialize. */
void RC4StreamInit(Stream *str, Stream *str1, Guchar *key, short keyLen);

void EOFStreamInit(Stream *str, Stream *str2);

#endif  /* STREAM_H */

