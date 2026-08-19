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

//------------------------------------------------------------------------
// Dict
//------------------------------------------------------------------------

void DictInit(Dict *dict)
{
  dict->entries = NULL;
  dict->size = dict->length = 0;
  dict->ref = 1;
}

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

#define DICT_MAX_ENTRIES ((word)(65535L / (long)sizeof(DictEntry)))

void
DictAdd(Dict *dict, char *key, Obj *val)
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
            growBy = 8;

        if (growBy > DICT_MAX_ENTRIES - dict->size)
            growBy = DICT_MAX_ENTRIES - dict->size;

        newSize = dict->size + growBy;
        newEntries = (DictEntry *)grealloc(dict->entries, (long)newSize * sizeof(DictEntry));

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

static DictEntry *DictFind(Dict *dict, char *key) 
{
  word i;

  for (i = 0; i < dict->length; ++i)
    if (!strcmp(key, dict->entries[i].key))
      return &dict->entries[i];

  return NULL;
}

GBool DictIs(Dict *dict, char *type)
{
  DictEntry *e;

  return (e = DictFind(dict, "Type")) && isNameSame(&e->val, type);
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

void DictGetVal(Dict *dict, word i, Obj *obj, XRef *xref)
{
  ObjFetch(obj, &dict->entries[i].val, xref);
}

void DictGetValNF(Dict *dict, word i, Obj *obj)
{
  ObjCopy(obj, &dict->entries[i].val);
}

// Get number of entries.
word DictGetLength(Dict *dict)
{ 
  return dict->length;
}
