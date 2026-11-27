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

#define PDF_MAX_PAGE_COUNT 65535L
#define catalogPageTreeMaxDepth 20

#define catalogPageSearchError    -1
#define catalogPageSearchNotFound  0
#define catalogPageSearchFound     1

/********************************************************************
 *                      CatalogInitNull
 ********************************************************************
 * SYNOPSIS:      Initialize an empty catalog that is not ok.
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
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
void CatalogInitNull(Catalog *cat)
{
    initNull(&cat->pagesRoot);
    cat->numPages = 0;
    initNull(&cat->dests);
    initNull(&cat->nameTree);
    cat->ok = gFalse;
}

/********************************************************************
 *                      CatalogInit
 ********************************************************************
 * SYNOPSIS:      Initialize the catalog from the /Root dictionary.
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
 *                *catDict              Ptr to the /Root dictionary.
 *                vmFile                Not used.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Keep /Pages as an unresolved reference (pages are found when
 *                they are needed), read the page count and the named
 *                destinations (/Dests, /Names /Dests). On error cat->ok is
 *                gFalse.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
void CatalogInit(Catalog *cat, Obj *catDict, VMFileHandle vmFile, XRef *xref)
{
    Obj pagesDict;
    Obj obj;
    long count;

    (void)vmFile;

    cat->ok = gTrue;
    initNull(&cat->pagesRoot);
    cat->numPages = 0;
    initNull(&cat->dests);
    initNull(&cat->nameTree);

    if (!ObjIsDictSame(catDict, "Catalog"))
    {
        EC_WARNING(-1);
        goto err1;
    }

    ObjDictLookupNF(catDict, "Pages", &cat->pagesRoot);
    if (!(isRef(&cat->pagesRoot) || isDict(&cat->pagesRoot)))
    {
        EC_WARNING(-1);
        goto err2;
    }

    ObjFetch(&pagesDict, &cat->pagesRoot, xref);
    if (!ObjIsDictSame(&pagesDict, "Pages"))
    {
        EC_WARNING(-1);
        goto err3;
    }

    ObjDictLookup(&pagesDict, "Count", &obj, xref);
    if (!isInt(&obj))
    {
        EC_WARNING(-1);
        goto err4;
    }
    count = getInt(&obj);
    ObjFree(&obj);
    if (count <= 0 || count > PDF_MAX_PAGE_COUNT)
    {
        EC_WARNING(-1);
        goto err3;
    }
    cat->numPages = (word)count;

    ObjFree(&pagesDict);

    ObjDictLookup(catDict, "Dests", &cat->dests, xref);

    ObjDictLookup(catDict, "Names", &obj, xref);
    if (isDict(&obj))
    {
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

/********************************************************************
 *                      CatalogFree
 ********************************************************************
 * SYNOPSIS:      Free the catalog.
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
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
void CatalogFree(Catalog *cat)
{
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

/********************************************************************
 *                      CatalogInitPageTreeFrame
 ********************************************************************
 * SYNOPSIS:      Start one level of the page tree walk.
 *
 * PARAMETERS:    *frame                Ptr to the frame to fill.
 *                *pagesDict            Ptr to the /Pages node.
 *                *parentAttrs          Ptr to the inherited attributes, NULL at
 *                                      the root.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       GBool                 gFalse if the node has no /Kids array.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static GBool
CatalogInitPageTreeFrame(CatalogPageTreeFrame *frame, Dict *pagesDict,
    PageAttrs *parentAttrs, XRef *xref)
{
    PageAttrsInit(&frame->attrs, parentAttrs, pagesDict, xref);
    DictLookup(pagesDict, "Kids", &frame->kids, xref);
    frame->nextKid = 0;

    if (!isArray(&frame->kids))
    {
        ObjFree(&frame->kids);
        PageAttrsFree(&frame->attrs);
        return gFalse;
    }
    return gTrue;
}

/********************************************************************
 *                      CatalogFreePageTreeFrame
 ********************************************************************
 * SYNOPSIS:      Free one level of the page tree walk.
 *
 * PARAMETERS:    *frame                Ptr to the frame.
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
static void CatalogFreePageTreeFrame(CatalogPageTreeFrame *frame)
{
    ObjFree(&frame->kids);
    PageAttrsFree(&frame->attrs);
}

/********************************************************************
 *                      CatalogFindPageInTree
 ********************************************************************
 * SYNOPSIS:      Find a page by its index in the page tree.
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
 *                *pagesDict            Ptr to the root /Pages node.
 *                *attrs                Ptr to the inherited attributes or NULL.
 *                target                Page index (0-based).
 *                *start                Ptr to the index of the next page found
 *                                      (in/out).
 *                *xref                 Ptr to the cross-reference table.
 *                *page                 Ptr to the page to fill.
 *                depth                 Level of pagesDict (1 = root).
 *                useCounts             gTrue: skip subtrees with /Count.
 *
 * RETURNS:       short                 catalogPageSearchFound,
 *                                      catalogPageSearchNotFound or
 *                                      catalogPageSearchError.
 *
 * STRATEGY:      Walk the tree with a stack of frames in a movable block (no
 *                recursion, the stack is small). /Count is only a hint: if the
 *                search with /Count fails, the caller walks all nodes.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static short
CatalogFindPageInTree(Catalog *cat, Dict *pagesDict, PageAttrs *attrs,
    word target, word *start, XRef *xref, Page *page,
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

    if (depth < 1 || depth > catalogPageTreeMaxDepth)
    {
        EC_WARNING(-1);
        return catalogPageSearchError;
    }

    /* The frames are too large for the stack */
    stackHandle = MemAlloc((word)(catalogPageTreeMaxDepth *
        sizeof(CatalogPageTreeFrame)),
        HF_SWAPABLE, HAF_NO_ERR);
    if (stackHandle == NullHandle)
    {
        GMemSetError();
        return catalogPageSearchError;
    }

    stack = (CatalogPageTreeFrame *)MemLock(stackHandle);
    if (!stack)
    {
        GMemSetError();
        MemFree(stackHandle);
        return catalogPageSearchError;
    }

    level = 0;
    if (!CatalogInitPageTreeFrame(&stack[0], pagesDict, attrs, xref))
    {
        EC_WARNING(-1);
        result = catalogPageSearchError;
        goto done;
    }

    for (;;)
    {
        frame = &stack[level];
        kidCount = ObjArrayGetLength(&frame->kids);

        if (frame->nextKid >= kidCount)
        {
            CatalogFreePageTreeFrame(frame);
            if (level == 0)
            {
                result = catalogPageSearchNotFound;
                goto done;
            }
            --level;
            continue;
        }

        ObjArrayGet(&frame->kids, frame->nextKid++, &kid, xref);

        /* A leaf: count it, or fill the page if it is the target */
        if (ObjIsDictSame(&kid, "Page"))
        {
            if (*start == target)
            {
                PageAttrsInit(&attrs2, &frame->attrs, getDict(&kid), xref);
                PageInit(page, target + 1, getDict(&kid), &attrs2);
                PageAttrsFree(&attrs2);

                if (!PageIsOk(page))
                {
                    PageFree(page);
                    result = catalogPageSearchError;
                }
                else
                {
                    result = catalogPageSearchFound;
                }
                ObjFree(&kid);
                goto cleanupFrames;
            }

            ++(*start);
            ObjFree(&kid);
            continue;
        }

        if (isDict(&kid))
        {
            childCount = -1;
            DictLookup(getDict(&kid), "Count", &countObj, xref);
            if (isInt(&countObj))
            {
                childCount = getInt(&countObj);
            }
            ObjFree(&countObj);

            /* Skip the whole subtree if the target is not in it */
            if (useCounts && childCount >= 0 &&
                *start <= cat->numPages &&
                childCount <= cat->numPages - *start &&
                target >= *start + childCount)
            {
                *start += (word)childCount;
                ObjFree(&kid);
                continue;
            }

            if (level + 1 >= catalogPageTreeMaxDepth)
            {
                ObjFree(&kid);
                EC_WARNING(-1);
                result = catalogPageSearchError;
                goto cleanupFrames;
            }

            if (!CatalogInitPageTreeFrame(&stack[level + 1], getDict(&kid),
                &frame->attrs, xref))
            {
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
    {
        CatalogFreePageTreeFrame(&stack[i]);
    }

    done:
    MemUnlock(stackHandle);
    MemFree(stackHandle);
    return result;
}

/********************************************************************
 *                      CatalogGetPage
 ********************************************************************
 * SYNOPSIS:      Get a page with its inherited attributes.
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
 *                i                     Page number (1-based).
 *                *xref                 Ptr to the cross-reference table.
 *                *page                 Ptr to the page to fill.
 *
 * RETURNS:       GBool                 gFalse if the page is not found.
 *
 * STRATEGY:      Search with /Count first; if that fails, walk the whole tree.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
GBool CatalogGetPage(Catalog *cat, word i, XRef *xref, Page *page)
{
    Obj pagesDict;
    word start = 0;
    short result;

    EC(ECCheckBounds(cat));
    EC(ECCheckBounds(xref));
    EC(ECCheckBounds(page));

    if (!cat->ok || i < 1 || i > cat->numPages)
    {
        return gFalse;
    }

    page->ok = gFalse;
    ObjFetch(&pagesDict, &cat->pagesRoot, xref);
    if (!ObjIsDictSame(&pagesDict, "Pages"))
    {
        ObjFree(&pagesDict);
        return gFalse;
    }

    result = CatalogFindPageInTree(cat, getDict(&pagesDict), NULL,
        i - 1, &start, xref, page, 1, gTrue);

    /* /Count can be wrong: search again without it */
    if (result == catalogPageSearchNotFound)
    {
        start = 0;
        result = CatalogFindPageInTree(cat, getDict(&pagesDict), NULL,
            i - 1, &start, xref, page, 1, gFalse);
    }

    ObjFree(&pagesDict);
    return result == catalogPageSearchFound;
}

/********************************************************************
 *                      CatalogFindPageRefInTree
 ********************************************************************
 * SYNOPSIS:      Find the page number of a page object.
 *
 * PARAMETERS:    *pagesDict            Ptr to the /Pages node.
 *                num                   Object number of the page.
 *                gen                   Generation of the page.
 *                *pageIndex            Ptr to the number of pages counted so
 *                                      far (in/out).
 *                *xref                 Ptr to the cross-reference table.
 *                depth                 Level of pagesDict.
 *
 * RETURNS:       word                  Page number (1-based), 0 if not found.
 *
 * STRATEGY:      Recursive, limited to catalogPageTreeMaxDepth levels.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
static word CatalogFindPageRefInTree(Dict *pagesDict, long num, long gen,
    word *pageIndex, XRef *xref, short depth)
{
    Obj kids, kidNF, kid;
    word i;
    word result;

    if (depth > catalogPageTreeMaxDepth)
    {
        return 0;
    }

    DictLookup(pagesDict, "Kids", &kids, xref);
    if (!isArray(&kids))
    {
        ObjFree(&kids);
        return 0;
    }

    for (i = 0; i < ObjArrayGetLength(&kids); ++i)
    {
        ObjArrayGetNF(&kids, i, &kidNF);
        ObjFetch(&kid, &kidNF, xref);

        if (ObjIsDictSame(&kid, "Page"))
        {
            ++(*pageIndex);
            if (isRef(&kidNF) && getRefNum(&kidNF) == num &&
                getRefGen(&kidNF) == gen)
            {
                ObjFree(&kid);
                ObjFree(&kidNF);
                ObjFree(&kids);
                return *pageIndex;
            }
        }
        else if (isDict(&kid))
        {
            result = CatalogFindPageRefInTree(getDict(&kid), num, gen,
                pageIndex, xref, depth + 1);
            if (result)
            {
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

/********************************************************************
 *                      CatalogFindPage
 ********************************************************************
 * SYNOPSIS:      Return the page number of a page object (for link
 *                destinations).
 *
 * PARAMETERS:    *cat                  Ptr to the catalog.
 *                num                   Object number of the page.
 *                gen                   Generation of the page.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       word                  Page number (1-based), 0 if not found.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
word CatalogFindPage(Catalog *cat, long num, long gen, XRef *xref)
{
    Obj pagesDict;
    word pageIndex = 0;
    word result = 0;

    if (!cat || !xref || !cat->ok)
    {
        return 0;
    }

    ObjFetch(&pagesDict, &cat->pagesRoot, xref);
    if (ObjIsDictSame(&pagesDict, "Pages"))
    {
        result = CatalogFindPageRefInTree(getDict(&pagesDict), num, gen,
            &pageIndex, xref, 1);
    }
    ObjFree(&pagesDict);
    return result;
}

GBool CatalogIsOk(Catalog *cat)
{
    return cat->ok;
}

word CatalogGetNumPages(Catalog *cat)
{
    return cat->numPages;
}

