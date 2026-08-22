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
 * FILE:          dict.c
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
 *      Port of Derek Noonburg's "Dict.cc" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include <Ansi/string.h>
#include "obj.h"
#include "xref.h"
#include "dict.h"
#include "gmem.h"

/* 
 * Dict
 */

#define DICT_MAX_ENTRIES ((word)(65535L / (long)sizeof(DictEntry)))

/***********************************************************************
 *      DictInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      Dict *dict    dictionary
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Clear the entry pointer and size/length counters, and set the
 *      reference count to 1.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictInit(Dict *dict)
{
    dict->entries = NULL;
    dict->size = dict->length = 0;
    dict->ref = 1;
}

/***********************************************************************
 *      DictFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      Dict *dict    dictionary
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictFree(Dict *dict)
{
    word i;

    for (i = 0; i < dict->length; ++i)
    {
        gfree(dict->entries[i].key);
        ObjFree(&dict->entries[i].val);
    }
    gfree(dict->entries);
}

/***********************************************************************
 *      DictAdd
 ***********************************************************************
 * SYNOPSIS:        Add.
 * PARAMETERS:      Dict *dict    dictionary
 *                  char *key    key
 *                  Obj *val    val
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Grow the backing storage by 50% (minimum 8, capped at
 *      DICT_MAX_ENTRIES) via grealloc() if the dictionary is full,...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictAdd(Dict *dict, char *key, Obj *val)
{
    word growBy;
    word newSize;
    DictEntry *newEntries;
    DictEntry *entry;

    if (dict->length >= dict->size)
    {
        if (dict->size >= DICT_MAX_ENTRIES)
        {
            GMemSetError();
            gfree(key);
            return;
        }

        growBy = dict->size >> 1;
        if (growBy < 8)
        {
            growBy = 8;
        }

        if (growBy > DICT_MAX_ENTRIES - dict->size)
        {
            growBy = DICT_MAX_ENTRIES - dict->size;
        }

        newSize = dict->size + growBy;
        newEntries = (DictEntry *)grealloc(dict->entries,
            (long)newSize * sizeof(DictEntry));

        if (!newEntries)
        {
            gfree(key);
            return;
        }

        dict->entries = newEntries;
        dict->size = newSize;
    }

    entry = &dict->entries[dict->length];
    entry->key = key;
    ObjCopy(&entry->val, val);

    if (GMemHadError())
    {
        gfree(key);
        return;
    }
    ++dict->length;
}

/***********************************************************************
 *      DictFind
 ***********************************************************************
 * SYNOPSIS:        Find.
 * PARAMETERS:      Dict *dict    dictionary
 *                  char *key    key
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Linear scan comparing each entry's key via strcmp().
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
static DictEntry *DictFind(Dict *dict, char *key)
{
    word i;

    for (i = 0; i < dict->length; ++i)
    {
        if (!strcmp(key, dict->entries[i].key))
        {
            return & dict->entries[i];
        }
    }

    return NULL;
}

/***********************************************************************
 *      DictIs
 ***********************************************************************
 * SYNOPSIS:        Check.
 * PARAMETERS:      Dict *dict    dictionary
 *                  char *type    type
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Look up "Type" via DictFind() and compare it against type.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
GBool DictIs(Dict *dict, char *type)
{
    DictEntry *e;

    return(e = DictFind(dict, "Type")) && isNameSame(&e->val, type);
}

/***********************************************************************
 *      DictLookup
 ***********************************************************************
 * SYNOPSIS:        Look up.
 * PARAMETERS:      Dict *dict    dictionary
 *                  char *key    key
 *                  Obj *obj    object
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Find the entry via DictFind() and ObjFetch() its value, or
 *      initialize obj to null if the key is absent.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictLookup(Dict *dict, char *key, Obj *obj, XRef *xref)
{
    DictEntry *e;

    (e = DictFind(dict, key)) ? ObjFetch(obj, &e->val, xref) : initNull(obj);
}

/***********************************************************************
 *      DictLookupNF
 ***********************************************************************
 * SYNOPSIS:        Look up nf.
 * PARAMETERS:      Dict *dict    dictionary
 *                  char *key    key
 *                  Obj *obj    object
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Find the entry via DictFind() and copy its value directly, or
 *      initialize obj to null if the key is absent.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictLookupNF(Dict *dict, char *key, Obj *obj)
{
    DictEntry *e;

    (e = DictFind(dict, key)) ? ObjCopy(obj, &e->val) : initNull(obj);
}

/***********************************************************************
 *      DictGetKey
 ***********************************************************************
 * SYNOPSIS:        Get key.
 * PARAMETERS:      Dict *dict    dictionary
 *                  word i    index
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
char *DictGetKey(Dict *dict, word i)
{
    return dict->entries[i].key;
}

/***********************************************************************
 *      DictGetVal
 ***********************************************************************
 * SYNOPSIS:        Get val.
 * PARAMETERS:      Dict *dict    dictionary
 *                  word i    index
 *                  Obj *obj    object
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Delegate to ObjFetch() on the stored value.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictGetVal(Dict *dict, word i, Obj *obj, XRef *xref)
{
    ObjFetch(obj, &dict->entries[i].val, xref);
}

/***********************************************************************
 *      DictGetValNF
 ***********************************************************************
 * SYNOPSIS:        Get val nf.
 * PARAMETERS:      Dict *dict    dictionary
 *                  word i    index
 *                  Obj *obj    object
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void DictGetValNF(Dict *dict, word i, Obj *obj)
{
    ObjCopy(obj, &dict->entries[i].val);
}

/***********************************************************************
 *      DictGetLength
 ***********************************************************************
 * SYNOPSIS:        Get length.
 * PARAMETERS:      Dict *dict    dictionary
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
word DictGetLength(Dict *dict)
{
    return dict->length;
}

