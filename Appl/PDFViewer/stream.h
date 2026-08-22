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
#define StreamIncRef(stream) ((long)++((stream)->ref))
#define StreamDecRef(stream) ((long)--((stream)->ref))

/* Get file. */
extern
FileHandle StreamGetFile(Stream *str);

/* Get char. */
extern
long StreamGetChar(Stream *str);

/* Read. */
extern
word StreamRead(Stream *str, Guchar *buf, word size);

/* Process stream look char. */
extern
long StreamLookChar(Stream *str);

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

/* Check binary. */
extern
GBool StreamIsBinary(Stream *str, GBool last);

/* Get line. */
extern
void StreamGetLine(Stream *str, char *buf, long size);

/* Reset image. */
extern
GBool StreamResetImage(Stream *stream, long width1, short nComps1,
    short nBits1);

/* Get image pixel. */
extern
GBool StreamGetImagePixel(Stream *this, Guchar *pix);

/* FileStream */

void FStreamInit(Stream *this, FileHandle f1, long start1, long length1,
    Obj *dict1);
/* Initialize with storage. */
void FStreamInitWithStorage(Stream *this, FStream *storage, FileHandle f1,
    long start1, long length1, Obj *dict1);
    
/* Read cache. */
void FStreamInvalidateReadCache(FileHandle file);

/* Release. */
void FStreamFree(Stream *this);

/* Reset. */
void FStreamReset(Stream *str);
void FStreamSetPos(Stream *str, long pos1);

/* Check for a PDF header on this stream. */

GBool FStreamIsBinary(Stream *str);

/* Get char. */
long FStreamGetChar(Stream *str);

/* Handle fstream look char. */
long FStreamLookChar(Stream *str);

/* Get length. */
long FStreamGetLength(Stream *str);

/* Get position. */
long FStreamGetPos(Stream *str);

/* Get file. */
FileHandle FStreamGetFile(Stream *str);

/* Get dictionary. */
Dict *FStreamGetDict(Stream *str);

/* SubStream */

void SubStreamInit(Stream *str, Stream *str1, Obj *dict1);
/* Release. */
void SubStreamFree(Stream *str);
/* Reset. */
void SubStreamReset(Stream *str);

/* LZWStream */

extern
void LZWStreamInit(Stream *str, Stream *str2, long predictor1, long columns1,
    long colors1,
    long bits1, long early1);
    
/* Release. */
extern
void LZWStreamFree(Stream *str);

/* Reset. */
extern
void LZWStreamReset(Stream *str);

/* Get char. */
extern
long LZWStreamGetChar(Stream *str);

/* Process lzwstream look char. */
extern
long LZWStreamLookChar(Stream *str);

/* FlateStream */

extern
void FlateStreamInit(Stream *str, Stream *str1, long predictor1, long columns1,
    long colors1, long bits1);
/* Release. */
extern
/* Release. */
void FlateStreamFree(Stream *str);
/* Reset. */
extern
/* Reset. */
void FlateStreamReset(Stream *str);
/* Get char. */
extern
long FlateStreamGetChar(Stream *str);
/* Read. */
extern
word FlateStreamRead(Stream *str, Guchar *buf, word size);
/* Process flate stream look char. */
extern
/* Handle flate stream look char. */
long FlateStreamLookChar(Stream *str);
/* Initialize. */
extern

/* ObjStmCacheStream */

GBool ObjStmCacheStreamInit(Stream *str, VMFileHandle vmFile,
    VMBlockHandle data, long start, long length);
    
/* Release. */
void ObjStmCacheStreamFree(Stream *str);

/* Reset. */
void ObjStmCacheStreamReset(Stream *str);

/* Get char. */
long ObjStmCacheStreamGetChar(Stream *str);

/* Handle obj stm cache stream look char. */
long ObjStmCacheStreamLookChar(Stream *str);

/* Get length. */
long ObjStmCacheStreamGetLength(Stream *str);

/* Get position. */
long ObjStmCacheStreamGetPos(Stream *str);

/* Set position. */
void ObjStmCacheStreamSetPos(Stream *str, long pos);

/* RC4Stream */

/* Initialize. */
void RC4StreamInit(Stream *str, Stream *str1, Guchar *key, short keyLen);

/* EOFStream */

void EOFStreamInit(Stream *str, Stream *str2);

/* Release. */
void EOFStreamFree(Stream *str);

/* Reset. */
void EOFStreamReset(Stream *str);

/* Get char. */
long EOFStreamGetChar(Stream *str);

/* Handle eofstream look char. */
long EOFStreamLookChar(Stream *str);

#endif  /* STREAM_H */

