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
 * FILE:          obj.h
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
 *      Port of Derek Noonburg's "Object" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef OBJ_H
#define OBJ_H

#ifdef __GNUC__
#pragma interface
#endif

#include "pdfGeode.h"

/* Initialize bool. */
extern
void initBool(Obj *obj, GBool booln1);

/* Initialize int. */
extern
void initInt(Obj *obj, long intg1);

/* Initialize real. */
extern
void initReal(Obj *obj, gdouble real1);

/* Initialize string. */
extern
void initString(Obj *obj, GooString *string1);

/* Initialize name. */
extern
void initName(Obj *obj, char *name1);

#ifdef DEBUG_MEM
/* Initialize null. */
extern
void initNull(Obj *obj);
#else
#define initNull(obj) ((void)((obj)->type = objNull))
#endif

/* Initialize ref. */
extern
void initRef(Obj *obj, long num1, long gen1);

/* Initialize cmd. */
extern
void initCmd(Obj *obj, char *cmd1);

/* Initialize error. */
extern
void initError(Obj *obj);

/* Initialize eof. */
extern
void initEOF(Obj *obj);

/* Initialize array. */
extern
void initArray(Obj *obj);
/* Initialize dictionary. */

extern
void initDict(Obj *obj);

/* Initialize dictionary data. */
extern
void initDictData(Obj *obj, Dict *dict1);

/* Initialize stream. */
extern
void initStream(Obj *obj, Stream *stream1);

/* Move. */
extern
void ObjMove(Obj *dest, Obj *src);

/* Copy. */
void ObjCopy(Obj *obj, Obj *obj2);

/* If object is a Ref, fetch and return the referenced object. */
/* Otherwise, return a copy of the object. */
extern
/* Fetch. */
void ObjFetch(Obj *dest, Obj *obj, XRef *xref);

/* Release. */
extern
void ObjFree(Obj *obj);

/* Type checking. */
#define isBool(obj)   ((obj)->type == objBool)
#define isInt(obj)    ((obj)->type == objInt)
#define isNum(obj)    ((obj)->type == objInt || (obj)->type == objReal)
#define isString(obj) ((obj)->type == objString)
#define isName(obj)   ((obj)->type == objName)
#define isNull(obj)   ((obj)->type == objNull)
#define isArray(obj)  ((obj)->type == objArray)
#define isDict(obj)   ((obj)->type == objDict)
#define isStream(obj) ((obj)->type == objStream)
#define isRef(obj)    ((obj)->type == objRef)
#define isCmd(obj)    ((obj)->type == objCmd)
#define isError(obj)  ((obj)->type == objError)
#define isEOF(obj)    ((obj)->type == objEOF)
#define isNone(obj)   ((obj)->type == objNone)

/* Special type checking. */
extern
GBool isNameSame(Obj *obj, char *name1);
/* Check cmd same. */
extern
GBool isCmdSame(Obj *obj, char *cmd1);

/* Accessors. */
#define getBool(obj)   ((obj)->u.booln)
#define getInt(obj)    ((obj)->u.intg)

/* Get number. */
extern
gdouble getNum(Obj *obj);

/* Get string. */
extern
GooString *getString(Obj *obj);

/* Get name. */
extern
char *getName(Obj *obj);

/* Get array. */
extern
Array *getArray(Obj *obj);

/* Get dictionary. */
extern
Dict *getDict(Obj *obj);

/* Get stream. */
extern
Stream *getStream(Obj *obj);

/* Get ref. */
extern
Ref getRef(Obj *obj);

#define getRefNum(obj) ((obj)->u.ref.num)

#define getRefGen(obj) ((obj)->u.ref.gen)

/* Get length. */
#define ObjArrayGetLength(obj) \
	ArrayGetLength((obj)->u.array)
	
#define ObjArrayAdd(obj, elem) \
	ArrayAdd((obj)->u.array, (elem))
	
#define ObjArrayGet(obj, i, obj2, xref) \
	ArrayGet((obj)->u.array, (i), (obj2), (xref))
	
#define ObjArrayGetNF(obj, i, obj2) \
	ArrayGetNF((obj)->u.array, (i), (obj2))

/* Dict accessors. */
#define ObjDictAdd(obj, key, val) \
	DictAdd((obj)->u.dict, (key), (val))

#define ObjIsDictSame(obj, dictType) \
	((obj)->type == objDict && DictIs((obj)->u.dict, (dictType)))

#define ObjDictLookup(obj, key, obj2, xref) \
	DictLookup((obj)->u.dict, (key), (obj2), (xref))

#define ObjDictLookupNF(obj, key, obj2) \
	DictLookupNF((obj)->u.dict, (key), (obj2))

#define ObjDictGetKey(obj, i) \
	DictGetKey((obj)->u.dict, (i))

/* Stream accessors. */

#define ObjIsStream(obj, dictType) \
((obj)->type == objStream && \
    DictIs(StreamGetDict((obj)->u.stream), (dictType)))

#define ObjStreamReset(obj) \
	StreamReset((obj)->u.stream)

#define ObjStreamGetChar(obj) \
	((int)StreamGetChar((obj)->u.stream))

#define ObjStreamLookChar(obj) \
	((int)StreamLookChar((obj)->u.stream))

#define ObjStreamGetPos(obj) \
	StreamGetPos((obj)->u.stream)

#define ObjStreamSetPos(obj, pos) \
	StreamSetPos((obj)->u.stream, (pos))

#define ObjStreamGetDict(obj) \
	StreamGetDict((obj)->u.stream)

#define ObjStreamGetLine(obj, buf, size) \
	StreamGetLine((obj)->u.stream, (buf), (size))

#ifdef DEBUG_MEM
static long /* number of each type of object */
numAlloc[numObjTypes]; /* currently allocated */
#endif

#endif  /* OBJ_H */

