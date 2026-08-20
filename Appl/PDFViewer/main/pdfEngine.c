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

/*
 * Cache-eviction limit for decoded pages, moved out of the UI
 * (formerly PDFDOC_PAGE_COUNT_LIMIT in ui/pdfvu.goc). Private to the
 * engine now -- see pdfEngine.h's note that the cache policy is no
 * longer the UI's business.
 */
#define PDF_MAX_CACHED_PAGES	30
#define PDF_PAGE_CACHE_VM_BUDGET 0x00800000UL /* OPT-19: 8 MB estimated VM payload */
#define PDF_PAGE_GSTRING_ESTIMATE 1024UL      /* small per-page baseline */


/***********************************************************************
 *		Internal layout behind the opaque handles
 ***********************************************************************
 *
 * Private to this file. Nothing outside main/pdfEngine.goc may know
 * these shapes -- that is the entire point of PdfDocHandle/
 * PdfDocHandle being an opaque MemHandle in pdfEngine.h.
 *
 * Field-for-field, PdfDocInternal is today's PDFDoc_* instance data
 * (see ui/pdfvu.goc) moved out of the UI class and into the engine,
 * plus the new per-document progress/cancel bookkeeping from Phase 4
 * of the plan (added now so the struct layout doesn't have to change
 * again later; the fields are simply unused until Phase 4 wires up
 * the callback from gfx.goc).
 */

typedef struct {
    word		pageNum;
    VMBlockHandle	gstringBlock;
    VMBlockHandle	resourceList;		/* HugeArray of page-owned VM blocks */
    word		resourceCount;
    dword		estimatedBytes;		/* OPT-19: GString + bitmap VM estimate */
    dword		lastUse;		/* monotonically increasing LRU stamp */
} PdfPageCacheEntry;

typedef struct {
    XRef		xref;
    Catalog		catalog;

    VMFileHandle	gstringFile;		/* scratch VM file for decoded pages */
    char		gstringFileName[PATH_LENGTH_ZT];

    PdfPageCacheEntry	pageCache[PDF_MAX_CACHED_PAGES];
    dword		cacheUseClock;		/* next LRU timestamp source */
    dword		cachedVmBytes;
    word		pageCount;		/* number of pages in the source PDF */

    Boolean		ok;			/* mirrors today's PDFDoc_ok */

    PdfProgressCallback	progressCallback;	/* Phase 4; NULL until then */
    void		*progressUserData;
    GBool		cancelRequested;	/* GBool, not Boolean: a pointer to this
						 * field gets handed to Gfx.cancelFlag
						 * (gfx.goc's own GBool convention), so
						 * the types must match for that pointer
						 * to be valid */
} PdfDocInternal;



/***********************************************************************
 *		Page geometry -- shared by PdfGetPageSize and PdfDrawPage
 ***********************************************************************
 *
 * Both need the same CropBox/MediaBox + /Rotate computation that used
 * to live only inside PDFSetupGState (behind #ifdef
 * USE_FULL_PAGE_ATTRS, which local.mk defines, so this path is live).
 * Factored out here instead of duplicated.
 */
typedef struct {
    word	width;		/* on-screen (post-rotation) width, points */
    word	height;		/* on-screen (post-rotation) height, points */
    sword	clipX;		/* pre-rotation crop/media origin X, points */
    sword	clipY;		/* pre-rotation crop/media origin Y, points */
    word	clipWidth;	/* pre-rotation (native) width, points --
				 * matches clipX/clipY's own coordinate space.
				 * GrSetClipRect runs after GrApplyRotation has
				 * already been applied to the target, so the
				 * clip rect has to stay in the same pre-
				 * rotation space the page content itself is
				 * drawn in -- using the post-rotation-swapped
				 * width/height above here (as this used to)
				 * mismatched an unswapped origin against a
				 * swapped extent, clipping the wrong edge on
				 * 90/270-degree pages. */
    word	clipHeight;	/* same, for height */
    long	rotate;		/* normalized to 0/90/180/270 */
    gdouble	rawHeight;	/* PageGetHeight(page), unrotated -- only
				 * needed for PdfDrawPage's printing-
				 * translation case */
} PdfPageGeometry;

static PdfError
PdfComputePageGeometry(Catalog *catalog, XRef *xref, word pageNum, PdfPageGeometry *geom)
{
    Page page;

    if (!CatalogGetPage(catalog, pageNum, xref, &page)) {
	return PDF_ERR_INVALID_PAGE;
    }

    if (PageIsCropped(&page)) {
	geom->clipX = GdoubleToWord(PageGetCropX1(&page));
	geom->clipY = GdoubleToWord(PageGetCropY1(&page));
	geom->width  = GdoubleToWord(PageGetCropX2(&page)) - geom->clipX;
	geom->height = GdoubleToWord(PageGetCropY2(&page)) - geom->clipY;
    } else {
	geom->clipX = GdoubleToWord(PageGetX1(&page));
	geom->clipY = GdoubleToWord(PageGetY1(&page));
	geom->width  = GdoubleToWord(PageGetWidth(&page));
	geom->height = GdoubleToWord(PageGetHeight(&page));
    }

    /* capture native (pre-swap) extent before width/height get
     * swapped for on-screen 90/270 reporting below -- this pair
     * stays in the same coordinate space as clipX/clipY, for
     * GrSetClipRect */
    geom->clipWidth = geom->width;
    geom->clipHeight = geom->height;

    geom->rotate = PageGetRotate(&page);
    if (geom->rotate == 90L || geom->rotate == 270L) {
	word temp = geom->width;
	geom->width = geom->height;
	geom->height = temp;
    } else if (geom->rotate != 0L && geom->rotate != 180L) {
	geom->rotate = 0L;
    }

    geom->rawHeight = PageGetHeight(&page);
    PageFree(&page);
    return PDF_OK;
}


/***********************************************************************
 *		Small internal helpers
 ***********************************************************************
 */

/*
 * Lock a document handle and return a pointer to its internals, or
 * NULL if the handle is bogus. Every public function below starts
 * with this instead of trusting its caller.
 *
 * PDF_NULL_DOC is a legitimate call (e.g. PdfIsValid before any
 * document was ever opened) and stays a plain runtime check -- but a
 * non-null handle that ISN'T a valid, currently-allocated MemHandle
 * (stale after PdfClose, garbage, etc.) is always a caller bug, so
 * that gets an EC check instead: traps immediately in an EC build
 * with the bad handle visible in Swat, compiles away entirely in
 * Retail.
 */
static PdfDocInternal *PdfLockDoc(PdfDocHandle doc)
{
    if (doc == PDF_NULL_DOC)
	    return NULL;

EC( ECCheckMemHandle((MemHandle) doc) );

    return (PdfDocInternal *) MemLock((MemHandle) doc);
}

static void PdfUnlockDoc(PdfDocHandle doc)
{
    if (doc != PDF_NULL_DOC)
	    MemUnlock((MemHandle) doc);
}

/*
 * The decoded-page cache is intentionally tiny and fixed-size.  A
 * linear scan over 30 entries is cheaper than allocating and locking
 * a pageCount-sized lookup table, and keeps conventional-memory usage
 * independent of document length.
 */
static void
PdfFreePageCacheEntry(PdfDocInternal *pdoc, PdfPageCacheEntry *entry)
{
    word i;

EC( ECCheckBounds(pdoc) );
EC( ECCheckBounds(entry) );
EC( ECVMCheckVMFile(pdoc->gstringFile) );

    /* A recorded GString may reference HugeBitmaps.  Drop the
     * GString first, then the referenced bitmap data. */
    if (entry->gstringBlock != NullHandle) {
EC( ECVMCheckVMBlockHandle(pdoc->gstringFile, entry->gstringBlock) );
	VMFreeVMChain(pdoc->gstringFile,
		      VMCHAIN_MAKE_FROM_VM_BLOCK(entry->gstringBlock));
    }

    if (entry->resourceList != NullHandle) {
	EC( ECVMCheckVMBlockHandle(pdoc->gstringFile, entry->resourceList) );
	for (i = 0; i < entry->resourceCount; ++i) {
	    VMBlockHandle *resource;
	    VMBlockHandle block;
	    word elemSize;

	    HugeArrayLock(pdoc->gstringFile, entry->resourceList, i,
			  (void **) &resource, &elemSize);
	    block = *resource;
	    HugeArrayUnlock(resource);

	    EC_ERROR_IF(block == NullHandle, -1);
	    EC( ECVMCheckVMBlockHandle(pdoc->gstringFile, block) );
	    /* BMT_COMPLEX bitmap data is a HugeArray.  Gfx used
	     * BMD_LEAVE_DATA after constructing it, transferring
	     * lifetime ownership to the page cache. */
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

static void PdfTouchPageCacheEntry(PdfDocInternal *pdoc, PdfPageCacheEntry *entry)
{
EC( ECCheckBounds(pdoc));
EC( ECCheckBounds(entry));

    ++pdoc->cacheUseClock;
    if (pdoc->cacheUseClock == 0) {
        PdfPageCacheEntry *e = pdoc->pageCache;
        PdfPageCacheEntry *end = e + PDF_MAX_CACHED_PAGES;
        dword stamp = 0;

        /* Renormalize the tiny cache on the extremely unlikely 32-bit
         * wrap.  Exact historical ordering at this one instant is less
         * important than keeping all live timestamps non-zero and
         * comparable afterward. */
        for (; e < end; ++e) {
            if (e->gstringBlock != NullHandle)
                e->lastUse = ++stamp;
            else
                e->lastUse = 0;
        }
        pdoc->cacheUseClock = stamp + 1;
    }

    entry->lastUse = pdoc->cacheUseClock;
}

static PdfPageCacheEntry *
PdfFindCachedPage(PdfDocInternal *pdoc, word pageNum)
{
    word i;

EC( ECCheckBounds(pdoc) );

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i) {
	if (pdoc->pageCache[i].gstringBlock != NullHandle &&
	    pdoc->pageCache[i].pageNum == pageNum) {
	    PdfTouchPageCacheEntry(pdoc, &pdoc->pageCache[i]);
	    return &pdoc->pageCache[i];
	}
    }
    return NULL;
}

static PdfPageCacheEntry *
PdfFindFreePageCacheEntry(PdfDocInternal *pdoc)
{
    word i;

    EC( ECCheckBounds(pdoc) );

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i) {
	if (pdoc->pageCache[i].gstringBlock == NullHandle) {
	    return &pdoc->pageCache[i];
	}
    }
    return NULL;
}

/* OPT-19: enforce a byte budget in addition to the 30-slot cap.
 * The just-decoded page is pinned for this pass; if it alone is
 * larger than the budget, all older pages are evicted and it stays. */
static void
PdfEnforceCacheByteBudget(PdfDocInternal *pdoc, PdfPageCacheEntry *keepEntry)
{
EC( ECCheckBounds(pdoc));
EC( ECCheckBounds(keepEntry));

    while (pdoc->cachedVmBytes > PDF_PAGE_CACHE_VM_BUDGET) {
        PdfPageCacheEntry *entry = pdoc->pageCache;
        PdfPageCacheEntry *end = entry + PDF_MAX_CACHED_PAGES;
        PdfPageCacheEntry *oldest = NULL;
        dword oldestUse = 0xffffffffUL;

        for (; entry < end; ++entry) {
            if (entry == keepEntry || entry->gstringBlock == NullHandle)
                continue;

            if (entry->lastUse < oldestUse) {
                oldestUse = entry->lastUse;
                oldest = entry;
            }
        }

        if (oldest == NULL)
            break;

        PdfFreePageCacheEntry(pdoc, oldest);
    }
}


/***********************************************************************
 *		A. Lifecycle
 ***********************************************************************
 */

/***********************************************************************
 *		PdfOpen
 ***********************************************************************
 * SYNOPSIS:	    Parse the xref table and page catalog of an
 *		    already-open file and hand back a document handle.
 * PARAMETERS:	    FileHandle file, PdfDocHandle *docOut
 * RETURNS:	    PdfError
 * SIDE EFFECTS:    opens a scratch VM file (deleted again by PdfClose)
 *
 * STRATEGY:	    Wraps XRefInit/XRefIsOk, the scratch-file VMOpen,
 *		    XRefGetCatalog/CatalogInit/CatalogIsOk -- same
 *		    sequence as today's MSG_GEN_DOCUMENT_PHYSICAL_OPEN.
 *
 *		    Three deliberate differences from the original:
 *
 *		    1. `file` is never closed by this function, on
 *		       success OR failure. The caller opened it and
 *		       stays responsible for it; the original
 *		       errorClose: path called FileClose() on it, but
 *		       that's a UI/file-selector decision, not the
 *		       engine's. Whoever ports
 *		       MSG_GEN_DOCUMENT_PHYSICAL_OPEN onto this facade
 *		       still needs to FileClose() on a non-PDF_OK return.
 *
 *		    2. The decoded-page cache is a fixed-size array in the
 *		       document handle. Its memory use is therefore O(1)
 *		       instead of O(pageCount), and opening a document no
 *		       longer allocates one VMBlockHandle per source page.
 *
 *		    3. A bad xref is no longer always PDF_ERR_BAD_XREF:
 *		       if XRefCheckEncrypted() says the file has an
 *		       /Encrypt entry (already fatal today, no
 *		       decryption support, unchanged from upstream xpdf
 *		       0.80), we report PDF_ERR_ENCRYPTED instead, so the
 *		       UI can tell "encrypted, unsupported" apart from
 *		       "genuinely corrupt". xref->trailerDict is still
 *		       valid to query here -- XRefInit's error path only
 *		       frees its local stream, not the trailer dict.
 *
 *		    No error dialog is shown here either -- that's
 *		    UI-side, driven off the returned PdfError.
 *
 ***********************************************************************/
PdfError
PdfOpen(FileHandle file, PdfDocHandle *docOut)
{
    MemHandle		mh;
    PdfDocInternal	*pdoc;
    Obj			catDict;
    char		*p;
    PdfError		err;

    *docOut = PDF_NULL_DOC;
    GMemClearError();

    if (file == 0) {
	return PDF_ERR_NOT_A_PDF;
    }

    mh = MemAllocSetOwner(GeodeGetCodeProcessHandle(),
			  sizeof(PdfDocInternal), HF_DYNAMIC | HF_SWAPABLE,
			  HAF_ZERO_INIT | HAF_NO_ERR);
    if (mh == NullHandle) {
	return PDF_ERR_OUT_OF_MEMORY;
    }

    pdoc = (PdfDocInternal *) MemLock(mh);

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
    if (GMemHadError()) {
        err = PDF_ERR_OUT_OF_MEMORY;
        goto errorClose;
    }
    if (!XRefIsOk(&pdoc->xref)) {
	/* xref->trailerDict is still valid at this point (XRefInit's
	 * error path only StreamFree()s its local stream, it doesn't
	 * touch the trailer dict) -- so we can still ask whether this
	 * was an /Encrypt-triggered failure and report that precisely
	 * instead of the generic PDF_ERR_BAD_XREF. XRefInit itself now
	 * tries the Standard Security Handler with an empty user
	 * password first (main/crypt.goc) and only leaves xref->ok
	 * false if that genuinely doesn't work (a real password is
	 * required, or the file uses AES/a handler this port doesn't
	 * support) -- so reaching this branch at all means decryption
	 * was already tried and didn't apply, not that it was never
	 * attempted. */
	err = XRefCheckEncrypted(&pdoc->xref) ? PDF_ERR_ENCRYPTED : PDF_ERR_BAD_XREF;
	goto errorClose;
    }

    /* ----- scratch VM file for decoded page gstrings, temp file
     * in the wastebasket, exactly like the original ----- */
    p = pdoc->gstringFileName;
    FileConstructFullPath(&p, sizeof(pdoc->gstringFileName),
			  SP_WASTE_BASKET, "", TRUE);

    pdoc->gstringFile =
	VMOpen(pdoc->gstringFileName,
	       VMAF_FORCE_READ_WRITE | VMAF_USE_BLOCK_LEVEL_SYNCHRONIZATION,
	       VMO_TEMP_FILE,
	       0);
    if (!pdoc->gstringFile) {
	err = PDF_ERR_NO_SCRATCH_FILE;
	goto errorClose;
    }

    /* ----- page catalog ----- */
    XRefGetCatalog(&pdoc->xref, &catDict);
    CatalogInit(&pdoc->catalog, &catDict, pdoc->gstringFile, &pdoc->xref);
    ObjFree(&catDict);

    if (GMemHadError()) {
	err = PDF_ERR_OUT_OF_MEMORY;
	goto errorClose;
    }
    if (!CatalogIsOk(&pdoc->catalog)) {
	err = PDF_ERR_BAD_CATALOG;
	goto errorClose;
    }

    /* ----- success: fill in what PdfGetPageCount/PdfDecodePage need ----- */
    pdoc->pageCount = (word) CatalogGetNumPages(&pdoc->catalog);

    pdoc->ok = TRUE;

    MemUnlock(mh);
    *docOut = (PdfDocHandle) mh;
    return PDF_OK;

errorClose:
    MemUnlock(mh);
    PdfClose((PdfDocHandle) mh);
    return err;
}

/***********************************************************************
 *		PdfClose
 ***********************************************************************
 * SYNOPSIS:	    Free a document handle and everything it owns.
 * PARAMETERS:	    PdfDocHandle doc
 * RETURNS:	    nothing
 * SIDE EFFECTS:    frees the MemHandle; doc is invalid after this call
 *
 * STRATEGY:	    Correct since Phase 0; now also the error-cleanup
 *		    path PdfOpen jumps to on any failure (see
 *		    errorClose: above). Mirrors today's `errorClose:`
 *		    label in MSG_GEN_DOCUMENT_PHYSICAL_OPEN plus the
 *		    cleanup half of MSG_GEN_DOCUMENT_CLOSE.
 *
 ***********************************************************************/
void
PdfClose(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;

    if (doc == PDF_NULL_DOC) {
	return;
    }

    pdoc = PdfLockDoc(doc);

    /* No per-page VMFreeVMChain loop needed here: the original
     * MSG_GEN_DOCUMENT_CLOSE had exactly this loop but #if 0'd out
     * with an open question ("XXX: is this necessary?"). It isn't --
     * deleting the whole scratch file below takes every VM chain in
     * it with it. */

    CatalogFree(&pdoc->catalog);
    XRefFree(&pdoc->xref);

    if (pdoc->gstringFile) {
	VMClose(pdoc->gstringFile, FALSE);
	FileDelete(pdoc->gstringFileName);
    }

    PdfUnlockDoc(doc);
    MemFree((MemHandle) doc);
}

/***********************************************************************
 *		PdfIsValid
 ***********************************************************************
 * SYNOPSIS:	    Report whether a document opened successfully.
 * PARAMETERS:	    PdfDocHandle doc
 * RETURNS:	    Boolean
 * SIDE EFFECTS:    none
 *
 ***********************************************************************/
Boolean
PdfIsValid(PdfDocHandle doc)
{
    PdfDocInternal	*pdoc;
    Boolean		valid;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return FALSE;
    }

    valid = pdoc->ok;

    PdfUnlockDoc(doc);
    return valid;
}


/***********************************************************************
 *		B. Document metadata
 ***********************************************************************
 */

/***********************************************************************
 *		PdfGetPageCount
 ***********************************************************************
 * SYNOPSIS:	    Return the number of pages in the document.
 * PARAMETERS:	    PdfDocHandle doc
 * RETURNS:	    word
 * SIDE EFFECTS:    none
 *
 * STRATEGY:	    Reads pdoc->pageCount, which PdfOpen will set from
 *		    CatalogGetNumPages in Phase 1. Returns 0 for an
 *		    invalid handle or an unopened document, which is
 *		    the same thing a real empty/failed document would
 *		    report, so callers don't need a special case.
 *
 ***********************************************************************/
word
PdfGetPageCount(PdfDocHandle doc)
{
    PdfDocInternal	*pdoc;
    word		count;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return 0;
    }

    count = pdoc->pageCount;

    PdfUnlockDoc(doc);
    return count;
}

/***********************************************************************
 *		PdfGetPageSize
 ***********************************************************************
 * SYNOPSIS:	    Real per-page CropBox/MediaBox + /Rotate size.
 * PARAMETERS:	    PdfDocHandle doc, word pageNum, Point *sizeOut
 * RETURNS:	    PdfError
 * SIDE EFFECTS:    none
 *
 * STRATEGY:	    Pulled forward from the original Phase 5 plan: once
 *		    it turned out PdfDrawPage needs this exact geometry
 *		    computation anyway (see "Weg 1" discussion), keeping
 *		    it stubbed here made no sense. Shares
 *		    PdfComputePageGeometry with PdfDrawPage.
 *
 ***********************************************************************/
PdfError
PdfGetPageSize(PdfDocHandle doc, word pageNum, Point *sizeOut)
{
    PdfDocInternal	*pdoc;
    PdfPageGeometry	geom;
    PdfError		err;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok) {
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_HANDLE;
    }
    if (pageNum < 1 || pageNum > pdoc->pageCount) {
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_PAGE;
    }

    err = PdfComputePageGeometry(&pdoc->catalog, &pdoc->xref, pageNum, &geom);
    if (err == PDF_OK && sizeOut != NULL) {
	sizeOut->P_x = geom.width;
	sizeOut->P_y = geom.height;
    }

    PdfUnlockDoc(doc);
    return err;
}


/***********************************************************************
 *		C. Rendering
 ***********************************************************************
 */

/***********************************************************************
 *		PdfDecodePageContent (private helper)
 ***********************************************************************
 * SYNOPSIS:	    Actually decode a page's content stream into a
 *		    cached GString, evicting the oldest cached page
 *		    first if the cache is full.
 * PARAMETERS:	    PdfDocInternal *pdoc (locked), word pageNum
 * RETURNS:	    PdfError; *pageDataOut set to the new VMBlockHandle
 *		    on PDF_OK (0 on failure)
 * SIDE EFFECTS:    updates one entry in pdoc->pageCache[]
 *
 * STRATEGY:	    Wraps the body of the original PDFGetPageGString,
 *		    minus the UI-object messaging: no busy-cursor
 *		    (MSG_GEN_APPLICATION_MARK_BUSY/NOT_BUSY) and no
 *		    page-decode progress dialog
 *		    (PDFPageProgressDialog). Both are the caller's
 *		    responsibility now -- see the note on PdfDecodePage
 *		    in pdfEngine.h. The eviction loop mirrors
 *		    ENFORCE_PAGE_GSTRING_LIMIT exactly, just against
 *		    PDF_MAX_CACHED_PAGES instead of a UI constant.
 *
 ***********************************************************************/
static PdfError
PdfDecodePageContent(PdfDocInternal *pdoc, word pageNum, VMBlockHandle *pageDataOut)
{
    Page		page;
    Gfx			gfx;
    Dict		*resDict;
    Handle		gstring;
    Obj			pageObj, resDictObj;
    VMBlockHandle	pageData;
    PdfPageCacheEntry	*cacheEntry;
    GBool		status;
    GBool		resourceLimitExceeded = gFalse;
    dword		resourceBytes = 0;

    EC( ECCheckBounds(pdoc) );
    EC( ECCheckBounds(pageDataOut) );

    *pageDataOut = 0;
    GMemClearError();

    /* ----- reserve a cache slot; evict the least-recently-used page if full ----- */
    cacheEntry = PdfFindFreePageCacheEntry(pdoc);
    if (cacheEntry == NULL) {
	word		i;
	dword		oldestUse = 0xffffffffUL;

	for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i) {
	    if (pdoc->pageCache[i].lastUse < oldestUse) {
		oldestUse = pdoc->pageCache[i].lastUse;
		cacheEntry = &pdoc->pageCache[i];
	    }
	}
	EC_ERROR_IF(cacheEntry == NULL, -1);
	PdfFreePageCacheEntry(pdoc, cacheEntry);
    }

    /* OPT-10: reserve a VM-backed resource list before decoding.
     * Gfx appends every HugeBitmap it creates to this list. */
    cacheEntry->resourceList = HugeArrayCreate(pdoc->gstringFile,
				      sizeof(VMBlockHandle), 0);
    cacheEntry->resourceCount = 0;
    cacheEntry->estimatedBytes = 0;
    if (cacheEntry->resourceList == NullHandle) {
	return PDF_ERR_OUT_OF_MEMORY;
    }

    /* ----- lazily materialize this page and decode its content stream ----- */
    if (!CatalogGetPage(&pdoc->catalog, pageNum, &pdoc->xref, &page)) {
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return GMemHadError() ? PDF_ERR_OUT_OF_MEMORY : PDF_ERR_INVALID_PAGE;
    }
    PageGetContents(&page, &pageObj, &pdoc->xref);
    if (GMemHadError()) {
        ObjFree(&pageObj);
        PageFree(&page);
        PdfFreePageCacheEntry(pdoc, cacheEntry);
        return PDF_ERR_OUT_OF_MEMORY;
    }

    gstring = GrCreateGString(pdoc->gstringFile, GST_VMEM, &pageData);
    if (!gstring) {
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
    if (!isNull(&pageObj)) {
	PageCopyResourceDict(&page, &resDictObj, &pdoc->xref);
	if (GMemHadError()) {
	    ObjFree(&resDictObj);
	    status = gFalse;
	} else {
	resDict = getDict(&resDictObj);

	GfxInit(&gfx, gstring, resDict, &pdoc->xref, pdoc->gstringFile);
	if (GMemHadError()) {
	    GfxFree(&gfx);
	    ObjFree(&resDictObj);
	    status = gFalse;
	} else {
	gfx.pageResourceList = cacheEntry->resourceList;
	gfx.pageResourceCount = 0;
	gfx.pageResourceBytes = 0;

	/* Phase 4: hand the per-document progress callback and cancel
	 * flag to Gfx, so gfx.goc's own progress/cancel checks (content-
	 * stream ticks and the per-image dialog lifecycle) go through
	 * the engine's callback instead of calling UI functions or a
	 * global flag directly. */
	gfx.progressCallback = pdoc->progressCallback;
	gfx.progressUserData = pdoc->progressUserData;
	gfx.cancelFlag = &pdoc->cancelRequested;

	status = GfxDisplay(&gfx, &pageObj);
	if (GMemHadError())
	    status = gFalse;
	resourceLimitExceeded = gfx.resourceLimitExceeded;

	/* Annotations (comments, highlights, filled-in form fields,
	 * ...) are never part of the page's own content stream -- PDF
	 * spec 12.5.5. Draw their Normal appearances now, while gfx
	 * (and its resolved CTM/state) is still alive, using the same
	 * Page struct already locked above. A failure here doesn't
	 * invalidate the page itself (status stays whatever
	 * GfxDisplay reported for the main content), matching the
	 * "degrade gracefully" approach the rest of the annotation
	 * code already takes internally. */
	if (status) {
	    Obj annotsObj;
	    PageGetAnnots(&page, &annotsObj, &pdoc->xref);
	    GfxDrawAnnotations(&gfx, &annotsObj);
	    ObjFree(&annotsObj);
	    if (GMemHadError())
		status = gFalse;
	    if (gfx.resourceLimitExceeded) {
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

    if (status) {
	GrDestroyGString(gstring, NULL, GSKT_LEAVE_DATA);
    } else {
	GrDestroyGString(gstring, NULL, GSKT_KILL_DATA);
	pageData = 0;
    }

    ObjFree(&pageObj);
    PageFree(&page);

    if (!status) {
	PdfFreePageCacheEntry(pdoc, cacheEntry);
	if (GMemHadError())
	    return PDF_ERR_OUT_OF_MEMORY;
	if (resourceLimitExceeded)
	    return PDF_ERR_OUT_OF_MEMORY;
	/* GfxDisplay only returns gFalse for a structurally malformed
	 * page-contents object (not a stream / array of streams) --
	 * verified by reading GfxGo itself: its main loop always
	 * returns gTrue, cancellation was never wired to abort it (the
	 * function's own comment says as much). A cancel only cuts
	 * short whatever single image is being decoded at the time
	 * (see the two image-draw functions in gfx.goc); the page
	 * decode as a whole still runs to completion either way. So
	 * PDF_ERR_CANCELLED below is reachable in principle but not in
	 * practice through this path -- kept for correctness, not
	 * because it fires often. */
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
 *		PdfDecodePage
 ***********************************************************************
 * SYNOPSIS:	    Decode a page if it is not already cached.
 * PARAMETERS:	    PdfDocHandle doc, word pageNum
 * RETURNS:	    PdfError
 * SIDE EFFECTS:    may write into doc's page cache
 *
 * OPT-21:	    No transient page handle is allocated anymore.  The
 *		    document cache is the source of truth; PdfDrawPage receives
 *		    doc + page number directly.
 *
 ***********************************************************************/
PdfError
PdfDecodePage(PdfDocHandle doc, word pageNum)
{
    PdfDocInternal	*pdoc;
    PdfPageCacheEntry	*cacheEntry;
    VMBlockHandle	pageData;
    PdfError		err;

    GMemClearError();

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok) {
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_HANDLE;
    }
    if (pageNum < 1 || pageNum > pdoc->pageCount) {
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_PAGE;
    }
    if (pdoc->cancelRequested) {
	pdoc->cancelRequested = FALSE;
	PdfUnlockDoc(doc);
	return PDF_ERR_CANCELLED;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    pageData = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!pageData) {
	err = PdfDecodePageContent(pdoc, pageNum, &pageData);
	if (err != PDF_OK) {
	    PdfUnlockDoc(doc);
	    return err;
	}
    }

    PdfUnlockDoc(doc);
    return PDF_OK;
}

/***********************************************************************
 *		PdfDrawPage
 ***********************************************************************
 * SYNOPSIS:	    Render a decoded page onto a target GState.
 * PARAMETERS:	    PdfDocHandle doc, word pageNum, GStateHandle target,
 *		    Boolean printing, Point *pageSizeOut
 * RETURNS:	    PdfError
 * SIDE EFFECTS:    none (GState transform is saved/restored internally)
 *
 * STRATEGY:	    Wraps PDFSetupGState (the USE_FULL_PAGE_ATTRS path,
 *		    which local.mk enables) plus the actual render call.
 *		    Re-reads the cache entry from the document; cache
 *		    pressure may have evicted it since PdfDecodePage.
 *		    Screen
 *		    (printing == FALSE) uses GrDrawGString; print and
 *		    clipboard (printing == TRUE, matching the original's
 *		    own PDFSetupGState(..., TRUE) call at both sites)
 *		    use GrCopyGString.
 *
 ***********************************************************************/
PdfError
PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target, Boolean printing, Point *pageSizeOut)
{
    PdfDocInternal	*pdoc;
    PdfPageGeometry	geom;
    PdfError		err;
    PdfPageCacheEntry	*cacheEntry;
    VMBlockHandle	gstringBlock;
    Handle		gstring;
    sword		transX, transY;
    word		elem;
    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return PDF_ERR_INVALID_HANDLE;
    }
    if (!pdoc->ok || pageNum < 1 || pageNum > pdoc->pageCount) {
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_PAGE;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    gstringBlock = (cacheEntry != NULL) ? cacheEntry->gstringBlock : NullHandle;

    if (!gstringBlock) {
	/* evicted since PdfDecodePage -- caller needs to decode again */
	PdfUnlockDoc(doc);
	return PDF_ERR_INVALID_PAGE;
    }

    err = PdfComputePageGeometry(&pdoc->catalog, &pdoc->xref, pageNum, &geom);
    if (err != PDF_OK) {
	PdfUnlockDoc(doc);
	return err;
    }

    if (pageSizeOut != NULL) {
	pageSizeOut->P_x = geom.width;
	pageSizeOut->P_y = geom.height;
    }

    gstring = GrLoadGString(pdoc->gstringFile, GST_VMEM, gstringBlock);

    GrSaveState(target);

    /* flip Y: pdf draws with the origin at the lower left */
    GrApplyScale(target, (1L) << 16, (-1L) << 16);

    if (!printing) {
	GrApplyRotation(target, MakeWWFixed(geom.rotate));

	switch ((int) geom.rotate) {
	case 0:   transX = 0;                    transY = -(sword) geom.height; break;
	case 90:  transX = 0;                    transY = 0;                    break;
	case 180: transX = -(sword) geom.width;  transY = 0;                    break;
	case 270: transX = -(sword) geom.height; transY = -(sword) geom.width;  break;
	default:  transX = 0;                    transY = 0;                    break;
	}
	GrApplyTranslation(target,
			    MakeWWFixed(transX - geom.clipX),
			    MakeWWFixed(transY - geom.clipY));

	GrSetClipRect(target, PCT_REPLACE, geom.clipX, geom.clipY,
		      geom.clipX + geom.clipWidth, geom.clipY + geom.clipHeight);

	GrDrawGString(target, gstring, 0, 0, 0, &elem);
    } else {
	GrApplyTranslation(target, 0, -(GdoubleToWWFixed(geom.rawHeight)));

	GrCopyGString(gstring, target, 0);
    }

    GrRestoreState(target);

    GrDestroyGString(gstring, NULL, GSKT_LEAVE_DATA);

    PdfUnlockDoc(doc);

    return PDF_OK;
}

/***********************************************************************
 *		PdfDiscardPage
 ***********************************************************************
 * SYNOPSIS:	    Evict one page from the cache.
 * PARAMETERS:	    PdfDocHandle doc, word pageNum
 * RETURNS:	    nothing
 * SIDE EFFECTS:    frees the cached VM resources if present
 *
 ***********************************************************************/
void
PdfDiscardPage(PdfDocHandle doc, word pageNum)
{
    PdfDocInternal	*pdoc;
    PdfPageCacheEntry	*cacheEntry;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return;
    }

    cacheEntry = PdfFindCachedPage(pdoc, pageNum);
    if (cacheEntry != NULL) {
	PdfFreePageCacheEntry(pdoc, cacheEntry);
    }

    PdfUnlockDoc(doc);
}

/***********************************************************************
 *		PdfDiscardAllPages
 ***********************************************************************
 * SYNOPSIS:	    Evict every cached page of a document.
 * PARAMETERS:	    PdfDocHandle doc
 * RETURNS:	    nothing
 * SIDE EFFECTS:    frees every cached VM chain.
 *
 ***********************************************************************/
void
PdfDiscardAllPages(PdfDocHandle doc)
{
    PdfDocInternal	*pdoc;
    word		i;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return;
    }

    for (i = 0; i < PDF_MAX_CACHED_PAGES; ++i) {
	if (pdoc->pageCache[i].gstringBlock != NullHandle ||
	    pdoc->pageCache[i].resourceList != NullHandle) {
	    PdfFreePageCacheEntry(pdoc, &pdoc->pageCache[i]);
	}
    }
    PdfUnlockDoc(doc);
}


/***********************************************************************
 *		D. Progress / cancellation
 ***********************************************************************
 *
 * Bookkeeping only in Phase 0 -- nothing calls these back yet.
 * gfx.goc keeps calling PDFUpdateProgress etc. directly until Phase 4.
 */

/***********************************************************************
 *		PdfSetProgressCallback
 ***********************************************************************/
void
PdfSetProgressCallback(PdfDocHandle doc, PdfProgressCallback callback, void *userData)
{
    PdfDocInternal *pdoc;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return;
    }

    pdoc->progressCallback = callback;
    pdoc->progressUserData = userData;

    PdfUnlockDoc(doc);
}

/***********************************************************************
 *		PdfCancelDecode
 ***********************************************************************/
void
PdfCancelDecode(PdfDocHandle doc)
{
    PdfDocInternal *pdoc;

    pdoc = PdfLockDoc(doc);
    if (pdoc == NULL) {
	return;
    }

    pdoc->cancelRequested = TRUE;

    PdfUnlockDoc(doc);
}
