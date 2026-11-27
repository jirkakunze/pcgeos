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

/* Keep page-cache policy private to the engine. */
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

/*
 * Internal layout behind the opaque handles
 * **********************************************************************
 * Private to this file.
 */

typedef struct {
    word pageNum;
    VMBlockHandle gstringBlock;
    dword estimatedBytes; /* VM bytes of the page GString */
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
    sword rotate; /* normalized to 0/90/180/270 */
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

    geom->rawHeight = PageGetHeight(&page);
    PageFree(&page);
    return PDF_OK;
}

/***********************************************************************
 *      PdfApplyScreenRotation
 ***********************************************************************
 * SYNOPSIS:        Rotate and move PDF user space onto the screen page
 *                  (after the Y flip): /Rotate and the crop box origin.
 * PARAMETERS:      target    GState with the Y flip already applied
 *                  geom      page geometry
 * STRATEGY:        Shared by PdfDrawPage and the link code, so a click
 *                  maps back exactly the way the page was drawn.
 ***********************************************************************/
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

/***********************************************************************
 *      PdfFitPrintPage
 ***********************************************************************
 * SYNOPSIS:        Fit a page into the printable area (G2).
 * PARAMETERS:      target    print GState, origin at the top left of
 *                            the printable area
 *                  geom      page geometry (changed: rotation and the
 *                            on-paper width/height)
 *                  area      printable width and height, points
 * STRATEGY:        Scale = min(area / page) keeps the aspect ratio. If
 *                  the page turned by another 90 degrees gets a larger
 *                  scale (landscape page on portrait paper), turn it.
 *                  Then center, scale, and apply the same Y flip and
 *                  rotation as the screen (PdfApplyScreenRotation).
 *                  Small pages are enlarged to fill the area.
 ***********************************************************************/
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


/***********************************************************************
 *      PdfFreeAllPageCacheEntries
 ***********************************************************************
 * SYNOPSIS:        Drop every decoded page of the document.
 ***********************************************************************/
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
 *      PdfGStringVmBytes
 ***********************************************************************
 * SYNOPSIS:        Estimate the VM bytes of a page GString.
 * PARAMETERS:      VMFileHandle file       scratch VM file
 *                  VMBlockHandle gstring   HugeArray of the GString
 *                  dword imageBytes        image bytes copied into it
 * RETURN:          dword                   estimated VM bytes
 * STRATEGY:        A VM GString is a HugeArray. HugeArrayGetCount locks
 *                  only the directory block, so that data blocks that
 *                  are swapped out stay on disk. The image bytes are
 *                  known exactly; all other elements are small.
 ***********************************************************************/
static dword PdfGStringVmBytes(VMFileHandle file, VMBlockHandle gstring,
    dword imageBytes)
{
    EC(ECVMCheckVMFile(file));
    EC(ECVMCheckVMBlockHandle(file, gstring));

    return imageBytes +
        HugeArrayGetCount(file, gstring) * PDF_GSTRING_ELEMENT_ESTIMATE;
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
 *                  const char *password    user/owner password of an
 *                                  encrypted file, NULL or "" for none
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

    /* ----- xref table ----- */
    XRefInit(&pdoc->xref, file, password);
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
    if (pdoc == NULL)
    {
        return;
    }

    /* No more progress reports or cancel requests for this document. */
    pdoc->progressCallback = NULL;
    pdoc->cancelRequested = FALSE;

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
/***********************************************************************
 *      PdfParseDateDigits
 ***********************************************************************
 * SYNOPSIS:        Read up to count decimal digits of a PDF date.
 * RETURNS:         value, or def if no digit is present
 ***********************************************************************/

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

/***********************************************************************
 *      PdfFormatDate
 ***********************************************************************
 * SYNOPSIS:        Format a PDF date "D:YYYYMMDDHHmmSS..." with the
 *                  system's short date and hour:minute formats.
 * RETURNS:         FALSE if the string is not a date (buf unchanged)
 *
 * STRATEGY:        The time zone suffix is ignored (shown as written).
 ***********************************************************************/

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

/***********************************************************************
 *      PdfGetInfoField
 ***********************************************************************
 * SYNOPSIS:        Entry of the document information dictionary.
 * PARAMETERS:      PdfDocHandle doc    document
 *                  const char *key    entry name, e.g. "Title"
 *                  char *buf    output (GEOS character set)
 *                  word bufSize    size of buf
 *
 * RETURNS:         TRUE if the entry exists and is not empty
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      11/23/26    Initial version (B4)
 *
 ***********************************************************************/

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

    cacheEntry->estimatedBytes = 0;

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
                 * Annotation appearances (PDF 12.5.5) are not drawn in the
                 * RM viewer (I1); links are handled by PdfFindLink.
                 * PM-TODO: Draw annotation appearance streams again
                 * (git history before I1: GfxDrawAnnotations).
                 */

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
    cacheEntry->estimatedBytes = PdfGStringVmBytes(pdoc->gstringFile,
        pageData, resourceBytes);
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
    /* A cancel request belongs to the decode that was running when the
     * user pressed Stop. Drop a leftover one instead of failing the next,
     * unrelated page with PDF_ERR_CANCELLED. */
    pdoc->cancelRequested = FALSE;

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    pageData = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!pageData)
    {
        err = PdfDecodePageContent(pdoc, pageNum, &pageData);

        /*
         * Out of memory is often only temporary in real mode: fixed blocks
         * (malloc, zlib's 32 KB window) fragment the heap, and the failed
         * attempt has just released all of its own memory. Drop the other
         * cached pages and try once more before reporting the error.
         * Only for real allocation failures -- the page resource budget
         * (also PDF_ERR_OUT_OF_MEMORY, but without GMemHadError) would
         * just fail again.
         */
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

/***********************************************************************
 *      PdfDrawPage
 ***********************************************************************
 * SYNOPSIS:        Draw page.
 * PARAMETERS:      PdfDocHandle doc    doc
 *                  word pageNum    page number
 *                  GStateHandle target    target
 *                  Boolean printing    printing
 *                  const Point *fitArea    printable area (print only)
 *                  Point *pageSizeOut    page size out
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *      Re-read the cache entry, compute page geometry, then flip Y and
 *      either rotate+clip+GrDrawGString (screen), fit+rotate+clip+
 *      GrCopyGString (print, G2) or copy at 1:1 (clipboard).
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
    Boolean printing, const Point *fitArea, Point *pageSizeOut)
{
    PdfDocInternal *pdoc;
    PdfPageGeometry geom;
    PdfError err;
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


#pragma code_seg("PdfLink")

/*
 * Links (F4). Nothing is kept in memory: a click walks the /Annots of the
 * current page once, finds the link rectangle and resolves its target.
 */

#define PDF_NAME_TREE_MAX_DEPTH 16

/***********************************************************************
 *      PdfScreenState
 ***********************************************************************
 * SYNOPSIS:        GState without a window that holds the screen
 *                  transformation of a page (PdfDrawPage, not printing).
 * RETURNS:         GState (caller frees with GrDestroyState), 0 on error
 ***********************************************************************/
static GStateHandle PdfScreenState(PdfDocInternal *pdoc, word pageNum)
{
    PdfPageGeometry geom;
    GStateHandle gs;

    if (PdfComputePageGeometry(&pdoc->catalog, &pdoc->xref, pageNum, &geom)
        != PDF_OK)
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

/***********************************************************************
 *      PdfStrCmp
 ***********************************************************************
 * SYNOPSIS:        Compare two PDF strings byte by byte (name trees).
 * RETURNS:         < 0, 0, > 0
 ***********************************************************************/
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

/***********************************************************************
 *      PdfNameTreeLookup
 ***********************************************************************
 * SYNOPSIS:        Find a key in a name tree (/Names /Dests).
 * PARAMETERS:      tree    root node (dict)
 *                  key     string key
 *                  out     value, null if not found (caller frees)
 * STRATEGY:        Walk down one node per level: /Names pairs, else the
 *                  /Kids entry whose /Limits contain the key. No
 *                  recursion, depth limited.
 ***********************************************************************/
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

/***********************************************************************
 *      PdfResolveDest
 ***********************************************************************
 * SYNOPSIS:        Resolve a destination to a page and a vertical
 *                  position.
 * PARAMETERS:      dest      explicit array, name (/Dests) or string
 *                            (name tree); a dict value uses its /D
 *                  topOut    PDF user space y of the top edge, or
 *                            PDF_DEST_NO_TOP
 * RETURNS:         page number (1-based), 0 = no valid target
 * STRATEGY:        [page /XYZ left top zoom], [page /FitH top],
 *                  [page /FitBH top] give a top; /Fit, /FitV, /FitR,
 *                  /FitB show the top of the page. The page is a page
 *                  reference (or a number, used by some producers).
 *                  ATTENTION: Zoom of /XYZ and /Fit* is ignored, the
 *                  view keeps its scale. Upgrade: set the view scale.
 ***********************************************************************/
#define PDF_DEST_NO_TOP ((sdword)0x7fffffffL)

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

/***********************************************************************
 *      PdfFindLink
 ***********************************************************************
 * SYNOPSIS:        Find the internal link at a point of a page.
 * PARAMETERS:      doc, pageNum    page on display
 *                  docX, docY      click point, document coordinates
 *                                  (as drawn by PdfDrawPage on screen)
 *                  destPage        target page (1-based)
 *                  destDocY        target y in document coordinates of
 *                                  the target page, -1 = page top
 * RETURNS:         TRUE if a link with a valid internal target is hit
 * STRATEGY:        Map the point back to PDF user space through the
 *                  same transformation as the page (GState without a
 *                  window), test the /Rect of every /Link annotation,
 *                  resolve /Dest or /A << /S /GoTo /D ... >>. Other
 *                  actions (URI, GoToR, Launch) are ignored.
 *                  PM-TODO: URI and GoToR links (Ausbauplan PM).
 ***********************************************************************/
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
