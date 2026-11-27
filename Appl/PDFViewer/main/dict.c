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
 *      Port of Derek Noonburg's "Dict" class from xpdf 0.8:
 *      the PDF dictionary (list of key/value pairs).
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include <Ansi/string.h>
#include "obj.h"
#include "xref.h"
#include "dict.h"
#include "gmem.h"

#define DICT_MAX_ENTRIES ((word)(65535L / (long)sizeof(DictEntry)))

/********************************************************************
 *                      DictInit
 ********************************************************************
 * SYNOPSIS:      Initialize an empty dictionary with reference count 1.
 *
 * PARAMETERS:    *dict                 Ptr to the dictionary.
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
void DictInit(Dict *dict)
{
    dict->entries = NULL;
    dict->size = dict->length = 0;
    dict->ref = 1;
}

/********************************************************************
 *                      DictFree
 ********************************************************************
 * SYNOPSIS:      Free all keys, values and the entry storage.
 *
 * PARAMETERS:    *dict                 Ptr to the dictionary.
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

/********************************************************************
 *                      DictAdd
 ********************************************************************
 * SYNOPSIS:      Add a key and a copy of a value to the dictionary.
 *
 * PARAMETERS:    *dict                 Ptr to the dictionary.
 *                *key                  Ptr to the key (gmalloc). The dictionary
 *                                      owns it, also when the add fails.
 *                *val                  Ptr to the value to copy.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Grow the storage by 50 %, at least 8 and at most
 *                DICT_MAX_ENTRIES entries. Out of memory: the key is freed and
 *                GMemHadError() is set.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void DictAdd(Dict *dict, char *key, Obj *val)
{
    word growBy;
    word newSize;
    DictEntry *newEntries;
    DictEntry *entry;

    if (dict->length >= dict->size)
    {
        /* All entries must fit into one 64 KB block */
        if (dict->size >= DICT_MAX_ENTRIES)
        {
            GMemSetError();
            gfree(key);
            return;
        }

        /* Grow by 50 %, but at least by 8 entries */
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
    /* Count the entry only if its value copy is complete */
    ObjCopy(&entry->val, val);

    if (GMemHadError())
    {
        gfree(key);
        return;
    }
    ++dict->length;
}

/********************************************************************
 *                      DictFind
 ********************************************************************
 * SYNOPSIS:      Find the entry of a key.
 *
 * PARAMETERS:    *dict                 Ptr to the dictionary.
 *                *key                  Ptr to the key.
 *
 * RETURNS:       DictEntry *           Ptr to the entry, NULL if the key is not
 *                                      found.
 *
 * STRATEGY:      Linear search; PDF dictionaries are small.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

GBool DictIs(Dict *dict, char *type)
{
    DictEntry *e;

    return(e = DictFind(dict, "Type")) && isNameSame(&e->val, type);
}

void DictLookup(Dict *dict, char *key, Obj *obj, XRef *xref)
{
    DictEntry *e;

    (e = DictFind(dict, key)) ? ObjFetch(obj, &e->val, xref) : initNull(obj);
}

void DictLookupNF(Dict *dict, char *key, Obj *obj)
{
    DictEntry *e;

    (e = DictFind(dict, key)) ? ObjCopy(obj, &e->val) : initNull(obj);
}

char *DictGetKey(Dict *dict, word i)
{
    return dict->entries[i].key;
}

void DictGetValNF(Dict *dict, word i, Obj *obj)
{
    ObjCopy(obj, &dict->entries[i].val);
}

word DictGetLength(Dict *dict)
{
    return dict->length;
}

