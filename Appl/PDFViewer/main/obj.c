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
 * FILE:          obj.c
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

#ifdef __GNUC__
#pragma implementation
#endif


#include <Ansi/string.h>
#include "obj.h"
#include "array.h"
#include "dict.h"
#include "xref.h"
#include "stream.h"
#include "gmem.h"
#include "gstr.h"
#include "pdfGeode.h"


//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

/*
char *objTypeNames[numObjTypes] = {
  "boolean",
  "integer",
  "real",
  "string",
  "name",
  "null",
  "array",
  "dictionary",
  "stream",
  "ref",
  "cmd",
  "error",
  "eof",
  "none"
};
*/

#ifdef DEBUG_MEM
long Object::numAlloc[numObjTypes] =
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif

#ifdef DEBUG_MEM
#define initObj(t) ++numAlloc[obj->type = t]
#else
#define initObj(t) obj->type = t
#endif


void initArray(Obj *obj) {
  initObj(objArray);
  obj->u.array = gmalloc( sizeof (Array) );
  if (!obj->u.array) {
    initError(obj);
    return;
  }
  ArrayInit(obj->u.array);
}

void initDict(Obj *obj) {
  initObj(objDict);
  obj->u.dict = gmalloc( sizeof (Dict) );
  if (!obj->u.dict) {
    initError(obj);
    return;
  }
  DictInit(obj->u.dict);
}

void initStream(Obj *obj, Stream *stream1) {
  initObj(objStream);
  obj->u.stream = stream1;
}


  // Copy an object.  (obj <- obj2)
/***********************************************************************
 *		ObjMove
 ***********************************************************************
 * SYNOPSIS:	    Transfer an Obj's contents to another Obj, without
 *		    the allocation/deep-copy work ObjCopy does -- for
 *		    the very common case throughout this project where
 *		    a value gets ObjCopy'd somewhere and the source is
 *		    ObjFree'd immediately after, with the source never
 *		    used again in between. That pattern does real,
 *		    avoidable work: ObjCopy deep-copies objString/
 *		    objName/objCmd (allocating fresh storage and
 *		    copying every byte) or bumps a refcount for
 *		    objArray/objDict/objStream, and then the very next
 *		    line's ObjFree immediately undoes exactly that
 *		    (freeing the copy, or decrementing the refcount
 *		    back down) -- a full allocate+copy+free cycle to
 *		    end up with the same net ownership as when it
 *		    started, just to move a value from one Obj to
 *		    another.
 *
 *		    ObjMove instead transfers ownership directly: a
 *		    single memcpy of the whole struct (which already
 *		    carries over every pointer/refcounted-handle/plain
 *		    value correctly, exactly as the first line of
 *		    ObjCopy's own implementation does), then the
 *		    source is invalidated so it no longer thinks it
 *		    owns anything -- no allocation, no deep copy, no
 *		    refcount churn at all.
 * PARAMETERS:	    Obj *dest -- like ObjCopy's own `obj` parameter,
 *		    assumed to not currently hold anything that itself
 *		    needs freeing first (same pre-existing contract
 *		    ObjCopy already has: neither function frees
 *		    whatever dest held going in)
 *		    Obj *src -- the Obj being moved from; ownership of
 *		    whatever it held transfers to *dest. Do not use
 *		    *src again afterward except to ObjFree it (which
 *		    is now a safe no-op, matching ObjFree's own
 *		    behavior on anything already objNone) -- this is a
 *		    real move, not a borrow.
 * RETURNS:	    nothing
 * SIDE EFFECTS:    *dest now holds what *src held; *src is reset to
 *		    objNone (ObjFree-safe, holds nothing)
 ***********************************************************************
 * STRATEGY:	    See project roadmap P1/A1 -- this is deliberately
 *		    just the tool itself for now, not yet used anywhere
 *		    in the project. The plan is to convert existing
 *		    ObjCopy-immediately-followed-by-ObjFree call sites
 *		    to this one at a time, verifying each individually,
 *		    rather than a single project-wide sweep -- P1/A1 is
 *		    rated high risk/large effort in the project's own
 *		    analysis specifically because of how pervasive
 *		    ObjCopy/ObjFree are, so converting call sites
 *		    gradually (starting with the clearest, most
 *		    unambiguous ones) is safer than attempting it all
 *		    at once.
 ***********************************************************************/
void ObjMove(Obj *dest, Obj *src) {
  if (dest == src) {
    /* moving something into itself is a no-op by definition -- and
     * without this check, the memcpy below would be a same-address
     * copy (harmless in practice, if technically an overlapping
     * memcpy) immediately followed by the "invalidate the source"
     * step wiping out the very value just "moved", since dest and
     * src would be the same memory. Guard against this degenerate
     * case explicitly rather than relying on that happening to work
     * out.
     */
    return;
  }
  memcpy(dest, src, sizeof (Obj));
  src->type = objNone;
}

void
ObjCopy(Obj *obj, Obj *obj2)
{
    memcpy(obj, obj2, sizeof(Obj));

    switch (obj->type) {
    case objString:
        GStrInitGS(&obj->u.string, &obj2->u.string);
        if (GMemHadError())
            initError(obj);
        break;
    case objName:
        obj->u.name = copyString(obj2->u.name);
        if (!obj->u.name)
            initError(obj);
        break;
    case objArray:
        if (!ArrayIncRef(obj->u.array))
            initError(obj);
        break;
    case objDict:
        if (!DictIncRef(obj->u.dict))
            initError(obj);
        break;
    case objStream:
        StreamIncRef(obj->u.stream);
        break;
    case objCmd:
        obj->u.cmd = copyString(obj2->u.cmd);
        if (!obj->u.cmd)
            initError(obj);
        break;
    default:
        break;
    }
}

/* fetch an object (dest <- src obj) */

void ObjFetch(Obj *dest, Obj *obj, XRef *xref) {
  (obj->type == objRef && xref) ?
         XRefFetch(xref, obj->u.ref.num, obj->u.ref.gen, dest) : ObjCopy(dest, obj);
}

  // Free object contents.
void ObjFree(Obj *obj) {
  switch (obj->type) {
  case objString:
    GStrFree(&obj->u.string);
    break;
  case objName:
    gfree(obj->u.name);
    break;
  case objArray:
      if (!ArrayDecRef(obj->u.array)) {
	  ArrayFree(obj->u.array);
	  gfree(obj->u.array);
      }
    break;
  case objDict:
      if (!DictDecRef(obj->u.dict)) {
	  DictFree(obj->u.dict);
	  gfree(obj->u.dict);
      }
    break;
  case objStream:
      if (!StreamDecRef(obj->u.stream)) {
	  StreamFree(obj->u.stream);
	  gfree(obj->u.stream);
      }
    break;
  case objCmd:
    gfree(obj->u.cmd);
    break;
  default:
    break;
  }

  obj->type = objNone;
}


  // Initialize an object.
  void initBool(Obj *obj, GBool booln1)
    { initObj(objBool); obj->u.booln = booln1; }
  void initInt(Obj *obj, long intg1)
    { initObj(objInt); obj->u.intg = intg1; }
  void initReal(Obj *obj, gdouble real1)
    { initObj(objReal); obj->u.real = real1; }
  void initString(Obj *obj, GooString *string1)
    { initObj(objString); GStrInitGS(&obj->u.string, string1); if (GMemHadError()) initError(obj); }
  void initName(Obj *obj, char *name1)
    { initObj(objName); obj->u.name = copyString(name1); if (!obj->u.name) initError(obj); }
#ifdef DEBUG_MEM
  void initNull(Obj *obj)
    { initObj(objNull); }
#endif
  void initDictData(Obj *obj, Dict *dict1)
    { initObj(objDict); obj->u.dict = dict1; }
  void initRef(Obj *obj, long num1, long gen1)
    { initObj(objRef); obj->u.ref.num = num1; obj->u.ref.gen = gen1; }
  void initCmd(Obj *obj, char *cmd1)
    { initObj(objCmd); obj->u.cmd = copyString(cmd1); if (!obj->u.cmd) initError(obj); }
  void initError(Obj *obj)
    { initObj(objError); }
  void initEOF(Obj *obj)
    { initObj(objEOF); }

  // Type checking.

  // Special type checking.
  GBool isNameSame(Obj *obj, char *name1)
    { return obj->type == objName && !strcmp(obj->u.name, name1); }
  GBool isCmdSame(Obj *obj, char *cmd1)
    { return obj->type == objCmd && !strcmp(obj->u.cmd, cmd1); }

//  GBool isDict(char *dictType);
//  GBool isStream(char *dictType);



  // Accessors.  NB: these assume object is of correct type.
  gdouble getNum(Obj *obj) { return obj->type == objInt ? 
				 IntToGdouble(obj->u.intg) : obj->u.real; }
  GooString *getString(Obj *obj) { return &obj->u.string; }
  char *getName(Obj *obj) { return obj->u.name; }
  Array *getArray(Obj *obj) { return obj->u.array; }
  Dict *getDict(Obj *obj) { return obj->u.dict; }
  Stream *getStream(Obj *obj) { return obj->u.stream; }
  Ref getRef(Obj *obj) { return obj->u.ref; }


//------------------------------------------------------------------------
// Dict/stream accessors that are not pure forwarders.
//------------------------------------------------------------------------

