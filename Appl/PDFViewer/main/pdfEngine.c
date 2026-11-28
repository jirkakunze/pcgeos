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
#include "dict.h"
#include "array.h"
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
#include <Ansi/string.h>
#include <graphics.h>
#include <localize.h>
#include <timedate.h>
#include "gfxFont.h"
#include "gstr.h"

#define PDF_MAX_CACHED_PAGES    10
#define PDF_PAGE_CACHE_VM_BUDGET 0x00200000UL /* 2 MB of page GStrings */

/*
 * ATTENTION: Mean size of a GString element that is not an image slice
 * (lines, paths, text, attributes). The public API gives only the number
 * of elements, not the bytes. Pages with much text are a little larger
 * than the estimate. Upgrade path: a kernel routine that returns the
 * byte size of a HugeArray without locking its data blocks.
 */
#define PDF_GSTRING_ELEMENT_ESTIMATE 16UL

/* Internal layout behind the opaque handles (private to this file) */

/* Page geometry: shared by PdfGetPageSize and PdfDrawPage */
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
    sword rotate; /* normalized to 0/90/180/270 */
    /*
     * PageGetHeight(page), unrotated -- only needed for PdfDrawPage's
     * printing- translation case
     */
    gdouble rawHeight;
} PdfPageGeometry;

typedef struct {
    word pageNum;
    VMBlockHandle gstringBlock;
    dword estimatedBytes; /* VM bytes of the page GString */
    dword lastUse; /* monotonically increasing LRU stamp */
    /*
     * Geometry of the page, set by PdfDecodePageContent from the page that
     * it loads anyway. Thus a draw of a cached page does not search the
     * page tree again.
     */
    PdfPageGeometry geom;
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

/********************************************************************
 *                      PdfPageGeometryFromPage
 ********************************************************************
 * SYNOPSIS:      Compute the box, size and rotation of a loaded page.
 *
 * PARAMETERS:    *page                 Ptr to the loaded page.
 *                *geom                 Ptr to the geometry (out).
 *
 * RETURNS:       void
 *
 * STRATEGY:      CropBox, else MediaBox; for /Rotate 90 and 270 width and
 *                height are swapped for the screen size.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *      11/28/26  Claude/jkunze  Use the loaded page (geometry cache)
 *******************************************************************/
static void PdfPageGeometryFromPage(Page *page, PdfPageGeometry *geom)
{
    EC(ECCheckBounds(page));
    EC(ECCheckBounds(geom));

    if (PageIsCropped(page))
    {
        geom->clipX = GdoubleToWord(PageGetCropX1(page));
        geom->clipY = GdoubleToWord(PageGetCropY1(page));
        geom->width = GdoubleToWord(PageGetCropX2(page)) - geom->clipX;
        geom->height = GdoubleToWord(PageGetCropY2(page)) - geom->clipY;
    }
    else
    {
        geom->clipX = GdoubleToWord(PageGetX1(page));
        geom->clipY = GdoubleToWord(PageGetY1(page));
        geom->width = GdoubleToWord(PageGetWidth(page));
        geom->height = GdoubleToWord(PageGetHeight(page));
    }

    /* Native extent, before width and height are swapped for 90/270 */
    geom->clipWidth = geom->width;
    geom->clipHeight = geom->height;

    geom->rotate = PageGetRotate(page);
    if (geom->rotate == 90 || geom->rotate == 270)
    {
        word temp = geom->width;
        geom->width = geom->height;
        geom->height = temp;
    }
    else if (geom->rotate != 0 && geom->rotate != 180)
    {
        geom->rotate = 0;
    }

    geom->rawHeight = PageGetHeight(page);
}

/********************************************************************
 *                      PdfApplyScreenRotation
 ********************************************************************
 * SYNOPSIS:      Apply /Rotate and the crop box origin (after the Y flip).
 *
 * PARAMETERS:    target                GState or GString.
 *                *geom                 Ptr to the page geometry.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Shared by PdfDrawPage and the link code, so that a click maps
 *                back exactly as the page was drawn.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void PdfApplyScreenRotation(GStateHandle target, PdfPageGeometry *geom)
{
    sword transX, transY;

    GrApplyRotation(target, MakeWWFixed(geom->rotate));

    switch ((int)geom->rotate)
    {
        case 0:
            transX = 0;
            transY = -(sword)geom->height;
            break;
        case 90:
            transX = 0;
            transY = 0;
            break;
        case 180:
            transX = -(sword)geom->width;
            transY = 0;
            break;
        case 270:
            transX = -(sword)geom->height;
            transY = -(sword)geom->width;
            break;
        default:
            transX = 0;
            transY = 0;
            break;
    }
    GrApplyTranslation(target,
        MakeWWFixed(transX - geom->clipX),
        MakeWWFixed(transY - geom->clipY));
}

/********************************************************************
 *                      PdfFitPrintPage
 ********************************************************************
 * SYNOPSIS:      Fit a page into the printable area.
 *
 * PARAMETERS:    target                Print GState.
 *                *geom                 Ptr to the page geometry.
 *                *area                 Ptr to the printable area.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Scale = min(area / page). Turn the page by 90 degrees if that
 *                gives a larger scale, then center it.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void PdfFitPrintPage(GStateHandle target, PdfPageGeometry *geom,
    const Point *area)
{
    WWFixedAsDWord sx, sy, s, sTurn, w, h;
    word t;

    /* scale as is and turned by 90 degrees */
    sx = GrUDivWWFixed(MakeWWFixed(area->P_x), MakeWWFixed(geom->width));
    sy = GrUDivWWFixed(MakeWWFixed(area->P_y), MakeWWFixed(geom->height));
    s = (sx < sy) ? sx : sy;
    sx = GrUDivWWFixed(MakeWWFixed(area->P_x), MakeWWFixed(geom->height));
    sy = GrUDivWWFixed(MakeWWFixed(area->P_y), MakeWWFixed(geom->width));
    sTurn = (sx < sy) ? sx : sy;
    if (sTurn > s)
    {
        s = sTurn;
        geom->rotate = (geom->rotate + 90) % 360;
        t = geom->width;
        geom->width = geom->height;
        geom->height = t;
    }

    /* center on the area */
    w = GrMulWWFixed(MakeWWFixed(geom->width), s);
    h = GrMulWWFixed(MakeWWFixed(geom->height), s);
    GrApplyTranslation(target,
        (sdword)(MakeWWFixed(area->P_x) - w) / 2,
        (sdword)(MakeWWFixed(area->P_y) - h) / 2);
    GrApplyScale(target, s, s);

    /* same page placement as on screen */
    GrApplyScale(target, (1L) << 16, (-1L) << 16);
    PdfApplyScreenRotation(target, geom);
}

/********************************************************************
 *                      PdfLockDoc
 ********************************************************************
 * SYNOPSIS:      Lock the document block.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       PdfDocInternal *      Ptr to the document, NULL for an invalid
 *                                      handle.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static PdfDocInternal *PdfLockDoc(PdfDocHandle doc)
{
    if (doc == PDF_NULL_DOC)
    {
        return NULL;
    }

    EC(ECCheckMemHandle((MemHandle)doc));

    return(PdfDocInternal *)MemLock((MemHandle)doc);
}

/********************************************************************
 *                      PdfUnlockDoc
 ********************************************************************
 * SYNOPSIS:      Unlock the document block.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void PdfUnlockDoc(PdfDocHandle doc)
{
    if (doc != PDF_NULL_DOC)
    {
        MemUnlock((MemHandle)doc);
    }
}

/********************************************************************
 *                      PdfFreePageCacheEntry
 ********************************************************************
 * SYNOPSIS:      Free a cached page and its GString.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                *entry                Ptr to the cache entry.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The GString holds copies of its images: one VM chain.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void
PdfFreePageCacheEntry(PdfDocInternal *pdoc, PdfPageCacheEntry *entry)
{
    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(entry));
    EC(ECVMCheckVMFile(pdoc->gstringFile));

    /* The GString holds copies of its images (no separate bitmaps). */
    if (entry->gstringBlock != NullHandle)
    {
        EC(ECVMCheckVMBlockHandle(pdoc->gstringFile, entry->gstringBlock));
        VMFreeVMChain(pdoc->gstringFile,
            VMCHAIN_MAKE_FROM_VM_BLOCK(entry->gstringBlock));
    }

    EC_ERROR_IF(entry->estimatedBytes > pdoc->cachedVmBytes, -1);
    pdoc->cachedVmBytes -= entry->estimatedBytes;

    entry->pageNum = 0;
    entry->gstringBlock = NullHandle;
    entry->estimatedBytes = 0;
    entry->lastUse = 0;
}

/********************************************************************
 *                      PdfFreeAllPageCacheEntries
 ********************************************************************
 * SYNOPSIS:      Free all cached pages.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void PdfFreeAllPageCacheEntries(PdfDocInternal *pdoc)
{
    word i;

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
    {
        if (pdoc->pageCache[i].gstringBlock != NullHandle)
        {
            PdfFreePageCacheEntry(pdoc, &pdoc->pageCache[i]);
        }
    }
}

/********************************************************************
 *                      PdfTouchPageCacheEntry
 ********************************************************************
 * SYNOPSIS:      Mark a cache entry as used now (LRU).
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                *entry                Ptr to the cache entry.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      PdfFindCachedPage
 ********************************************************************
 * SYNOPSIS:      Find a page in the page cache.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                pageNum               Page number.
 *
 * RETURNS:       PdfPageCacheEntry *   Ptr to the cache entry, NULL if the page
 *                                      is not cached.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      PdfFindFreePageCacheEntry
 ********************************************************************
 * SYNOPSIS:      Find an unused cache entry.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *
 * RETURNS:       PdfPageCacheEntry *   Ptr to the entry, NULL if the cache is
 *                                      full.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      PdfComputePageGeometry
 ********************************************************************
 * SYNOPSIS:      Get the box, size and rotation of a page.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                pageNum               Page number (1-based).
 *                *geom                 Ptr to the geometry (out).
 *
 * RETURNS:       PdfError              PDF_OK or PDF_ERR_INVALID_PAGE.
 *
 * STRATEGY:      A cached page has its geometry in the cache entry. Only
 *                for a page that is not cached, load the page from the
 *                page tree. The LRU stamp does not change: a size query
 *                is not a use of the page GString.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/28/26  Claude/jkunze  Initial version (geometry cache)
 *******************************************************************/
static PdfError PdfComputePageGeometry(PdfDocInternal *pdoc, word pageNum,
    PdfPageGeometry *geom)
{
    Page page;
    word i;

    EC(ECCheckBounds(pdoc));
    EC(ECCheckBounds(geom));

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i)
    {
        if (pdoc->pageCache[i].gstringBlock != NullHandle &&
            pdoc->pageCache[i].pageNum == pageNum)
        {
            *geom = pdoc->pageCache[i].geom;
            return PDF_OK;
        }
    }

    if (!CatalogGetPage(&pdoc->catalog, pageNum, &pdoc->xref, &page))
    {
        return PDF_ERR_INVALID_PAGE;
    }
    PdfPageGeometryFromPage(&page, geom);
    PageFree(&page);
    return PDF_OK;
}

/********************************************************************
 *                      PdfGStringVmBytes
 ********************************************************************
 * SYNOPSIS:      Estimate the VM bytes of a page GString.
 *
 * PARAMETERS:    file                  Scratch VM file.
 *                gstring               HugeArray of the GString.
 *                imageBytes            Image bytes copied into the GString.
 *
 * RETURNS:       dword                 Estimated bytes.
 *
 * STRATEGY:      HugeArrayGetCount locks only the directory block: swapped-out
 *                data blocks stay on disk. The image bytes are exact; the other
 *                elements count with PDF_GSTRING_ELEMENT_ESTIMATE.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static dword PdfGStringVmBytes(VMFileHandle file, VMBlockHandle gstring,
    dword imageBytes)
{
    EC(ECVMCheckVMFile(file));
    EC(ECVMCheckVMBlockHandle(file, gstring));

    return imageBytes +
        HugeArrayGetCount(file, gstring) * PDF_GSTRING_ELEMENT_ESTIMATE;
}

/********************************************************************
 *                      PdfEnforceCacheByteBudget
 ********************************************************************
 * SYNOPSIS:      Free the oldest pages until the cache fits its byte budget.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                *keepEntry            Ptr to the entry that must stay (the new
 *                                      page).
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      PdfOpen
 ********************************************************************
 * SYNOPSIS:      Open a PDF file.
 *
 * PARAMETERS:    file                  Handle of the open PDF file (the caller
 *                                      owns it).
 *                *password             Ptr to the password, NULL or "" for
 *                                      none.
 *                *docOut               Ptr to the document handle (out).
 *
 * RETURNS:       PdfError              PDF_OK or a PDF_ERR_* value
 *                                      (PDF_ERR_PASSWORD: password missing or
 *                                      wrong, PDF_ERR_ENCRYPTED: not
 *                                      supported).
 *
 * STRATEGY:      Read the cross-reference table, make the scratch VM file for
 *                the page cache and read the catalog. On error everything is
 *                freed again.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
PdfError
PdfOpen(FileHandle file, const char *password, PdfDocHandle *docOut)
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

    XRefInit(&pdoc->xref, file, password);
    if (GMemHadError())
    {
        err = PDF_ERR_OUT_OF_MEMORY;
        goto errorClose;
    }
    if (!XRefIsOk(&pdoc->xref))
    {
        /* The trailer dictionary of xref is still valid here */
        if (pdoc->xref.needPassword)
        {
            err = PDF_ERR_PASSWORD;
        }
        else
        {
            err = XRefCheckEncrypted(&pdoc->xref) ?
                PDF_ERR_ENCRYPTED : PDF_ERR_BAD_XREF;
        }
        goto errorClose;
    }

    /* Scratch VM file for the page GStrings (temporary file) */
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

    /* Success: fill in the document */
    pdoc->pageCount = CatalogGetNumPages(&pdoc->catalog);

    pdoc->ok = TRUE;

    MemUnlock(mh);
    *docOut = (PdfDocHandle)mh;
    return PDF_OK;

    errorClose:
    MemUnlock(mh);
    PdfClose((PdfDocHandle)mh);
    return err;
}

/********************************************************************
 *                      PdfClose
 ********************************************************************
 * SYNOPSIS:      Close a document and free all its data.
 *
 * PARAMETERS:    doc                   Engine document handle.
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
void PdfClose(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;

    if (doc == PDF_NULL_DOC)
    {
        return;
    }

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return;
    }

    /* No more progress reports or cancel requests for this document. */
    pdoc->progressCallback = NULL;
    pdoc->cancelRequested = FALSE;

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

/********************************************************************
 *                      PdfParseDateDigits
 ********************************************************************
 * SYNOPSIS:      Read up to count digits of a PDF date.
 *
 * PARAMETERS:    *p                    Ptr to the read position (in/out).
 *                count                 Number of digits.
 *                def                   Value if there are no digits.
 *
 * RETURNS:       word                  Value.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static word PdfParseDateDigits(const char **p, word count, word def)
{
    word val = 0, n = 0;

    while (n < count && **p >= '0' && **p <= '9')
    {
        val = val * 10 + (word)(**p - '0');
        ++*p;
        ++n;
    }
    return n ? val : def;
}

/********************************************************************
 *                      PdfFormatDate
 ********************************************************************
 * SYNOPSIS:      Format a PDF date "D:YYYYMMDDHHmmSS" with the system date and
 *                time formats.
 *
 * PARAMETERS:    *s                    Ptr to the PDF date string.
 *                *buf                  Ptr to the result (out).
 *                bufSize               Size of buf.
 *
 * RETURNS:       Boolean               FALSE if s is not a date.
 *
 * STRATEGY:      The time zone is ignored.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static Boolean PdfFormatDate(const char *s, char *buf, word bufSize)
{
    TimerDateAndTime dt;
    char tmp[DATE_TIME_BUFFER_SIZE];
    word len;

    if (s[0] == 'D' && s[1] == ':')
    {
        s += 2;
    }
    if (s[0] < '0' || s[0] > '9')
    {
        return FALSE;
    }
    dt.TDAT_year = PdfParseDateDigits(&s, 4, 0);
    dt.TDAT_month = PdfParseDateDigits(&s, 2, 1);
    dt.TDAT_day = PdfParseDateDigits(&s, 2, 1);
    dt.TDAT_hours = PdfParseDateDigits(&s, 2, 0);
    dt.TDAT_minutes = PdfParseDateDigits(&s, 2, 0);
    dt.TDAT_seconds = PdfParseDateDigits(&s, 2, 0);
    dt.TDAT_dayOfWeek = 0;
    if (dt.TDAT_year < 1900 || dt.TDAT_month < 1 || dt.TDAT_month > 12 ||
        dt.TDAT_day < 1 || dt.TDAT_day > 31 || dt.TDAT_hours > 23 ||
        dt.TDAT_minutes > 59)
    {
        return FALSE;
    }

    len = LocalFormatDateTime(tmp, DTF_SHORT, &dt);
    tmp[len++] = ' ';
    len += LocalFormatDateTime(tmp + len, DTF_HM, &dt);
    if (len >= bufSize)
    {
        len = bufSize - 1;
    }
    memcpy(buf, tmp, len);
    buf[len] = 0;
    return TRUE;
}

/********************************************************************
 *                      PdfGetInfoField
 ********************************************************************
 * SYNOPSIS:      Return an entry of the document information dictionary.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                *key                  Ptr to the key (Title, Author, ...).
 *                *buf                  Ptr to the result in the GEOS character
 *                                      set (out).
 *                bufSize               Size of buf.
 *
 * RETURNS:       Boolean               TRUE if the entry is not empty.
 *
 * STRATEGY:      Dates (CreationDate, ModDate) are formatted with the system
 *                formats.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/23/26  JK        Initial version
 *******************************************************************/
Boolean PdfGetInfoField(PdfDocHandle doc, const char *key, char *buf,
                        word bufSize)
{
    PdfDocInternal *pdoc;
    Obj info, val;
    Boolean isDate;

    if (bufSize == 0)
    {
        return FALSE;
    }
    buf[0] = 0;
    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return FALSE;
    }

    XRefGetDocInfo(&pdoc->xref, &info);
    if (isDict(&info))
    {
        ObjDictLookup(&info, (char *)key, &val, &pdoc->xref);
        if (isString(&val))
        {
            isDate = !strcmp(key, "CreationDate") || !strcmp(key, "ModDate");
            if (!isDate || !PdfFormatDate(GStrGetCString(getString(&val)),
                    buf, bufSize))
            {
                PdfTextStringToGeos(getString(&val), buf, bufSize);
            }
        }
        ObjFree(&val);
    }
    ObjFree(&info);

    PdfUnlockDoc(doc);
    return buf[0] != 0;
}

/********************************************************************
 *                      PdfOkToPrint
 ********************************************************************
 * SYNOPSIS:      Check if /P allows printing.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       Boolean               TRUE if printing is allowed.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
Boolean PdfOkToPrint(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;
    Boolean ok;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return FALSE;
    }
    ok = XRefOkToPrint(&pdoc->xref) ? TRUE : FALSE;
    PdfUnlockDoc(doc);
    return ok;
}

/********************************************************************
 *                      PdfOkToCopy
 ********************************************************************
 * SYNOPSIS:      Check if /P allows copying.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       Boolean               TRUE if copying is allowed.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
Boolean PdfOkToCopy(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;
    Boolean ok;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return FALSE;
    }
    ok = XRefOkToCopy(&pdoc->xref) ? TRUE : FALSE;
    PdfUnlockDoc(doc);
    return ok;
}

/********************************************************************
 *                      PdfGetPageCount
 ********************************************************************
 * SYNOPSIS:      Return the number of pages.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       word                  Number of pages, 0 for an invalid
 *                                      handle.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

/********************************************************************
 *                      PdfGetPageSize
 ********************************************************************
 * SYNOPSIS:      Return the screen size of a page.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                pageNum               Page number (1-based).
 *                *sizeOut              Ptr to the size (out).
 *
 * RETURNS:       PdfError              PDF_OK or an error.
 *
 * STRATEGY:      Same geometry as PdfDrawPage (crop box and /Rotate).
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

    err = PdfComputePageGeometry(pdoc, pageNum, &geom);
    if (err == PDF_OK && sizeOut != NULL)
    {
        sizeOut->P_x = geom.width;
        sizeOut->P_y = geom.height;
    }

    PdfUnlockDoc(doc);
    return err;
}

/********************************************************************
 *                      PdfDecodePageContent
 ********************************************************************
 * SYNOPSIS:      Decode a page into a GString of the page cache.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                pageNum               Page number (1-based).
 *                *pageDataOut          Ptr to the GString VM block (out).
 *
 * RETURNS:       PdfError              PDF_OK or an error.
 *
 * STRATEGY:      Take a free cache entry (or the least recently used one), draw
 *                the page content into a new VM GString, then enforce the cache
 *                budget.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
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

    /* Reserve a cache entry; free the oldest page if the cache is full */
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

    cacheEntry->estimatedBytes = 0;

    /* Load the page and decode its content stream */
    if (!CatalogGetPage(&pdoc->catalog, pageNum, &pdoc->xref, &page))
    {
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return GMemHadError() ? PDF_ERR_OUT_OF_MEMORY : PDF_ERR_INVALID_PAGE;
    }
    /* Keep the geometry with the GString: draws do not load the page */
    PdfPageGeometryFromPage(&page, &cacheEntry->geom);
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
            GfxSetPageBox(&gfx, PageGetX1(&page), PageGetY1(&page),
                PageGetX1(&page) + PageGetWidth(&page),
                PageGetY1(&page) + PageGetHeight(&page));
            if (GMemHadError())
            {
                GfxFree(&gfx);
                ObjFree(&resDictObj);
                status = gFalse;
            }
            else
            {
                gfx.pageResourceBytes = 0;

                /* Progress and cancel through the document callback */
                gfx.progressCallback = pdoc->progressCallback;
                gfx.progressUserData = pdoc->progressUserData;
                gfx.cancelFlag = &pdoc->cancelRequested;

                status = GfxDisplay(&gfx, &pageObj);
                if (GMemHadError())
                {
                    status = gFalse;
                }
                resourceLimitExceeded = gfx.resourceLimitExceeded;

                /* PM-TODO: draw annotation appearances (git history before I1) */

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
        return pdoc->cancelRequested ? PDF_ERR_CANCELLED : PDF_ERR_NOT_A_PDF;
    }

    /* Store the new page in the reserved cache entry */
    cacheEntry->pageNum = pageNum;
    cacheEntry->gstringBlock = pageData;
    cacheEntry->estimatedBytes = PdfGStringVmBytes(pdoc->gstringFile,
        pageData, resourceBytes);
    pdoc->cachedVmBytes += cacheEntry->estimatedBytes;
    PdfTouchPageCacheEntry(pdoc, cacheEntry);
    PdfEnforceCacheByteBudget(pdoc, cacheEntry);

    *pageDataOut = pageData;
    return PDF_OK;
}

/********************************************************************
 *                      PdfDecodePage
 ********************************************************************
 * SYNOPSIS:      Make sure that a page is in the page cache.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                pageNum               Page number (1-based).
 *
 * RETURNS:       PdfError              PDF_OK or an error.
 *
 * STRATEGY:      Decode only if the page is not cached. Out of memory: empty
 *                the cache and try once more (the heap is fragmented, not
 *                full).
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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
    /* Drop a cancel request that belongs to an earlier decode */
    pdoc->cancelRequested = FALSE;

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    pageData = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!pageData)
    {
        err = PdfDecodePageContent(pdoc, pageNum, &pageData);

        /* Out of memory is often temporary (fragmented heap): retry once */
        if (err == PDF_ERR_OUT_OF_MEMORY && GMemHadError() &&
            !pdoc->cancelRequested)
        {
            EC_WARNING(-1);
            PdfFreeAllPageCacheEntries(pdoc);
            GMemClearError();
            err = PdfDecodePageContent(pdoc, pageNum, &pageData);
        }
        if (err != PDF_OK)
        {
            PdfUnlockDoc(doc);
            return err;
        }
    }

    PdfUnlockDoc(doc);
    return PDF_OK;
}

/********************************************************************
 *                      PdfDrawPage
 ********************************************************************
 * SYNOPSIS:      Draw a cached page into a GState or GString.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                pageNum               Page number (1-based).
 *                target                GState or GString to draw into.
 *                printing              TRUE for printing and the clipboard
 *                                      (copy the GString).
 *                *fitArea              Ptr to the printable area (print), NULL
 *                                      for 1:1.
 *                *pageSizeOut          Ptr to the page size (out).
 *
 * RETURNS:       PdfError              PDF_OK; PDF_ERR_INVALID_PAGE also if the
 *                                      page left the cache (decode it again).
 *
 * STRATEGY:      Flip Y, then rotate and clip (screen), fit, rotate and clip
 *                (print), or copy 1:1 (clipboard).
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
    Boolean printing, const Point *fitArea, Point *pageSizeOut)
{
    PdfDocInternal *pdoc;
    PdfPageGeometry geom;
    PdfPageCacheEntry *cacheEntry;
    VMBlockHandle gstringBlock;
    Handle gstring;
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

    /* A copy: the print fit changes the geometry */
    geom = cacheEntry->geom;

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
        PdfApplyScreenRotation(target, &geom);

        GrSetClipRect(target, PCT_REPLACE, geom.clipX, geom.clipY,
            geom.clipX + geom.clipWidth, geom.clipY + geom.clipHeight);

        GrDrawGString(target, gstring, 0, 0, 0, &elem);
    }
    else if (fitArea != NULL && geom.width > 0 && geom.height > 0)
    {
        PdfFitPrintPage(target, &geom, fitArea);
        GrSetClipRect(target, PCT_INTERSECTION, geom.clipX, geom.clipY,
            geom.clipX + geom.clipWidth, geom.clipY + geom.clipHeight);
        GrCopyGString(gstring, target, 0);
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

/********************************************************************
 *                      PdfSetProgressCallback
 ********************************************************************
 * SYNOPSIS:      Set the progress callback of a document.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                callback              Callback, NULL for none.
 *                *userData             Ptr passed to the callback.
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

/********************************************************************
 *                      PdfCancelDecode
 ********************************************************************
 * SYNOPSIS:      Ask the running decode to stop.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Only a flag is set (called from the UI thread); GfxGo checks
 *                it after each operator.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/18/26  JK        Initial Revision
 *******************************************************************/
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

#pragma code_seg("PdfLink")

/* Links: a click reads the /Annots of the page once; nothing is kept */

#define PDF_NAME_TREE_MAX_DEPTH 16

/********************************************************************
 *                      PdfScreenState
 ********************************************************************
 * SYNOPSIS:      Make a GState without a window with the screen transform of a
 *                page.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                pageNum               Page number (1-based).
 *
 * RETURNS:       GStateHandle          GState handle, 0 on error.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static GStateHandle PdfScreenState(PdfDocInternal *pdoc, word pageNum)
{
    PdfPageGeometry geom;
    GStateHandle gs;

    if (PdfComputePageGeometry(pdoc, pageNum, &geom) != PDF_OK)
    {
        return 0;
    }
    gs = GrCreateState(0);
    if (gs)
    {
        GrApplyScale(gs, (1L) << 16, (-1L) << 16);
        PdfApplyScreenRotation(gs, &geom);
    }
    return gs;
}

/********************************************************************
 *                      PdfStrCmp
 ********************************************************************
 * SYNOPSIS:      Compare two PDF strings byte by byte (name trees).
 *
 * PARAMETERS:    *a                    Ptr to the first string.
 *                *b                    Ptr to the second string.
 *
 * RETURNS:       sword                 < 0, 0 or > 0.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static sword PdfStrCmp(GooString *a, GooString *b)
{
    long la = GStrGetLength(a), lb = GStrGetLength(b);
    long n = (la < lb) ? la : lb;
    sword r = (n > 0) ? (sword)memcmp(GStrGetCString(a), GStrGetCString(b),
        (word)n) : 0;

    if (r != 0)
    {
        return r;
    }
    return (la < lb) ? -1 : (la > lb) ? 1 : 0;
}

/********************************************************************
 *                      PdfNameTreeLookup
 ********************************************************************
 * SYNOPSIS:      Find a key in a name tree.
 *
 * PARAMETERS:    *tree                 Ptr to the root node.
 *                *key                  Ptr to the key.
 *                *xref                 Ptr to the cross-reference table.
 *                *out                  Ptr to the value (out); null if not
 *                                      found.
 *
 * RETURNS:       void
 *
 * STRATEGY:      One node per level: /Names pairs, else the /Kids entry whose
 *                /Limits hold the key. No recursion, depth limited.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void PdfNameTreeLookup(Obj *tree, GooString *key, XRef *xref, Obj *out)
{
    Obj node, arr, kid, lim, lo, hi, k;
    word i, n, depth;
    GBool next;

    initNull(out);
    ObjCopy(&node, tree);
    for (depth = 0; depth < PDF_NAME_TREE_MAX_DEPTH && isDict(&node); ++depth)
    {
        ObjDictLookup(&node, "Names", &arr, xref);
        if (isArray(&arr))
        {
            n = ObjArrayGetLength(&arr);
            for (i = 0; i + 1 < n; i += 2)
            {
                ObjArrayGet(&arr, i, &k, xref);
                if (isString(&k) && PdfStrCmp(getString(&k), key) == 0)
                {
                    ObjFree(&k);
                    ObjArrayGet(&arr, i + 1, out, xref);
                    break;
                }
                ObjFree(&k);
            }
            ObjFree(&arr);
            break;
        }
        ObjFree(&arr);

        ObjDictLookup(&node, "Kids", &arr, xref);
        next = gFalse;
        if (isArray(&arr))
        {
            n = ObjArrayGetLength(&arr);
            for (i = 0; i < n && !next; ++i)
            {
                ObjArrayGet(&arr, i, &kid, xref);
                ObjDictLookup(&kid, "Limits", &lim, xref);
                if (isArray(&lim) && ObjArrayGetLength(&lim) == 2)
                {
                    ObjArrayGet(&lim, 0, &lo, xref);
                    ObjArrayGet(&lim, 1, &hi, xref);
                    if (isString(&lo) && isString(&hi) &&
                        PdfStrCmp(getString(&lo), key) <= 0 &&
                        PdfStrCmp(key, getString(&hi)) <= 0)
                    {
                        ObjFree(&node);
                        ObjMove(&node, &kid);
                        initNull(&kid);
                        next = gTrue;
                    }
                    ObjFree(&lo);
                    ObjFree(&hi);
                }
                ObjFree(&lim);
                ObjFree(&kid);
            }
        }
        ObjFree(&arr);
        if (!next)
        {
            break;
        }
    }
    ObjFree(&node);
}

#define PDF_DEST_NO_TOP ((sdword)0x7fffffffL)

/********************************************************************
 *                      PdfResolveDest
 ********************************************************************
 * SYNOPSIS:      Resolve a link destination to a page and a top position.
 *
 * PARAMETERS:    *pdoc                 Ptr to the locked document.
 *                *dest                 Ptr to the destination (array, name or
 *                                      string).
 *                *topOut               Ptr to the top in PDF user space (out),
 *                                      PDF_DEST_NO_TOP for the top of the page.
 *
 * RETURNS:       word                  Page number (1-based), 0 if not valid.
 *
 * STRATEGY:      Named destinations through /Dests and the name tree. /XYZ,
 *                /FitH and /FitBH give a top. ATTENTION: the zoom is ignored,
 *                the view keeps its scale. Upgrade: set the view scale.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static word PdfResolveDest(PdfDocInternal *pdoc, Obj *dest, sdword *topOut)
{
    Obj d, v, pg, kind, top;
    word page = 0;
    word idx = 0;

    *topOut = PDF_DEST_NO_TOP;
    initNull(&d);

    /* named destination -> its value */
    if (isName(dest))
    {
        if (isDict(&pdoc->catalog.dests))
        {
            ObjDictLookup(&pdoc->catalog.dests, getName(dest), &d,
                &pdoc->xref);
        }
    }
    else if (isString(dest))
    {
        if (isDict(&pdoc->catalog.nameTree))
        {
            PdfNameTreeLookup(&pdoc->catalog.nameTree, getString(dest),
                &pdoc->xref, &d);
        }
    }
    else
    {
        ObjCopy(&d, dest);
    }
    if (isDict(&d))
    {
        ObjDictLookup(&d, "D", &v, &pdoc->xref);
        ObjFree(&d);
        ObjMove(&d, &v);
    }

    if (isArray(&d) && ObjArrayGetLength(&d) >= 2)
    {
        ObjArrayGetNF(&d, 0, &pg);
        if (isRef(&pg))
        {
            page = CatalogFindPage(&pdoc->catalog, getRefNum(&pg),
                getRefGen(&pg), &pdoc->xref);
        }
        else if (isInt(&pg) && getInt(&pg) >= 0 &&
            getInt(&pg) < (long)pdoc->pageCount)
        {
            page = (word)getInt(&pg) + 1;
        }
        ObjFree(&pg);

        /* index of the top value: [page /XYZ left top], [page /FitH top] */
        ObjArrayGet(&d, 1, &kind, &pdoc->xref);
        if (isNameSame(&kind, "XYZ"))
        {
            idx = 3;
        }
        else if (isNameSame(&kind, "FitH") || isNameSame(&kind, "FitBH"))
        {
            idx = 2;
        }
        ObjFree(&kind);
        if (idx)
        {
            ObjArrayGet(&d, idx, &top, &pdoc->xref);
            if (isNum(&top))
            {
                *topOut = (sword)GdoubleToWord(getNum(&top));
            }
            ObjFree(&top);
        }
    }
    ObjFree(&d);
    if (page > pdoc->pageCount)
    {
        page = 0;
    }
    return page;
}

/********************************************************************
 *                      PdfFindLink
 ********************************************************************
 * SYNOPSIS:      Find the internal link at a point of a page.
 *
 * PARAMETERS:    doc                   Engine document handle.
 *                pageNum               Page number (1-based).
 *                docX                  x in document coordinates.
 *                docY                  y in document coordinates.
 *                *destPage             Ptr to the target page (out).
 *                *destDocY             Ptr to the target y in document
 *                                      coordinates (out), < 0 for the top.
 *
 * RETURNS:       Boolean               TRUE if a link with a valid target was
 *                                      found.
 *
 * STRATEGY:      Map the point back to PDF user space with the page transform,
 *                test the /Rect of each /Link annotation (last one first) and
 *                resolve /Dest or /A /GoTo. URI and GoToR: PM-TODO.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
Boolean PdfFindLink(PdfDocHandle doc, word pageNum, sword docX, sword docY,
    word *destPage, sdword *destDocY)
{
    PdfDocInternal *pdoc;
    GStateHandle gs;
    XYValueAsDWord pt;
    sword px, py;
    Page page;
    Obj annots, annot, obj1, obj2, act;
    word i, j, n;
    gdouble r[4], t;
    sdword top;
    Boolean found = FALSE;

    *destPage = 0;
    *destDocY = -1;
    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL)
    {
        return FALSE;
    }
    if (!pdoc->ok || pageNum < 1 || pageNum > pdoc->pageCount)
    {
        PdfUnlockDoc(doc);
        return FALSE;
    }

    /* click point in PDF user space */
    gs = PdfScreenState(pdoc, pageNum);
    if (!gs)
    {
        PdfUnlockDoc(doc);
        return FALSE;
    }
    pt = GrUntransform(gs, docX, docY);
    GrDestroyState(gs);
    px = DWORD_X(pt);
    py = DWORD_Y(pt);

    if (!CatalogGetPage(&pdoc->catalog, pageNum, &pdoc->xref, &page))
    {
        PdfUnlockDoc(doc);
        return FALSE;
    }
    PageGetAnnots(&page, &annots, &pdoc->xref);
    PageFree(&page);

    n = isArray(&annots) ? ObjArrayGetLength(&annots) : 0;
    /* last annotation is on top: search backwards */
    for (i = n; i > 0 && !found; --i)
    {
        ObjArrayGet(&annots, i - 1, &annot, &pdoc->xref);
        if (!isDict(&annot))
        {
            ObjFree(&annot);
            continue;
        }
        ObjDictLookup(&annot, "Subtype", &obj1, &pdoc->xref);
        if (!isNameSame(&obj1, "Link"))
        {
            ObjFree(&obj1);
            ObjFree(&annot);
            continue;
        }
        ObjFree(&obj1);

        /* /Rect, normalized */
        ObjDictLookup(&annot, "Rect", &obj1, &pdoc->xref);
        if (isArray(&obj1) && ObjArrayGetLength(&obj1) == 4)
        {
            for (j = 0; j < 4; ++j)
            {
                ObjArrayGet(&obj1, j, &obj2, &pdoc->xref);
                r[j] = isNum(&obj2) ? getNum(&obj2) : 0;
                ObjFree(&obj2);
            }
            if (r[0] > r[2])
            {
                t = r[0]; r[0] = r[2]; r[2] = t;
            }
            if (r[1] > r[3])
            {
                t = r[1]; r[1] = r[3]; r[3] = t;
            }
            if (IntToGdouble(px) >= r[0] && IntToGdouble(px) <= r[2] &&
                IntToGdouble(py) >= r[1] && IntToGdouble(py) <= r[3])
            {
                /* target: /Dest, else /A GoTo */
                ObjDictLookup(&annot, "Dest", &obj2, &pdoc->xref);
                if (isNull(&obj2))
                {
                    ObjDictLookup(&annot, "A", &act, &pdoc->xref);
                    if (isDict(&act))
                    {
                        ObjDictLookup(&act, "S", &obj2, &pdoc->xref);
                        if (isNameSame(&obj2, "GoTo"))
                        {
                            ObjFree(&obj2);
                            ObjDictLookup(&act, "D", &obj2, &pdoc->xref);
                        }
                        else
                        {
                            ObjFree(&obj2);
                            initNull(&obj2);
                        }
                    }
                    ObjFree(&act);
                }
                if (!isNull(&obj2))
                {
                    *destPage = PdfResolveDest(pdoc, &obj2, &top);
                    if (*destPage)
                    {
                        found = TRUE;
                        if (top != PDF_DEST_NO_TOP)
                        {
                            gs = PdfScreenState(pdoc, *destPage);
                            if (gs)
                            {
                                pt = GrTransform(gs, 0, (sword)top);
                                GrDestroyState(gs);
                                *destDocY = DWORD_Y(pt);
                                if (*destDocY < 0)
                                {
                                    *destDocY = 0;
                                }
                            }
                        }
                    }
                }
                ObjFree(&obj2);
            }
        }
        ObjFree(&obj1);
        ObjFree(&annot);
    }
    ObjFree(&annots);
    PdfUnlockDoc(doc);
    return found;
}
