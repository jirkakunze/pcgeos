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
 * FILE:          catalog.c
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
 *      Port of Derek Noonburg's "Catalog" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif


#include "pdfGeode.h"
#include "catalog.h"
#include "obj.h"
#include "page.h"
#include "xref.h"
#include "dict.h"
#include "array.h"
#include "gmem.h"
#include <heap.h>
#include <ec.h>

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

#define PDF_MAX_PAGE_COUNT 65535L
#define catalogPageTreeMaxDepth 20

#define catalogPageSearchError    -1
#define catalogPageSearchNotFound  0
#define catalogPageSearchFound     1

/*
 * OPT-12: the catalog no longer materializes one persistent Page object
 * for every page at open time.  We retain only the top-level /Pages
 * object (normally an indirect reference).  A requested page is resolved
 * on demand by walking the /Pages tree and using child /Count values to
 * skip whole subtrees.  Inherited PageAttrs are therefore constructed only
 * along the path to the requested page, and the caller owns/frees the
 * resulting transient Page object.
 */
void CatalogInitNull(Catalog *cat) {

  initNull(&cat->pagesRoot);
  cat->numPages = 0;
  initNull(&cat->dests);
  initNull(&cat->nameTree);
  cat->ok = gFalse;
}

void CatalogInit(Catalog *cat, Obj *catDict, VMFileHandle vmFile, XRef *xref)
{
    Obj pagesDict;
    Obj obj;

    (void)vmFile; /* retained in the API for compatibility */

    cat->ok = gTrue;
    initNull(&cat->pagesRoot);
    cat->numPages = 0;
    initNull(&cat->dests);
    initNull(&cat->nameTree);

    if (!ObjIsDictSame(catDict, "Catalog")) {
        EC_WARNING(-1);
        goto err1;
    }

    /* Keep the non-fetched object.  For normal PDFs this is just a compact
     * indirect reference; a direct dictionary is still supported. */
    ObjDictLookupNF(catDict, "Pages", &cat->pagesRoot);
    if (!(isRef(&cat->pagesRoot) || isDict(&cat->pagesRoot))) {
        EC_WARNING(-1);
        goto err2;
    }

    ObjFetch(&pagesDict, &cat->pagesRoot, xref);
    if (!ObjIsDictSame(&pagesDict, "Pages")) {
        EC_WARNING(-1);
        goto err3;
    }

    ObjDictLookup(&pagesDict, "Count", &obj, xref);
    if (!isInt(&obj)) {
        EC_WARNING(-1);
        goto err4;
    }
    cat->numPages = getInt(&obj);
    ObjFree(&obj);
    if (cat->numPages <= 0 || cat->numPages > PDF_MAX_PAGE_COUNT) {
        EC_WARNING(-1);
        goto err3;
    }

    ObjFree(&pagesDict);

    /* named destinations are still catalog-lifetime objects */
    ObjDictLookup(catDict, "Dests", &cat->dests, xref);

    ObjDictLookup(catDict, "Names", &obj, xref);
    if (isDict(&obj)) {
        ObjDictLookup(&obj, "Dests", &cat->nameTree, xref);
    }
    ObjFree(&obj);

    return;

err4:
    ObjFree(&obj);
err3:
    ObjFree(&pagesDict);
err2:
    ObjFree(&cat->pagesRoot);
    initNull(&cat->pagesRoot);
err1:
    ObjFree(&cat->dests);
    ObjFree(&cat->nameTree);
    initNull(&cat->dests);
    initNull(&cat->nameTree);
    cat->numPages = 0;
    cat->ok = gFalse;
}

void CatalogFree(Catalog *cat) {
  ObjFree(&cat->pagesRoot);
  ObjFree(&cat->dests);
  ObjFree(&cat->nameTree);
  initNull(&cat->pagesRoot);
  initNull(&cat->dests);
  initNull(&cat->nameTree);
  cat->numPages = 0;
  cat->ok = gFalse;
}

typedef struct CatalogPageTreeFrame {
  Obj kids;
  PageAttrs attrs;
  word nextKid;
} CatalogPageTreeFrame;

static GBool
CatalogInitPageTreeFrame(CatalogPageTreeFrame *frame, Dict *pagesDict,
                         PageAttrs *parentAttrs, XRef *xref)
{
  PageAttrsInit(&frame->attrs, parentAttrs, pagesDict, xref);
  DictLookup(pagesDict, "Kids", &frame->kids, xref);
  frame->nextKid = 0;

  if (!isArray(&frame->kids)) {
    ObjFree(&frame->kids);
    PageAttrsFree(&frame->attrs);
    return gFalse;
  }
  return gTrue;
}

static void
CatalogFreePageTreeFrame(CatalogPageTreeFrame *frame)
{
  ObjFree(&frame->kids);
  PageAttrsFree(&frame->attrs);
}

/*
 * Search a /Pages node for target (zero-based page index).  `start` is the
 * zero-based page index represented by the next leaf encountered.  /Count
 * is used only as a skip hint; if it is missing or inconsistent we descend
 * instead of trusting it, preserving the old parser's tolerance of damaged
 * page trees.
 *
 * Unlike the OPT-12 version this routine is iterative: no page-tree level
 * consumes C stack space.
 */
static short
CatalogFindPageInTree(Catalog *cat, Dict *pagesDict, PageAttrs *attrs,
                      long target, long *start, XRef *xref, Page *page,
                      short depth, GBool useCounts)
{
  MemHandle stackHandle;
  CatalogPageTreeFrame *stack;
  CatalogPageTreeFrame *frame;
  Obj kid;
  Obj countObj;
  PageAttrs attrs2;
  long childCount;
  word kidCount;
  short level;
  short result = catalogPageSearchNotFound;
  short i;

  if (depth < 1 || depth > catalogPageTreeMaxDepth) {
    EC_WARNING(-1);
    return catalogPageSearchError;
  }

  stackHandle = MemAlloc((word)(catalogPageTreeMaxDepth *
                         sizeof(CatalogPageTreeFrame)),
                         HF_SWAPABLE, HAF_NO_ERR);
  if (stackHandle == NullHandle) {
    GMemSetError();
    return catalogPageSearchError;
  }

  stack = (CatalogPageTreeFrame *)MemLock(stackHandle);
  if (!stack) {
    GMemSetError();
    MemFree(stackHandle);
    return catalogPageSearchError;
  }

  level = 0;
  if (!CatalogInitPageTreeFrame(&stack[0], pagesDict, attrs, xref)) {
    EC_WARNING(-1);
    result = catalogPageSearchError;
    goto done;
  }

  for (;;) {
    frame = &stack[level];
    kidCount = ObjArrayGetLength(&frame->kids);

    if (frame->nextKid >= kidCount) {
      CatalogFreePageTreeFrame(frame);
      if (level == 0) {
        result = catalogPageSearchNotFound;
        goto done;
      }
      --level;
      continue;
    }

    ObjArrayGet(&frame->kids, frame->nextKid++, &kid, xref);

    if (ObjIsDictSame(&kid, "Page")) {
      if (*start == target) {
        PageAttrsInit(&attrs2, &frame->attrs, getDict(&kid), xref);
        PageInit(page, target + 1, getDict(&kid), &attrs2);
        PageAttrsFree(&attrs2);

        if (!PageIsOk(page)) {
          PageFree(page);
          result = catalogPageSearchError;
        } else {
          result = catalogPageSearchFound;
        }
        ObjFree(&kid);
        goto cleanupFrames;
      }

      ++(*start);
      ObjFree(&kid);
      continue;
    }

    /* Some real files omit /Type on intermediate /Pages dictionaries;
     * preserve the old implementation's tolerant `isDict` fallback. */
    if (isDict(&kid)) {
      childCount = -1;
      DictLookup(getDict(&kid), "Count", &countObj, xref);
      if (isInt(&countObj))
        childCount = getInt(&countObj);
      ObjFree(&countObj);

      if (useCounts && childCount >= 0 &&
          *start >= 0 && *start <= cat->numPages &&
          childCount <= cat->numPages - *start &&
          target >= *start + childCount) {
        *start += childCount;
        ObjFree(&kid);
        continue;
      }

      if (level + 1 >= catalogPageTreeMaxDepth) {
        ObjFree(&kid);
        EC_WARNING(-1);
        result = catalogPageSearchError;
        goto cleanupFrames;
      }

      if (!CatalogInitPageTreeFrame(&stack[level + 1], getDict(&kid),
                                    &frame->attrs, xref)) {
        ObjFree(&kid);
        EC_WARNING(-1);
        result = catalogPageSearchError;
        goto cleanupFrames;
      }

      ObjFree(&kid);
      ++level;
      continue;
    }

    ObjFree(&kid);
    EC_WARNING(-1);
    result = catalogPageSearchError;
    goto cleanupFrames;
  }

 cleanupFrames:
  for (i = level; i >= 0; --i)
    CatalogFreePageTreeFrame(&stack[i]);

 done:
  MemUnlock(stackHandle);
  MemFree(stackHandle);
  return result;
}

/*
 * Materialize one Page into caller-owned storage.  The caller must call
 * PageFree() exactly once when this returns gTrue.
 */
GBool CatalogGetPage(Catalog *cat, long i, XRef *xref, Page *page) {
  Obj pagesDict;
  long start = 0;
  short result;

  EC( ECCheckBounds(cat) );
  EC( ECCheckBounds(xref) );
  EC( ECCheckBounds(page) );

  if (!cat->ok || i < 1 || i > cat->numPages) {
    return gFalse;
  }

  page->ok = gFalse;
  ObjFetch(&pagesDict, &cat->pagesRoot, xref);
  if (!ObjIsDictSame(&pagesDict, "Pages")) {
    ObjFree(&pagesDict);
    return gFalse;
  }

  result = CatalogFindPageInTree(cat, getDict(&pagesDict), NULL,
                                 i - 1, &start, xref, page, 1, gTrue);

  /* A damaged PDF can contain an incorrect child /Count.  The fast path
   * above follows the spec and uses Count to skip subtrees; if that fails
   * to locate the requested page, retry once without Count-based skipping
   * to retain the old viewer's tolerance of under-reported counts. */
  if (result == catalogPageSearchNotFound) {
    start = 0;
    result = CatalogFindPageInTree(cat, getDict(&pagesDict), NULL,
                                   i - 1, &start, xref, page, 1, gFalse);
  }

  ObjFree(&pagesDict);
  return result == catalogPageSearchFound;
}

#ifdef KEEP_PAGE_REFS
/*
 * KEEP_PAGE_REFS is not enabled by the current viewer build.  Preserve its
 * public helper without reintroducing a permanent per-page table: scan the
 * tree lazily and compare leaf references on demand.
 */
static long
CatalogFindPageRefInTree(Dict *pagesDict, long num, long gen,
                         long *pageIndex, XRef *xref, short depth)
{
  Obj kids, kidNF, kid;
  word i;
  long result;

  if (depth > catalogPageTreeMaxDepth)
    return 0;

  DictLookup(pagesDict, "Kids", &kids, xref);
  if (!isArray(&kids)) {
    ObjFree(&kids);
    return 0;
  }

  for (i = 0; i < ObjArrayGetLength(&kids); ++i) {
    ObjArrayGetNF(&kids, i, &kidNF);
    ObjFetch(&kid, &kidNF, xref);

    if (ObjIsDictSame(&kid, "Page")) {
      ++(*pageIndex);
      if (isRef(&kidNF) && getRefNum(&kidNF) == num &&
          getRefGen(&kidNF) == gen) {
        ObjFree(&kid);
        ObjFree(&kidNF);
        ObjFree(&kids);
        return *pageIndex;
      }
    } else if (isDict(&kid)) {
      result = CatalogFindPageRefInTree(getDict(&kid), num, gen,
                                        pageIndex, xref, depth + 1);
      if (result) {
        ObjFree(&kid);
        ObjFree(&kidNF);
        ObjFree(&kids);
        return result;
      }
    }

    ObjFree(&kid);
    ObjFree(&kidNF);
  }

  ObjFree(&kids);
  return 0;
}

long CatalogFindPage(Catalog *cat, long num, long gen, XRef *xref) {
  Obj pagesDict;
  long pageIndex = 0;
  long result = 0;

  if (!cat || !xref || !cat->ok)
    return 0;

  ObjFetch(&pagesDict, &cat->pagesRoot, xref);
  if (ObjIsDictSame(&pagesDict, "Pages")) {
    result = CatalogFindPageRefInTree(getDict(&pagesDict), num, gen,
                                      &pageIndex, xref, 1);
  }
  ObjFree(&pagesDict);
  return result;
}
#endif

// Is catalog valid?
GBool CatalogIsOk(Catalog *cat) { return cat->ok; }

// Get number of pages.
long CatalogGetNumPages(Catalog *cat) { return cat->numPages; }
