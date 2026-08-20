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

#ifndef _STREAM_H
#define _STREAM_H

#ifdef __GNUC__
#pragma interface
#endif


#include "gtypes.h"


//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

  // Constructor.
extern
void StreamInit(Stream *stream);

  // Destructor.
extern
void StreamFree(Stream *stream);

  // Reference counting.
#define StreamIncRef(stream) ((long)++((stream)->ref))
#define StreamDecRef(stream) ((long)--((stream)->ref))

extern
FileHandle StreamGetFile(Stream *str);

extern
long StreamGetChar(Stream *str);
extern
word StreamRead(Stream *str, Guchar *buf, word size);
extern
long StreamLookChar(Stream *str);

extern
Dict *StreamGetDict(Stream *str);

extern
void StreamReset(Stream *str);

extern
long StreamGetLength(Stream *str);
extern
long StreamGetPos(Stream *str);
extern
void StreamSetPos(Stream *str, long pos1);

extern
Stream *StreamAddFilters(Stream *str1, Obj *dict, XRef *xref);

extern
GBool StreamIsBinary(Stream *str, GBool last);

extern
void StreamGetLine(Stream *str, char *buf, long size);

extern
GBool StreamResetImage(Stream *stream, long width1, short nComps1, short nBits1);

extern
GBool StreamGetImagePixel(Stream *this, Guchar *pix);


//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------


void FStreamInit(Stream *this, FileHandle f1, long start1, long length1, Obj *dict1);
void FStreamInitWithStorage(Stream *this, FStream *storage, FileHandle f1,
                            long start1, long length1, Obj *dict1);
void FStreamInvalidateReadCache(FileHandle file);
void FStreamFree(Stream *this);

//  virtual StreamKind getKind() { return strFile; }

void FStreamReset(Stream *str);
void FStreamSetPos(Stream *str, long pos1);

  // Check for a PDF header on this stream.  Skip past some garbage
  // if necessary.


GBool FStreamIsBinary(Stream *str);
long FStreamGetChar(Stream *str);
long FStreamLookChar(Stream *str);
long FStreamGetLength(Stream *str);
long FStreamGetPos(Stream *str);
FileHandle FStreamGetFile(Stream *str);
Dict *FStreamGetDict(Stream *str);


//------------------------------------------------------------------------
// SubStream
//------------------------------------------------------------------------

void SubStreamInit(Stream *str, Stream *str1, Obj *dict1);
void SubStreamFree(Stream *str);
void SubStreamReset(Stream *str);


//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------


extern
void LZWStreamInit(Stream *str, Stream *str2, long predictor1, long columns1, long colors1,
		     long bits1, long early1);
extern
void LZWStreamFree(Stream *str);
extern
void LZWStreamReset(Stream *str);
extern
long LZWStreamGetChar(Stream *str);
extern
long LZWStreamLookChar(Stream *str);



//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

extern
void FlateStreamInit(Stream *str, Stream *str1, long predictor1, long columns1,
			 long colors1, long bits1);
extern
void FlateStreamFree(Stream *str);
extern
void FlateStreamReset(Stream *str);
extern
long FlateStreamGetChar(Stream *str);
extern
word FlateStreamRead(Stream *str, Guchar *buf, word size);
extern
long FlateStreamLookChar(Stream *str);
extern


//------------------------------------------------------------------------
// ObjStmCacheStream
//------------------------------------------------------------------------

GBool ObjStmCacheStreamInit(Stream *str, VMFileHandle vmFile,
                             VMBlockHandle data, long start, long length);
void ObjStmCacheStreamFree(Stream *str);
void ObjStmCacheStreamReset(Stream *str);
long ObjStmCacheStreamGetChar(Stream *str);
long ObjStmCacheStreamLookChar(Stream *str);
long ObjStmCacheStreamGetLength(Stream *str);
long ObjStmCacheStreamGetPos(Stream *str);
void ObjStmCacheStreamSetPos(Stream *str, long pos);


//------------------------------------------------------------------------
// RC4Stream
//------------------------------------------------------------------------

/*
 * Only RC4StreamInit needs to be visible outside stream.goc --
 * RC4StreamFree/Reset/LookChar/GetChar are all static (file-local),
 * dispatched to internally via the stream's own vtable, not called
 * directly from elsewhere.
 */
void RC4StreamInit(Stream *str, Stream *str1, Guchar *key, short keyLen);


//------------------------------------------------------------------------
// EOFStream
//------------------------------------------------------------------------

void EOFStreamInit(Stream *str, Stream *str2);

void EOFStreamFree(Stream *str);

void EOFStreamReset(Stream *str);

long EOFStreamGetChar(Stream *str);

long EOFStreamLookChar(Stream *str);


#endif  /* _STREAM_H */
