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
 * FILE:          pdfEngine.h
 *
 * AUTHOR:        Jirka Kunze: 18.08.2026
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      18.08.26  JK        Relicensed under Apache 2.0, cleanup.
 *
 * DESCRIPTION:
 *      Public interface for the PDF engine.
 ***********************************************************************/

#ifndef _PDF_ENGINE_H
#define _PDF_ENGINE_H


#include <geos.h>
#include <graphics.h>
#include <heap.h>
#include <char.h>


/***********************************************************************
 *		Opaque handles
 ***********************************************************************
 *
 * Backed by GEOS MemHandle rather than a plain struct pointer. This is
 * the idiomatic GEOS choice, not just a style preference: blocks under
 * a MemHandle can be swapped/moved by the compacting heap, so nothing
 * outside the engine may hold a raw pointer to the document data
 * across calls. The UI only ever holds the handle value; the engine
 * locks it (MemLock) for the duration of each call and unlocks it
 * before returning. The struct definitions behind these handles
 * (PdfDocInternal, PdfPageInternal) are private to main/pdfEngine.goc
 * and are not declared here.
 *
 * Ownership is unambiguous: whoever gets the handle from
 * PdfOpen is responsible for handing the document handle back to
 * PdfClose exactly once.
 */

/* An open PDF document (today: PDFDoc_xref + PDFDoc_catalog + the
 * scratch VM file that holds decoded pages). */
typedef MemHandle PdfDocHandle;

#define PDF_NULL_DOC  ((PdfDocHandle)NullHandle)


/***********************************************************************
 *		Error codes
 ***********************************************************************
 *
 * Every engine call that can fail returns one of these instead of the
 * scattered EC_WARNING(-1) calls used today. PDF_OK is guaranteed to
 * be zero, so "if (PdfOpen(...))" style error checks work.
 */
typedef enum {
    PDF_OK = 0,
    PDF_ERR_NOT_A_PDF,		/* xref/header didn't parse */
    PDF_ERR_BAD_XREF,		/* XRefIsOk() came back false */
    PDF_ERR_ENCRYPTED,		/* file has an /Encrypt entry */
    PDF_ERR_BAD_CATALOG,	/* CatalogIsOk() came back false */
    PDF_ERR_NO_SCRATCH_FILE,	/* couldn't open the VM scratch file */
    PDF_ERR_OUT_OF_MEMORY,
    PDF_ERR_INVALID_PAGE,	/* page number out of range */
    PDF_ERR_CANCELLED,		/* decode aborted via PdfCancelDecode */
    PDF_ERR_INVALID_HANDLE	/* PDF_NULL_DOC or stale document handle passed in */
} PdfError;


/***********************************************************************
 *		A. Lifecycle
 ***********************************************************************
 */

/*
 * Open a PDF document from an already-open GEOS file handle (the UI's
 * GenDocument superclass owns opening/selecting the file; the engine
 * only ever consumes a FileHandle).
 *
 * Wraps: XRefInitNull/XRefInit/XRefIsOk, XRefGetCatalog,
 *        CatalogInitNull/CatalogInit/CatalogIsOk, VMOpen of the
 *        scratch file.
 *
 * On any error, *docOut is left at PDF_NULL_DOC and everything
 * allocated so far is freed before returning -- the caller does not
 * need to call PdfClose after a failed PdfOpen.
 */
PdfError PdfOpen(FileHandle file, PdfDocHandle *docOut);

/*
 * Close a document and free everything associated with it, including
 * any pages still decoded/cached (equivalent to PdfDiscardAllPages
 * first). Safe to call with PDF_NULL_DOC (no-op).
 *
 * Wraps: CatalogFree, XRefFree, VMClose + FileDelete of the scratch file.
 */
void PdfClose(PdfDocHandle doc);

/*
 * Cheap validity check, e.g. for use in MSG_VIS_DRAW before touching a
 * document that failed to open. Equivalent to today's PDFDoc_ok.
 */
Boolean PdfIsValid(PdfDocHandle doc);


/***********************************************************************
 *		B. Document metadata
 ***********************************************************************
 */

/*
 * Wraps: CatalogGetNumPages.
 */
word PdfGetPageCount(PdfDocHandle doc);

/*
 * Page dimensions in points, post-rotation (i.e. what you'd actually
 * see on screen). Real per-page CropBox/MediaBox + /Rotate lookup --
 * this replaced the UI's old hardcoded 8.5x11 assumption. Shares its
 * geometry computation with PdfDrawPage (see there for why the two
 * couldn't stay separate).
 */
PdfError PdfGetPageSize(PdfDocHandle doc, word pageNum, Point *sizeOut);


/***********************************************************************
 *		C. Rendering (two-phase: decode once, draw many times)
 ***********************************************************************
 *
 * This mirrors the existing PDFGetPageGString / PDFSetupGState split,
 * it is not a new design: decoding a page's content stream into a
 * GString is the expensive part and is cached; drawing is just a
 * cheap replay onto any target GState. PdfDrawPage is therefore the
 * one function behind screen redraw, printing, AND clipboard copy,
 * which today are three separate call sites -- internally it still
 * picks between GrDrawGString (screen) and GrCopyGString (print/
 * clipboard) the same way the original does, driven by `printing`.
 *
 * IMPORTANT for whoever wires the UI to this: the original
 * PDFGetPageGString brackets the decode with a busy cursor
 * (MSG_GEN_APPLICATION_MARK_BUSY/NOT_BUSY) and a page-decode progress
 * dialog (PDFPageProgressDialog). Neither belongs in the engine --
 * that's exactly the kind of UI-object messaging this facade exists
 * to keep out. The caller is still responsible for showing/hiding
 * both around its PdfDecodePage call. What PdfSetProgressCallback
 * (section D) DOES now cover, as of Phase 4: the page-decode progress
 * bar's value, and the separate per-image progress dialog that
 * gfx.goc puts up/updates/dismisses on its own as it hits each image
 * -- the caller can't bracket that one from outside since it doesn't
 * know when or how many images a page contains.
 */

/*
 * Decode a page's content stream if it is not already cached.
 * No transient page handle is allocated; the document cache owns the
 * decoded GString and all page resources.
 */
PdfError PdfDecodePage(PdfDocHandle doc, word pageNum);

/*
 * Render a decoded page onto a target GState.
 *
 * printing:    FALSE for on-screen display, TRUE for printing OR
 *              clipboard copy (both used PDFSetupGState(..., TRUE)
 *              in the original; the flag really means "off-screen
 *              target", not literally "printer"). Controls both the
 *              rotation/translation/clip setup and which GEOS
 *              primitive is used to render.
 * pageSizeOut: optional (may be NULL); receives the same
 *              post-rotation page size PdfGetPageSize would return,
 *              since PdfDrawPage computes it anyway to set up the
 *              transform. Lets the caller update its view bounds
 *              (MSG_GEN_VIEW_SET_DOC_BOUNDS) without a second call.
 *
 * Saves/restores the target GState's transform internally, so callers
 * no longer need to bracket the call with GrSaveState/GrRestoreState
 * themselves (two of today's three call sites already did this
 * externally; folding it in here is a no-op for those and harmless
 * for the third).
 *
 * Wraps: PDFSetupGState + GrDrawGString/GrCopyGString.
 */
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
		      Boolean printing, Point *pageSizeOut);

/*
 * Explicitly evict one decoded page from the document cache.
 */
void PdfDiscardPage(PdfDocHandle doc, word pageNum);

/*
 * Evict every cached page of a document without closing it.
 */
void PdfDiscardAllPages(PdfDocHandle doc);


/***********************************************************************
 *		D. Progress / cancellation
 ***********************************************************************
 *
 * Replaces the direct calls from gfx.goc into PDFUpdateProgress /
 * PDFPutUpImageProgress / PDFUpdateImageProgress /
 * PDFTakeDownImageProgress, and the single global stopLoadingImage
 * flag (which today is shared across all open documents -- a latent
 * bug with more than one document open at once). Callback and cancel
 * flag are now per-document.
 *
 * The original had two independent progress mechanisms: a page-decode
 * progress bar (ticks only, no start/end signal needed since the
 * caller already brackets the whole PdfDecodePage call -- see the
 * note on PdfDecodePage above) and a per-image progress DIALOG that
 * gfx.goc itself puts up and dismisses as it encounters each image
 * inside a page, independent of the caller. The `kind` argument below
 * is what makes one callback cover both: PDF_PROGRESS_PAGE is a plain
 * tick, the PDF_PROGRESS_IMAGE_* ones bracket one image's dialog
 * lifecycle the same way PDFPutUpImageProgress/PDFUpdateImageProgress/
 * PDFTakeDownImageProgress used to.
 */

#define PDF_PROGRESS_PAGE		0	/* page content-stream progress; periodic ticks, no begin/end */
#define PDF_PROGRESS_IMAGE_BEGIN	1	/* an image on the page is about to be decoded */
#define PDF_PROGRESS_IMAGE_UPDATE	2	/* that image's decode progress */
#define PDF_PROGRESS_IMAGE_END		3	/* that image is done (or was cancelled) */

/*
 * kind is one of the PDF_PROGRESS_* values above. For PDF_PROGRESS_PAGE,
 * current/total are the content-stream byte position/length. For the
 * IMAGE_* kinds, they're the scanline/total-scanlines; current and
 * total are both meaningless (0) for IMAGE_BEGIN.
 */
typedef void (*PdfProgressCallback)(dword kind, dword current, dword total, void *userData);

/*
 * Register a callback the engine invokes during PdfDecodePage --
 * periodically for page-decode progress, and around/during each
 * image on the page. userData is passed back unchanged; pass NULL
 * callback to unregister.
 */
void PdfSetProgressCallback(PdfDocHandle doc, PdfProgressCallback callback, void *userData);

/*
 * Request that an in-progress PdfDecodePage stop as soon as possible.
 * The in-progress call returns PDF_ERR_CANCELLED; already-decoded
 * pages are unaffected.
 */
void PdfCancelDecode(PdfDocHandle doc);


#endif /* _PDF_ENGINE_H */
