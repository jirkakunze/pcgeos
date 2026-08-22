/***********************************************************************
 *
 *                      Copyright FreeGEOS-Project
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * PROJECT:       FreeGEOS
 * MODULE:        PDF Viewer
 * FILE:          pdfEngine.c
 *
 * AUTHOR:        Jirka Kunze: 18.08.2026
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      18.08.26  JK        Relicensed under Apache 2.0, cleanup.
 *
 * DESCRIPTION:
 *     PDF engine implementation, formerly part of the UI class PDFDoc.
 ***********************************************************************/

#include "pdfGeode.h"
#include "pdfEngine.h"
#include "catalog.h"
#include "xref.h"
#include "obj.h"
#include "page.h"
#include "gfx.h"
#include "gmem.h"
#include <geode.h>
#include <lmem.h>
#include <hugearr.h>
#include <vm.h>
#include <ec.h>
#include <gstring.h>
#include <graphics.h>

/* Keep page-cache policy private to the engine. */
#define PDF_MAX_CACHED_PAGES    30
#define PDF_PAGE_CACHE_VM_BUDGET 0x00800000UL
#define PDF_PAGE_GSTRING_ESTIMATE 1024UL      /* small per-page baseline */

/*
 * Internal layout behind the opaque handles
 * **********************************************************************
 * Private to this file.
 */

typedef struct {
    word pageNum;
    VMBlockHandle gstringBlock;
    VMBlockHandle resourceList; /* HugeArray of page-owned VM blocks */
    word resourceCount;
    dword estimatedBytes; /* OPT-19: GString + bitmap VM estimate */
    dword lastUse; /* monotonically increasing LRU stamp */
} PdfPageCacheEntry;

typedef struct {
    XRef xref;
    Catalog catalog;

    VMFileHandle gstringFile; /* scratch VM file for decoded pages */
    char gstringFileName[PATH_LENGTH_ZT];

    PdfPageCacheEntry pageCache[PDF_MAX_CACHED_PAGES];
    dword cacheUseClock; /* next LRU timestamp source */
    dword cachedVmBytes;
    word pageCount; /* number of pages in the source PDF */

    Boolean ok; /* mirrors today's PDFDoc_ok */

    PdfProgressCallback progressCallback; /* Phase 4; NULL until then */
    void *progressUserData;
    /*
     * GBool, not Boolean: a pointer to this field gets handed to
     * Gfx.cancelFlag (gfx.goc's own GBool convention), so the types must match
     * for that pointer to be valid
     */
    GBool cancelRequested;
} PdfDocInternal;

/*
 * Page geometry -- shared by PdfGetPageSize and PdfDrawPage
 * ********************************************************************** Both
 * need the same CropBox/MediaBox + /Rotate.
 */
typedef struct {
    word width; /* on-screen(post-rotation) width, points */
    word height; /* on-screen(post-rotation) height, points */
    sword clipX; /* pre-rotation crop/media origin X, points */
    sword clipY; /* pre-rotation crop/media origin Y, points */
    /*
     * pre-rotation (native) width, points -- matches clipX/clipY's own
     * coordinate space.
     */
    word clipWidth;
    word clipHeight; /* same, for height */
    long rotate; /* normalized to 0/90/180/270 */
    /*
     * PageGetHeight(page), unrotated -- only needed for PdfDrawPage's
     * printing- translation case
     */
    gdouble rawHeight;
} PdfPageGeometry;

/***********************************************************************
 *      PdfComputePageGeometry
 ***********************************************************************
 * SYNOPSIS:        Compute page geometry.
 * PARAMETERS:      Catalog *catalog    catalog
 *                  XRef *xref    cross-reference table
 *                  word pageNum    page number
 *                  PdfPageGeometry *geom    page geometry
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static PdfError
PdfComputePageGeometry(Catalog *catalog, XRef *xref, word pageNum,
    PdfPageGeometry *geom)
{
    Page page;

    if (!CatalogGetPage(catalog, pageNum, xref, &page))
    {
        return PDF_ERR_INVALID_PAGE;
    }

    if (PageIsCropped(&page))
    {
        geom->clipX = GdoubleToWord(PageGetCropX1(&page));
        geom->clipY = GdoubleToWord(PageGetCropY1(&page));
        geom->width = GdoubleToWord(PageGetCropX2(&page)) - geom->clipX;
        geom->height = GdoubleToWord(PageGetCropY2(&page)) - geom->clipY;
    }
    else
    {
        geom->clipX = GdoubleToWord(PageGetX1(&page));
        geom->clipY = GdoubleToWord(PageGetY1(&page));
        geom->width = GdoubleToWord(PageGetWidth(&page));
        geom->height = GdoubleToWord(PageGetHeight(&page));
    }

    /*
     * capture native (pre-swap) extent before width/height get swapped for on-
     * screen 90/270 reporting below.
     */
    geom->clipWidth = geom->width;
    geom->clipHeight = geom->height;

    geom->rotate = PageGetRotate(&page);
    if (geom->rotate == 90L || geom->rotate == 270L)
    {
        word temp = geom->width;
        geom->width = geom->height;
        geom->height = temp;
    }
    else if (geom->rotate != 0L && geom->rotate != 180L)
    {
        geom->rotate = 0L;
    }

    geom->rawHeight = PageGetHeight(&page);
    PageFree(&page);
    return PDF_OK;
}

/*
 * Small internal helpers
 * **********************************************************************
 */

/***********************************************************************
 *      PdfLockDoc
 ***********************************************************************
 * SYNOPSIS:        Lock document.
 * PARAMETERS:      PdfDocHandle doc    doc
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static PdfDocInternal *PdfLockDoc(PdfDocHandle doc)
{
    if (doc == PDF_NULL_DOC)
    {
        return NULL;
    }

    EC(ECCheckMemHandle((MemHandle)doc));

    return(PdfDocInternal *)MemLock((MemHandle)doc);
}

/***********************************************************************
 *      PdfUnlockDoc
 ***********************************************************************
 * SYNOPSIS:        Unlock document.
 * PARAMETERS:      PdfDocHandle doc    doc
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void PdfUnlockDoc(PdfDocHandle doc)
{
    if (doc != PDF_NULL_DOC)
    {
        MemUnlock((MemHandle)doc);
    }
}

/***********************************************************************
 *      PdfFreePageCacheEntry
 ***********************************************************************
 * SYNOPSIS:        Release page cache entry.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
 *                  PdfPageCacheEntry *entry    cache entry
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void
PdfFreePageCacheEntry(PdfDocInternal *pdoc, PdfPageCacheEntry *entry)
{
    word i;

    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(entry));
    EC(ECVMCheckVMFile(pdoc->gstringFile));

    /* A recorded GString may reference HugeBitmaps. */
    if (entry->gstringBlock != NullHandle)
    {
        EC(ECVMCheckVMBlockHandle(pdoc->gstringFile, entry->gstringBlock));
        VMFreeVMChain(pdoc->gstringFile,
            VMCHAIN_MAKE_FROM_VM_BLOCK(entry->gstringBlock));
    }

    if (entry->resourceList != NullHandle)
    {
        EC(ECVMCheckVMBlockHandle(pdoc->gstringFile, entry->resourceList));
        for (i = 0; i < entry->resourceCount; ++i)
        {
            VMBlockHandle *resource;
            VMBlockHandle block;
            word elemSize;

            HugeArrayLock(pdoc->gstringFile, entry->resourceList, i,
                (void **) & resource, &elemSize);
            block = *resource;
            HugeArrayUnlock(resource);

            EC_ERROR_IF(block == NullHandle, -1);
            EC(ECVMCheckVMBlockHandle(pdoc->gstringFile, block));
            /*
             * BMT_COMPLEX bitmap data is a HugeArray. Gfx used BMD_LEAVE_DATA
             * after constructing it, transferring lifetime ownership to the
             * page cache.
             */
            HugeArrayDestroy(pdoc->gstringFile, block);
        }
        HugeArrayDestroy(pdoc->gstringFile, entry->resourceList);
    }

    EC_ERROR_IF(entry->estimatedBytes > pdoc->cachedVmBytes, -1);
    pdoc->cachedVmBytes -= entry->estimatedBytes;

    entry->pageNum = 0;
    entry->gstringBlock = NullHandle;
    entry->resourceList = NullHandle;
    entry->resourceCount = 0;
    entry->estimatedBytes = 0;
    entry->lastUse = 0;
}


/***********************************************************************
 *      PdfTouchPageCacheEntry
 ***********************************************************************
 * SYNOPSIS:        Touch page cache entry.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
 *                  PdfPageCacheEntry *entry    cache entry
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void PdfTouchPageCacheEntry(PdfDocInternal *pdoc,
    PdfPageCacheEntry *entry)
{
    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(entry));

    ++pdoc->cacheUseClock;
    if (pdoc->cacheUseClock == 0)
    {
        PdfPageCacheEntry *e = pdoc->pageCache;
        PdfPageCacheEntry *end = e + PDF_MAX_CACHED_PAGES;
        dword stamp = 0;

        for (; e < end; ++e)
        {
            if (e->gstringBlock != NullHandle)
            {
                e->lastUse = ++stamp;
            }
            else
            {
                e->lastUse = 0;
            }
        }
        pdoc->cacheUseClock = stamp + 1;
    }

    entry->lastUse = pdoc->cacheUseClock;
}

/***********************************************************************
 *      PdfFindCachedPage
 ***********************************************************************
 * SYNOPSIS:        Find cached page.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
 *                  word pageNum    page number
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static PdfPageCacheEntry *
    PdfFindCachedPage(PdfDocInternal *pdoc, word pageNum)
{
    word i;

    EC(ECCheckBounds(pdoc));

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
    {
        if (pdoc->pageCache[i].gstringBlock != NullHandle &&
            pdoc->pageCache[i].pageNum == pageNum)
        {
            PdfTouchPageCacheEntry(pdoc, &pdoc->pageCache[i]);
            return & pdoc->pageCache[i];
        }
    }
    return NULL;
}


/***********************************************************************
 *      PdfFindFreePageCacheEntry
 ***********************************************************************
 * SYNOPSIS:        Find free page cache entry.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static PdfPageCacheEntry *
    PdfFindFreePageCacheEntry(PdfDocInternal *pdoc)
{
    word i;

    EC(ECCheckBounds(pdoc));

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
    {
        if (pdoc->pageCache[i].gstringBlock == NullHandle)
        {
            return & pdoc->pageCache[i];
        }
    }
    return NULL;
}


/***********************************************************************
 *      PdfEnforceCacheByteBudget
 ***********************************************************************
 * SYNOPSIS:        Enforce cache byte budget.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
 *                  PdfPageCacheEntry *keepEntry    keep entry
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void
PdfEnforceCacheByteBudget(PdfDocInternal *pdoc, PdfPageCacheEntry *keepEntry)
{
    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(keepEntry));

    while (pdoc->cachedVmBytes > PDF_PAGE_CACHE_VM_BUDGET)
    {
        PdfPageCacheEntry *entry = pdoc->pageCache;
        PdfPageCacheEntry *end = entry + PDF_MAX_CACHED_PAGES;
        PdfPageCacheEntry *oldest = NULL;
        dword oldestUse = 0xffffffffUL;

        for (; entry < end; ++entry)
        {
            if (entry == keepEntry || entry->gstringBlock == NullHandle)
            {
                continue;
            }

            if (entry->lastUse < oldestUse)
            {
                oldestUse = entry->lastUse;
                oldest = entry;
            }
        }

        if (oldest == NULL)
        {
            break;
        }

        PdfFreePageCacheEntry(pdoc, oldest);
    }
}

/* A. */

/***********************************************************************
 *      PdfOpen
 ***********************************************************************
 * SYNOPSIS:        Open.
 * PARAMETERS:      FileHandle file    file
 *                  PdfDocHandle *docOut    doc out
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      XRefInit -> open scratch VM file -> XRefGetCatalog/CatalogInit,
 *      bailing via errorClose: on any failure. An...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
PdfError
PdfOpen(FileHandle file, PdfDocHandle *docOut)
{
    MemHandle mh;
    PdfDocInternal *pdoc;
    Obj catDict;
    char *p;
    PdfError err;

    *docOut = PDF_NULL_DOC;
    GMemClearError();

    if (file == 0)
    {
        return PDF_ERR_NOT_A_PDF;
    }

    mh = MemAllocSetOwner(GeodeGetCodeProcessHandle(),
        sizeof(PdfDocInternal), HF_DYNAMIC | HF_SWAPABLE,
        HAF_ZERO_INIT | HAF_NO_ERR);
    if (mh == NullHandle)
    {
        return PDF_ERR_OUT_OF_MEMORY;
    }
    pdoc = (PdfDocInternal *)MemLock(mh);

    XRefInitNull(&pdoc->xref);
    CatalogInitNull(&pdoc->catalog);
    pdoc->ok = FALSE;
    pdoc->gstringFile = 0;
    pdoc->cacheUseClock = 0;
    pdoc->cachedVmBytes = 0;
    pdoc->pageCount = 0;
    pdoc->progressCallback = NULL;
    pdoc->progressUserData = NULL;
    pdoc->cancelRequested = FALSE;

    /* ----- xref table ----- */
    XRefInit(&pdoc->xref, file);
    if (GMemHadError())
    {
        err = PDF_ERR_OUT_OF_MEMORY;
        goto errorClose;
    }
    if (!XRefIsOk(&pdoc->xref))
    {
        /*
         * xref->trailerDict is still valid at this point s its local stream,
         * it doesn't touch the trailer dict).
         */
        err = XRefCheckEncrypted(&pdoc->xref) ?
            PDF_ERR_ENCRYPTED : PDF_ERR_BAD_XREF;
        goto errorClose;
    }

    /*
     * ----- scratch VM file for decoded page gstrings, temp file in the
     * wastebasket, exactly like the original -----
     */
    p = pdoc->gstringFileName;
    FileConstructFullPath(&p, sizeof(pdoc->gstringFileName),
        SP_WASTE_BASKET, "", TRUE);

    pdoc->gstringFile =
        VMOpen(pdoc->gstringFileName,
        VMAF_FORCE_READ_WRITE | VMAF_USE_BLOCK_LEVEL_SYNCHRONIZATION,
        VMO_TEMP_FILE,
        0);
    if (!pdoc->gstringFile)
    {
        err = PDF_ERR_NO_SCRATCH_FILE;
        goto errorClose;
    }

    /* ----- page catalog ----- */
    XRefGetCatalog(&pdoc->xref, &catDict);
    CatalogInit(&pdoc->catalog, &catDict, pdoc->gstringFile, &pdoc->xref);
    ObjFree(&catDict);

    if (GMemHadError())
    {
        err = PDF_ERR_OUT_OF_MEMORY;
        goto errorClose;
    }
    if (!CatalogIsOk(&pdoc->catalog))
    {
        err = PDF_ERR_BAD_CATALOG;
        goto errorClose;
    }

    /* ----- success: fill in what PdfGetPageCount/PdfDecodePage need ----- */
    pdoc->pageCount = (word)CatalogGetNumPages(&pdoc->catalog);

    pdoc->ok = TRUE;

    MemUnlock(mh);
    *docOut = (PdfDocHandle)mh;
    return PDF_OK;

    errorClose:
    MemUnlock(mh);
    PdfClose((PdfDocHandle)mh);
    return err;
}

/***********************************************************************
 *      PdfClose
 ***********************************************************************
 * SYNOPSIS:        Close.
 * PARAMETERS:      PdfDocHandle doc    doc
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
void PdfClose(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;

    if (doc == PDF_NULL_DOC)
    {
        return;
    }

    pdoc = PdfLockDoc(doc);

    /* Page-cache cleanup replaces the old disabled VMFreeVMChain loop. */

    CatalogFree(&pdoc->catalog);
    XRefFree(&pdoc->xref);

    if (pdoc->gstringFile)
    {
        VMClose(pdoc->gstringFile, FALSE);
        FileDelete(pdoc->gstringFileName);
    }

    PdfUnlockDoc(doc);
    MemFree((MemHandle)doc);
}

/***********************************************************************
 *      PdfIsValid
 ***********************************************************************
 * SYNOPSIS:        Check valid.
 * PARAMETERS:      PdfDocHandle doc    doc
 *
 * RETURNS:         success flag
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
Boolean PdfIsValid(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;
    Boolean valid;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return FALSE;
    }

    valid = pdoc->ok;

    PdfUnlockDoc(doc);
    return valid;
}

/* B. */

/***********************************************************************
 *      PdfGetPageCount
 ***********************************************************************
 * SYNOPSIS:        Get page count.
 * PARAMETERS:      PdfDocHandle doc    doc
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
word PdfGetPageCount(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;
    word count;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return 0;
    }

    count = pdoc->pageCount;

    PdfUnlockDoc(doc);
    return count;
}

/***********************************************************************
 *      PdfGetPageSize
 ***********************************************************************
 * SYNOPSIS:        Get page size.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  word pageNum    page number
 *                  Point *sizeOut    size out
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Delegate to PdfComputePageGeometry(), shared with PdfDrawPage()
 *      since both need the same CropBox/MediaBox + /Rotate...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
PdfError PdfGetPageSize(PdfDocHandle doc, word pageNum, Point *sizeOut)
{
    PdfDocInternal *pdoc;
    PdfPageGeometry geom;
    PdfError err;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok)
    {
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_HANDLE;
    }
    if (pageNum < 1 || pageNum > pdoc->pageCount)
    {
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_PAGE;
    }

    err = PdfComputePageGeometry(&pdoc->catalog, &pdoc->xref, pageNum, &geom);
    if (err == PDF_OK && sizeOut != NULL)
    {
        sizeOut->P_x = geom.width;
        sizeOut->P_y = geom.height;
    }

    PdfUnlockDoc(doc);
    return err;
}

/* C. */

/***********************************************************************
 *      PdfDecodePageContent
 ***********************************************************************
 * SYNOPSIS:        Decode page content.
 * PARAMETERS:      PdfDocInternal *pdoc    PDF document
 *                  word pageNum    page number
 *                  VMBlockHandle *pageDataOut    page data out
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
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static PdfError PdfDecodePageContent(PdfDocInternal *pdoc, word pageNum,
    VMBlockHandle *pageDataOut)
{
    Page page;
    Gfx gfx;
    Dict *resDict;
    Handle gstring;
    Obj pageObj, resDictObj;
    VMBlockHandle pageData;
    PdfPageCacheEntry *cacheEntry;
    GBool status;
    GBool resourceLimitExceeded = gFalse;
    dword resourceBytes = 0;

    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(pageDataOut));

    *pageDataOut = 0;
    GMemClearError();

    /*
     * ----- reserve a cache slot; evict the least-recently-used page if full
     */
    cacheEntry = PdfFindFreePageCacheEntry(pdoc);
    if (cacheEntry == NULL)
    {
        word i;
        dword oldestUse = 0xffffffffUL;

        for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
        {
            if (pdoc->pageCache[i].lastUse < oldestUse)
            {
                oldestUse = pdoc->pageCache[i].lastUse;
                cacheEntry = &pdoc->pageCache[i];
            }
        }
        EC_ERROR_IF(cacheEntry == NULL, -1);
        PdfFreePageCacheEntry(pdoc, cacheEntry);
    }

    /* OPT-10: reserve a VM-backed resource list before decoding. */
    cacheEntry->resourceList = HugeArrayCreate(pdoc->gstringFile,
        sizeof(VMBlockHandle), 0);
    cacheEntry->resourceCount = 0;
    cacheEntry->estimatedBytes = 0;
    if (cacheEntry->resourceList == NullHandle)
    {
        return PDF_ERR_OUT_OF_MEMORY;
    }

    /* ----- lazily materialize this page and decode its content stream ----- */
    if (!CatalogGetPage(&pdoc->catalog, pageNum, &pdoc->xref, &page))
    {
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return GMemHadError() ? PDF_ERR_OUT_OF_MEMORY : PDF_ERR_INVALID_PAGE;
    }
    PageGetContents(&page, &pageObj, &pdoc->xref);
    if (GMemHadError())
    {
        ObjFree(&pageObj);
        PageFree(&page);
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return PDF_ERR_OUT_OF_MEMORY;
    }

    gstring = GrCreateGString(pdoc->gstringFile, GST_VMEM, &pageData);
    if (!gstring)
    {
        ObjFree(&pageObj);
        PageFree(&page);
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return PDF_ERR_OUT_OF_MEMORY;
    }
    GrSetTextMode(gstring, TM_DRAW_BASE, 0);
    GrSetTextColorMap(gstring, CMT_DITHER);
    GrSetLineColorMap(gstring, CMT_DITHER);
    GrSetAreaColorMap(gstring, CMT_DITHER);

    status = gTrue;
    if (!isNull(&pageObj))
    {
        PageCopyResourceDict(&page, &resDictObj, &pdoc->xref);
        if (GMemHadError())
        {
            ObjFree(&resDictObj);
            status = gFalse;
        }
        else
        {
            resDict = getDict(&resDictObj);

            GfxInit(&gfx, gstring, resDict, &pdoc->xref, pdoc->gstringFile);
            if (GMemHadError())
            {
                GfxFree(&gfx);
                ObjFree(&resDictObj);
                status = gFalse;
            }
            else
            {
                gfx.pageResourceList = cacheEntry->resourceList;
                gfx.pageResourceCount = 0;
                gfx.pageResourceBytes = 0;

                /*
                 * Route progress and cancellation through the document
                 * callback.
                 */
                gfx.progressCallback = pdoc->progressCallback;
                gfx.progressUserData = pdoc->progressUserData;
                gfx.cancelFlag = &pdoc->cancelRequested;

                status = GfxDisplay(&gfx, &pageObj);
                if (GMemHadError())
                {
                    status = gFalse;
                }
                resourceLimitExceeded = gfx.resourceLimitExceeded;

                /*
                 * Annotations are never part of the page's own content stream
                 * -- PDF spec 12.5.5.
                 */
                if (status)
                {
                    Obj annotsObj;
                    PageGetAnnots(&page, &annotsObj, &pdoc->xref);
                    GfxDrawAnnotations(&gfx, &annotsObj);
                    ObjFree(&annotsObj);
                    if (GMemHadError())
                    {
                        status = gFalse;
                    }
                    if (gfx.resourceLimitExceeded)
                    {
                        resourceLimitExceeded = gTrue;
                        status = gFalse;
                    }
                }

                cacheEntry->resourceCount = gfx.pageResourceCount;
                resourceBytes = gfx.pageResourceBytes;
                GfxFree(&gfx);
                ObjFree(&resDictObj);
            }
        }
    }

    GrEndGString(gstring);

    if (status)
    {
        GrDestroyGString(gstring, NULL, GSKT_LEAVE_DATA);
    }
    else
    {
        GrDestroyGString(gstring, NULL, GSKT_KILL_DATA);
        pageData = 0;
    }

    ObjFree(&pageObj);
    PageFree(&page);

    if (!status)
    {
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        if (GMemHadError())
        {
            return PDF_ERR_OUT_OF_MEMORY;
        }
        if (resourceLimitExceeded)
        {
            return PDF_ERR_OUT_OF_MEMORY;
        }
        /*
         * So PDF_ERR_CANCELLED below is reachable in principle but not in
         * practice through this path -- kept for correctness, not because it
         * fires often.
         */
        return pdoc->cancelRequested ? PDF_ERR_CANCELLED : PDF_ERR_NOT_A_PDF;
    }

    /* ----- store the new page in the reserved fixed cache slot ----- */
    cacheEntry->pageNum = pageNum;
    cacheEntry->gstringBlock = pageData;
    cacheEntry->estimatedBytes = PDF_PAGE_GSTRING_ESTIMATE + resourceBytes;
    pdoc->cachedVmBytes += cacheEntry->estimatedBytes;
    PdfTouchPageCacheEntry(pdoc, cacheEntry);
    PdfEnforceCacheByteBudget(pdoc, cacheEntry);

    *pageDataOut = pageData;
    return PDF_OK;
}

/***********************************************************************
 *      PdfDecodePage
 ***********************************************************************
 * SYNOPSIS:        Decode page.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  word pageNum    page number
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Validate doc/pageNum, check for a pending cancel request, then
 *      decode via PdfDecodePageContent() only if the page...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
PdfError PdfDecodePage(PdfDocHandle doc, word pageNum)
{
    PdfDocInternal *pdoc;
    PdfPageCacheEntry *cacheEntry;
    VMBlockHandle pageData;
    PdfError err;

    GMemClearError();

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok)
    {
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_HANDLE;
    }
    if (pageNum < 1 || pageNum > pdoc->pageCount)
    {
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_PAGE;
    }
    if (pdoc->cancelRequested)
    {
        pdoc->cancelRequested = FALSE;
        PdfUnlockDoc(doc);
        return PDF_ERR_CANCELLED;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    pageData = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!pageData)
    {
        err = PdfDecodePageContent(pdoc, pageNum, &pageData);
        if (err != PDF_OK)
        {
            PdfUnlockDoc(doc);
            return err;
        }
    }

    PdfUnlockDoc(doc);
    return PDF_OK;
}

/***********************************************************************
 *      PdfDrawPage
 ***********************************************************************
 * SYNOPSIS:        Draw page.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  word pageNum    page number
 *                  GStateHandle target    target
 *                  Boolean printing    printing
 *                  Point *pageSizeOut    page size out
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Re-read the cache entry, compute page geometry, then flip Y and
 *      either rotate+clip+GrDrawGString (screen) or...
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
    Boolean printing, Point *pageSizeOut)
{
    PdfDocInternal *pdoc;
    PdfPageGeometry geom;
    PdfError err;
    PdfPageCacheEntry *cacheEntry;
    VMBlockHandle gstringBlock;
    Handle gstring;
    sword transX, transY;
    word elem;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok || pageNum < 1 || pageNum > pdoc->pageCount)
    {
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_PAGE;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    gstringBlock = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!gstringBlock)
    {
        /* evicted since PdfDecodePage -- caller needs to decode again */
        PdfUnlockDoc(doc);
        return PDF_ERR_INVALID_PAGE;
    }

    err = PdfComputePageGeometry(&pdoc->catalog, &pdoc->xref, pageNum, &geom);
    if (err != PDF_OK)
    {
        PdfUnlockDoc(doc);
        return err;
    }

    if (pageSizeOut != NULL)
    {
        pageSizeOut->P_x = geom.width;
        pageSizeOut->P_y = geom.height;
    }

    gstring = GrLoadGString(pdoc->gstringFile, GST_VMEM, gstringBlock);

    GrSaveState(target);

    /* flip Y: pdf draws with the origin at the lower left */
    GrApplyScale(target, (1L) << 16, (-1L) << 16);

    if (!printing)
    {
        GrApplyRotation(target, MakeWWFixed(geom.rotate));

        switch ((int)geom.rotate)
        {
            case 0:
                transX = 0;
                transY = -(sword)geom.height;
                break;
            case 90:
                transX = 0;
                transY = 0;
                break;
            case 180:
                transX = -(sword)geom.width;
                transY = 0;
                break;
            case 270:
                transX = -(sword)geom.height;
                transY = -(sword)geom.width;
                break;
            default:
                transX = 0;
                transY = 0;
                break;
        }
        GrApplyTranslation(target,
            MakeWWFixed(transX - geom.clipX),
            MakeWWFixed(transY - geom.clipY));

        GrSetClipRect(target, PCT_REPLACE, geom.clipX, geom.clipY,
            geom.clipX + geom.clipWidth, geom.clipY + geom.clipHeight);

        GrDrawGString(target, gstring, 0, 0, 0, &elem);
    }
    else
    {
        GrApplyTranslation(target, 0, -(GdoubleToWWFixed(geom.rawHeight)));

        GrCopyGString(gstring, target, 0);
    }

    GrRestoreState(target);

    GrDestroyGString(gstring, NULL, GSKT_LEAVE_DATA);

    PdfUnlockDoc(doc);

    return PDF_OK;
}

/***********************************************************************
 *      PdfDiscardPage
 ***********************************************************************
 * SYNOPSIS:        Discard page.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  word pageNum    page number
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Find the cache entry via PdfFindCachedPage() and free it, if
 *      present; a no-op if the page isn't currently cached.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void PdfDiscardPage(PdfDocHandle doc, word pageNum)
{
    PdfDocInternal *pdoc;
    PdfPageCacheEntry *cacheEntry;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    if (cacheEntry != NULL)
    {
        PdfFreePageCacheEntry(pdoc, cacheEntry);
    }

    PdfUnlockDoc(doc);
}

/***********************************************************************
 *      PdfDiscardAllPages
 ***********************************************************************
 * SYNOPSIS:        Discard all pages.
 * PARAMETERS:      PdfDocHandle doc    doc
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
void PdfDiscardAllPages(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;
    word i;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return;
    }

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
    {
        if (pdoc->pageCache[i].gstringBlock != NullHandle ||
            pdoc->pageCache[i].resourceList != NullHandle)
        {
            PdfFreePageCacheEntry(pdoc, &pdoc->pageCache[i]);
        }
    }
    PdfUnlockDoc(doc);
}

/* D. */

/***********************************************************************
 *      PdfSetProgressCallback
 ***********************************************************************
 * SYNOPSIS:        Set progress callback.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  PdfProgressCallback callback    callback
 *                  void *userData    callback data
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Store callback and userData directly on the document.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void PdfSetProgressCallback(PdfDocHandle doc, PdfProgressCallback callback,
    void *userData)
{
    PdfDocInternal *pdoc;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return;
    }

    pdoc->progressCallback = callback;
    pdoc->progressUserData = userData;

    PdfUnlockDoc(doc);
}

/***********************************************************************
 *      PdfCancelDecode
 ***********************************************************************
 * SYNOPSIS:        Cancel decode.
 * PARAMETERS:      PdfDocHandle doc    doc
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
void PdfCancelDecode(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return;
    }

    pdoc->cancelRequested = TRUE;

    PdfUnlockDoc(doc);
}

