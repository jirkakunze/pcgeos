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
 * FILE:          dict.h
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
 *      Port of Derek Noonburg's "Dict" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef DICT_H
#define DICT_H

#ifdef __GNUC__
#pragma interface
#endif

/* Initialize. */
extern void DictInit(Dict *dict);

/* Release all entries and storage owned by a dictionary. */
extern void DictFree(Dict *dict);

/* Increment the reference count, failing if it would overflow. */
#define DictIncRef(dict)		 		\
	(((dict)->ref == (word)0xffff) ? 	\
    (GMemSetError(), (word)0) : (word)++((dict)->ref))

/* Decrement the reference count. */
#define DictDecRef(dict) ((word)--((dict)->ref))

extern word DictGetLength(Dict *dict);

/* Takes ownership of key; does not copy it. */
extern void DictAdd(Dict *dict, char *key, Obj *val);

/* Check whether the dictionary's /Type entry matches the given name. */
extern GBool DictIs(Dict *dict, char *type);

/* Look up. */
extern void DictLookup(Dict *dict, char *key, Obj *obj, XRef *xref);

/* Look up an entry by key without resolving indirect references. */
extern void DictLookupNF(Dict *dict, char *key, Obj *obj);

/* Get key. */
extern char *DictGetKey(Dict *dict, word i);

/* Fetch and dereference the value at a given entry index. */
extern void DictGetVal(Dict *dict, word i, Obj *obj, XRef *xref);

/* Fetch the value at a given entry index without resolving references. */
extern void DictGetValNF(Dict *dict, word i, Obj *obj);

#endif  /* DICT_H */

