/***********************************************************************
 *
 * dict.h
 *
 * Copyright 1996 Derek B. Noonburg
 * Modifications Copyright 2026 Jirka Kunze/FreeGEOS Project
 *
 * This file is derived from the original Xpdf source code and has been
 * modified for use in the PC/GEOS PDF Viewer.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***********************************************************************/

#ifndef _DICT_H
#define _DICT_H

#ifdef __GNUC__
#pragma interface
#endif


  // Constructor.
extern void
  DictInit(Dict *dict);

  // Destructor.
extern void
  DictFree(Dict *dict);

#define DictIncRef(dict) \
  (((dict)->ref == (word)0xffff) ? \
   (GMemSetError(), (word)0) : (word)++((dict)->ref))
#define DictDecRef(dict) ((word)--((dict)->ref))

  // Get number of entries.
extern word
  DictGetLength(Dict *dict);

  // Add an entry.  NB: does not copy key.
extern void
  DictAdd(Dict *dict, char *key, Obj *val);

  // Check if dictionary is of specified type.
extern GBool
  DictIs(Dict *dict, char *type);

  // Look up an entry and return the value.  Returns a null object
  // if <key> is not in the dictionary.
extern void
  DictLookup(Dict *dict, char *key, Obj *obj, XRef *xref);

extern void
  DictLookupNF(Dict *dict, char *key, Obj *obj);

  // Iterative accessors.
extern char
  *DictGetKey(Dict *dict, word i);

extern void
  DictGetVal(Dict *dict, word i, Obj *obj, XRef *xref);

extern void
  DictGetValNF(Dict *dict, word i, Obj *obj);


#endif  /* _DICT_H */
