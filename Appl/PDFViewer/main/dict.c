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


/***********************************************************************
 *    Dict
 ***********************************************************************/

#define DICT_MAX_ENTRIES ((word)(65535L / (long)sizeof(DictEntry)))

/***********************************************************************
 *      DictInit
 ***********************************************************************
 * SYNOPSIS:        initialize an empty dictionary
 * PARAMETERS:      Dict *dict  dictionary to initialize
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called once before a Dict is used, to bring it into a
 *      well-defined, empty state with a single owning reference.
 *
 * STRATEGY:
 *      Clear the entry pointer and size/length counters, and set
 *      the reference count to 1.
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
 * SYNOPSIS:        release all entries and storage owned by a dictionary
 * PARAMETERS:      Dict *dict  dictionary to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when the last reference to a Dict goes away, to
 *      release its keys, values, and backing storage.
 *
 * STRATEGY:
 *      Free each entry's key and value in turn, then free the
 *      entry buffer itself.
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

    for (i = 0; i < dict->length; ++i) {
        gfree(dict->entries[i].key);
        ObjFree(&dict->entries[i].val);
    }
    gfree(dict->entries);
}

/***********************************************************************
 *      DictAdd
 ***********************************************************************
 * SYNOPSIS:        add an entry to a dictionary
 * PARAMETERS:      Dict *dict  dictionary to add to
 *                  char *key   entry key; ownership transfers to dict
 *                  Obj *val    value to copy into the dictionary
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called whenever a new key/value pair needs to be added, e.g.
 *      while parsing a PDF dictionary object.
 *
 * STRATEGY:
 *      Grow the backing storage by 50% (minimum 8, capped at
 *      DICT_MAX_ENTRIES) via grealloc() if the dictionary is full,
 *      then store the key and copy the value into the new entry. On
 *      any failure key is freed and the dictionary is left unchanged.
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

    if (dict->length >= dict->size) {
        if (dict->size >= DICT_MAX_ENTRIES) {
            GMemSetError();
            gfree(key);
            return;
        }

        growBy = dict->size >> 1;
        if (growBy < 8) {
            growBy = 8;
        }

        if (growBy > DICT_MAX_ENTRIES - dict->size) {
            growBy = DICT_MAX_ENTRIES - dict->size;
        }

        newSize = dict->size + growBy;
        newEntries = (DictEntry *)grealloc(dict->entries, (long)newSize * sizeof(DictEntry));

        if (!newEntries) {
            gfree(key);
            return;
        }

        dict->entries = newEntries;
        dict->size = newSize;
    }

    entry = &dict->entries[dict->length];
    entry->key = key;
    ObjCopy(&entry->val, val);

    if (GMemHadError()) {
        gfree(key);
        return;
    }
    ++dict->length;
}

/***********************************************************************
 *      DictFind
 ***********************************************************************
 * SYNOPSIS:        find an entry by key
 * PARAMETERS:      Dict *dict  dictionary to search
 *                  char *key   key to look for
 *
 * RETURNS:         DictEntry *  matching entry, or NULL if not found
 *
 * CONTEXT:
 *      Called by DictIs(), DictLookup(), and DictLookupNF() to
 *      resolve a key to its entry.
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

    for (i = 0; i < dict->length; ++i) {
        if (!strcmp(key, dict->entries[i].key)) {
            return &dict->entries[i];
        }
    }

    return NULL;
}

/***********************************************************************
 *      DictIs
 ***********************************************************************
 * SYNOPSIS:        check whether a dictionary's /Type matches
 * PARAMETERS:      Dict *dict  dictionary to check
 *                  char *type  expected type name
 *
 * RETURNS:         GBool  gTrue if /Type is present and matches
 *
 * CONTEXT:
 *      Called to verify a dictionary is of an expected PDF object
 *      type before trusting its other entries.
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

    return (e = DictFind(dict, "Type")) && isNameSame(&e->val, type);
}

/***********************************************************************
 *      DictLookup
 ***********************************************************************
 * SYNOPSIS:        look up and dereference an entry by key
 * PARAMETERS:      Dict *dict  dictionary to search
 *                  char *key   key to look for
 *                  Obj *obj    receives the fetched value
 *                  XRef *xref  cross-reference table for indirect
 *                              object resolution
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by callers that need the fully resolved value for a
 *      key, following indirect references if present.
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
 * SYNOPSIS:        look up an entry by key without resolving references
 * PARAMETERS:      Dict *dict  dictionary to search
 *                  char *key   key to look for
 *                  Obj *obj    receives a copy of the raw value
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when the caller wants the value as stored, e.g. to
 *      inspect whether it is itself an indirect reference, without
 *      the fetch/resolve overhead of DictLookup().
 *
 * STRATEGY:
 *      Find the entry via DictFind() and copy its value directly,
 *      or initialize obj to null if the key is absent.
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
 * SYNOPSIS:        get the key at a given entry index
 * PARAMETERS:      Dict *dict  dictionary to read from
 *                  word i      zero-based entry index
 *
 * RETURNS:         char *  the entry's key
 *
 * CONTEXT:
 *      Called by callers iterating over all entries in a dictionary.
 *
 * STRATEGY:
 *      Return the stored key pointer directly.
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
 * SYNOPSIS:        fetch and dereference the value at an entry index
 * PARAMETERS:      Dict *dict  dictionary to read from
 *                  word i      zero-based entry index
 *                  Obj *obj    receives the fetched value
 *                  XRef *xref  cross-reference table for indirect
 *                              object resolution
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called by callers iterating over all entries that need the
 *      fully resolved value.
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
 * SYNOPSIS:        fetch the value at an entry index without resolving
 *                  references
 * PARAMETERS:      Dict *dict  dictionary to read from
 *                  word i      zero-based entry index
 *                  Obj *obj    receives a copy of the raw value
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when the caller wants the value as stored, bypassing
 *      indirect reference resolution.
 *
 * STRATEGY:
 *      Copy the stored value directly via ObjCopy().
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
 * SYNOPSIS:        get the number of entries in the dictionary
 * PARAMETERS:      Dict *dict  dictionary to query
 *
 * RETURNS:         word  current entry count
 *
 * CONTEXT:
 *      Called by callers needing to bound iteration over a
 *      dictionary's entries.
 *
 * STRATEGY:
 *      Return the length field directly.
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
