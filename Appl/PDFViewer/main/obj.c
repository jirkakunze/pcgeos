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
 *      Port of Derek Noonburg's "Object" class from xpdf 0.8:
 *      the PDF object (null, bool, number, string, name, array,
 *      dictionary, stream, reference).
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

#ifdef DEBUG_MEM
long Object::numAlloc[numObjTypes] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif

#ifdef DEBUG_MEM
#define initObj(t) ++numAlloc[obj->type = t]
#else
#define initObj(t) obj->type = t
#endif

/********************************************************************
 *                      initArray
 ********************************************************************
 * SYNOPSIS:      Make an empty array object.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void initArray(Obj *obj)
{
    initObj(objArray);
    obj->u.array = gmalloc(sizeof (Array));
    if (!obj->u.array)
    {
        initError(obj);
        return;
    }
    ArrayInit(obj->u.array);
}

/********************************************************************
 *                      initDict
 ********************************************************************
 * SYNOPSIS:      Make an empty dictionary object.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void initDict(Obj *obj)
{
    initObj(objDict);
    obj->u.dict = gmalloc(sizeof (Dict));
    if (!obj->u.dict)
    {
        initError(obj);
        return;
    }
    DictInit(obj->u.dict);
}

/********************************************************************
 *                      initStream
 ********************************************************************
 * SYNOPSIS:      Make a stream object.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                *stream1              Ptr to the stream. The object owns it.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void initStream(Obj *obj, Stream *stream1)
{
    initObj(objStream);
    obj->u.stream = stream1;
}

/********************************************************************
 *                      ObjMove
 ********************************************************************
 * SYNOPSIS:      Move an object; the source becomes objNone.
 *
 * PARAMETERS:    *dest                 Ptr to the destination.
 *                *src                  Ptr to the source.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void ObjMove(Obj *dest, Obj *src)
{
    if (dest == src)
    {
        /* Self-move must be a no-op to avoid invalidating the object. */
        return;
    }
    memcpy(dest, src, sizeof (Obj));
    src->type = objNone;
}

/********************************************************************
 *                      ObjCopy
 ********************************************************************
 * SYNOPSIS:      Copy an object.
 *
 * PARAMETERS:    *obj                  Ptr to the copy.
 *                *obj2                 Ptr to the object to copy.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Strings, names and commands are duplicated. Arrays,
 *                dictionaries and streams are shared through their reference
 *                count. Out of memory: the copy is an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void ObjCopy(Obj *obj, Obj *obj2)
{
    memcpy(obj, obj2, sizeof(Obj));

    switch (obj->type)
    {
        case objString:
            GStrInitGS(&obj->u.string, &obj2->u.string);
            if (GMemHadError())
            {
                initError(obj);
            }
            break;
        case objName:
            obj->u.name = copyString(obj2->u.name);
            if (!obj->u.name)
            {
                initError(obj);
            }
            break;
        case objArray:
            if (!ArrayIncRef(obj->u.array))
            {
                initError(obj);
            }
            break;
        case objDict:
            if (!DictIncRef(obj->u.dict))
            {
                initError(obj);
            }
            break;
        case objStream:
            StreamIncRef(obj->u.stream);
            break;
        case objCmd:
            obj->u.cmd = copyString(obj2->u.cmd);
            if (!obj->u.cmd)
            {
                initError(obj);
            }
            break;
        default:
            break;
    }
}

void ObjFetch(Obj *dest, Obj *obj, XRef *xref)
{
    (obj->type == objRef && xref) ?
        XRefFetch(xref, obj->u.ref.num, obj->u.ref.gen, dest) : ObjCopy(dest,
        obj);
}

/********************************************************************
 *                      ObjFree
 ********************************************************************
 * SYNOPSIS:      Free the data of an object; it becomes objNone.
 *
 * PARAMETERS:    *obj                  Ptr to the object.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Arrays, dictionaries and streams are freed with their last
 *                reference.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void ObjFree(Obj *obj)
{
    switch (obj->type)
    {
        case objString:
            GStrFree(&obj->u.string);
            break;
        case objName:
            gfree(obj->u.name);
            break;
        case objArray:
            if (!ArrayDecRef(obj->u.array))
            {
                ArrayFree(obj->u.array);
                gfree(obj->u.array);
            }
            break;
        case objDict:
            if (!DictDecRef(obj->u.dict))
            {
                DictFree(obj->u.dict);
                gfree(obj->u.dict);
            }
            break;
        case objStream:
            if (!StreamDecRef(obj->u.stream))
            {
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

void initBool(Obj *obj, GBool booln1)
{ 
    initObj(objBool); obj->u.booln = booln1; 
}

void initInt(Obj *obj, long intg1)
{
    initObj(objInt); obj->u.intg = intg1;
}

void initReal(Obj *obj, gdouble real1)
{
    initObj(objReal); obj->u.real = real1;
}

/********************************************************************
 *                      initString
 ********************************************************************
 * SYNOPSIS:      Make a string object with a copy of a string.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                *string1              Ptr to the string to copy.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void initString(Obj *obj, GooString *string1)
{
    initObj(objString);
    GStrInitGS(&obj->u.string, string1);
    if (GMemHadError())
    {
        initError(obj);
    }
}

/********************************************************************
 *                      initName
 ********************************************************************
 * SYNOPSIS:      Make a name object with a copy of a name.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                *name1                Ptr to the name.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void initName(Obj *obj, char *name1)
{
    initObj(objName);
    obj->u.name = copyString(name1);
    if (!obj->u.name)
    {
        initError(obj);
    }
}

#ifdef DEBUG_MEM
void initNull(Obj *obj)
{ 
    initObj(objNull);
}
#endif

/********************************************************************
 *                      initDictData
 ********************************************************************
 * SYNOPSIS:      Make a dictionary object for an existing dictionary.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                *dict1                Ptr to the dictionary. The object owns
 *                                      it.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void initDictData(Obj *obj, Dict *dict1)
{
    initObj(objDict); 
    obj->u.dict = dict1;
}

/********************************************************************
 *                      initRef
 ********************************************************************
 * SYNOPSIS:      Make an indirect reference object.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                num1                  Object number.
 *                gen1                  Generation number.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void initRef(Obj *obj, long num1, long gen1)
{
    initObj(objRef);
    obj->u.ref.num = num1;
    obj->u.ref.gen = gen1;
}

/********************************************************************
 *                      initCmd
 ********************************************************************
 * SYNOPSIS:      Make a command (operator) object with a copy of a name.
 *
 * PARAMETERS:    *obj                  Ptr to the object to fill.
 *                *cmd1                 Ptr to the command name.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Out of memory: an error object.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void initCmd(Obj *obj, char *cmd1)
{
    initObj(objCmd);
    obj->u.cmd = copyString(cmd1);
    if (!obj->u.cmd)
    {
        initError(obj);
    }
}

void initError(Obj *obj)
{
    initObj(objError);
}

void initEOF(Obj *obj)
{
    initObj(objEOF);
}

GBool isNameSame(Obj *obj, char *name1)
{ 
    return obj->type == objName && !strcmp(obj->u.name, name1); 
}
    
GBool isCmdSame(Obj *obj, char *cmd1)
{
    return obj->type == objCmd && !strcmp(obj->u.cmd, cmd1);
}

gdouble getNum(Obj *obj)
{ 
    return obj->type == objInt ? IntToGdouble(obj->u.intg) : obj->u.real;
}

GooString *getString(Obj *obj)
{
    return & obj->u.string;
}

char *getName(Obj *obj)
{
    return obj->u.name;
}

Array *getArray(Obj *obj)
{
    return obj->u.array;
}

Dict *getDict(Obj *obj)
{
    return obj->u.dict;
}

Stream *getStream(Obj *obj)
{
    return obj->u.stream;
}

Ref getRef(Obj *obj)
{
    return obj->u.ref;
}

