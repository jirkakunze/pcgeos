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

#ifndef _OBJ_H
#define _OBJ_H

#ifdef __GNUC__
#pragma interface
#endif

#include "pdfGeode.h"


  // Initialize an object.
extern
  void initBool(Obj *obj, GBool booln1);
extern
  void initInt(Obj *obj, long intg1);
extern
  void initReal(Obj *obj, gdouble real1);
extern
  void initString(Obj *obj, GooString *string1);
extern
  void initName(Obj *obj, char *name1);
#ifdef DEBUG_MEM
extern
  void initNull(Obj *obj);
#else
#define initNull(obj) ((void)((obj)->type = objNull))
#endif
extern
  void initRef(Obj *obj, long num1, long gen1);
extern
  void initCmd(Obj *obj, char *cmd1);
extern
  void initError(Obj *obj);
extern
  void initEOF(Obj *obj);

extern
void initArray(Obj *obj);
extern
void initDict(Obj *obj);
extern
void initDictData(Obj *obj, Dict *dict1);
extern
void initStream(Obj *obj, Stream *stream1);


  // Copy an object.
extern
/*
 * Transfer ownership from *src to *dest without ObjCopy's
 * allocation/deep-copy work -- see ObjMove's own doc comment in
 * main/obj.goc for the full contract. Part of project roadmap P1/A1;
 * not yet used anywhere in the project as of this declaration.
 */
void ObjMove(Obj *dest, Obj *src);
void ObjCopy(Obj *obj, Obj *obj2);

  // If object is a Ref, fetch and return the referenced object.
  // Otherwise, return a copy of the object.
extern
void ObjFetch(Obj *dest, Obj *obj, XRef *xref);

  // Free object contents.
extern
void ObjFree(Obj *obj);

  // Type checking.  SEG-05A: single-field tests stay local.
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

  // Special type checking.
extern
  GBool isNameSame(Obj *obj, char *name1);
extern
  GBool isCmdSame(Obj *obj, char *cmd1);

  // Accessors.  NB: these assume object is of correct type.
#define getBool(obj)   ((obj)->u.booln)
#define getInt(obj)    ((obj)->u.intg)
extern
  gdouble getNum(Obj *obj);
extern
  GooString *getString(Obj *obj);
extern
  char *getName(Obj *obj);
extern
  Array *getArray(Obj *obj);
extern
  Dict *getDict(Obj *obj);
extern
  Stream *getStream(Obj *obj);
extern
  Ref getRef(Obj *obj);
#define getRefNum(obj) ((obj)->u.ref.num)
#define getRefGen(obj) ((obj)->u.ref.gen)


  // Array accessors.  SEG-05C: bypass the obj resource for pure forwarders.
#define ObjArrayGetLength(obj) \
  ArrayGetLength((obj)->u.array)
#define ObjArrayAdd(obj, elem) \
  ArrayAdd((obj)->u.array, (elem))
#define ObjArrayGet(obj, i, obj2, xref) \
  ArrayGet((obj)->u.array, (i), (obj2), (xref))
#define ObjArrayGetNF(obj, i, obj2) \
  ArrayGetNF((obj)->u.array, (i), (obj2))


  // Dict accessors.
#define ObjDictAdd(obj, key, val) \
  DictAdd((obj)->u.dict, (key), (val))
/* SEG-05C2: bypass obj resource for the remaining small typed wrappers. */
#define ObjIsDictSame(obj, dictType) \
  ((obj)->type == objDict && DictIs((obj)->u.dict, (dictType)))
#define ObjDictLookup(obj, key, obj2, xref) \
  DictLookup((obj)->u.dict, (key), (obj2), (xref))
#define ObjDictLookupNF(obj, key, obj2) \
  DictLookupNF((obj)->u.dict, (key), (obj2))
#define ObjDictGetKey(obj, i) \
  DictGetKey((obj)->u.dict, (i))


  // Stream accessors.

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
  static long			// number of each type of object
  numAlloc[numObjTypes];	//   currently allocated
#endif


#endif  /* _OBJ_H */
