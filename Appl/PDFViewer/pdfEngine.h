/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	Copyright (c) GlobalPC 1999.  All rights reserved.
	GLOBALPC CONFIDENTIAL

PROJECT:	GEOS
MODULE:		PDF Viewer
FILE:		pdfEngine.h

DESCRIPTION:

	Public interface to the PDF engine.

	This is the ONLY header the UI layer (ui/pdfvu*.goc) should need
	to include in order to open, inspect, and render PDF documents.
	Everything below the line -- XRef, Catalog, Obj, Dict, Gfx, Stream,
	and the filter/parser layer -- is an implementation detail of the
	engine and is not exposed here. UI code should not need to
	@include catalog.goh, xref.goh, obj.goh, page.goh, or gfx.goh
	directly any more; if it finds itself needing to, that is a sign
	something belongs in this interface instead.

	This is a plain ANSI C header (no @class/@message/@instance).
	It contains no GEOS Object System declarations, so it is meant to
	be brought in with a plain

		#include "pdfEngine.h"

	from both .goc and .c files, the same way obj.goh/catalog.goh/
	array.goh already are today despite their .goh extension.

	DEPENDENCIES / INCLUDE ORDER:
	Like the existing engine headers (catalog.goh, xref.goh, obj.goh,
	page.goh), this file does NOT pull in the GEOS SDK base headers
	(stdapp.goh and friends) itself -- it assumes those are already
	visible from whatever included it first (in practice: pdfvu.goh
	at the top of every .goc file). It needs Boolean, word, dword,
	Point, FileHandle, GStateHandle, and MemHandle/NullHandle from
	that chain.

	Note this header deliberately uses the native GEOS Boolean
	(TRUE/FALSE), not GBool (gTrue/gFalse). GBool is xpdf's own
	boolean type and stays confined to the engine's internals
	(XRef/Catalog/Obj); nothing from the xpdf side of the engine
	should leak across this boundary. That also means this header
	needs no include of gtypes.goh.

	STATUS: interface sketch only. No implementation exists yet;
	main/pdfEngine.goc (wrapping the current XRef/Catalog/Gfx calls,
	and converting GBool -> Boolean at the boundary) is the next step
	once this signature set is agreed on.

	$Id$

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef PDF_ENGINE_H
#define PDF_ENGINE_H


/***********************************************************************
 *		Opaque handles
 ***********************************************************************
 *
 * Backed by GEOS MemHandle rather than a plain struct pointer. This is
 * the idiomatic GEOS choice, not just a style preference: blocks under
 * a MemHandle can be swapped/moved by the compacting heap, so nothing
 * outside the engine may hold a raw pointer to the document/page data
 * across calls. The UI only ever holds the handle value; the engine
 * locks it (MemLock) for the duration of each call and unlocks it
 * before returning. The struct definitions behind these handles
 * (PdfDocInternal, PdfPageInternal) are private to main/pdfEngine.goc
 * and are not declared here.
 *
 * Ownership is unambiguous: whoever gets the handle from
 * PdfOpen/PdfDecodePage is responsible for handing it back to
 * PdfClose/PdfDiscardPage exactly once.
 */

/* An open PDF document (today: PDFDoc_xref + PDFDoc_catalog + the
 * scratch VM file that holds decoded pages). */
typedef MemHandle PdfDocHandle;

/* One decoded page (today: one page's cached GString). Returned by
 * PdfDecodePage, consumed by PdfDrawPage. */
typedef MemHandle PdfPageHandle;

#define PDF_NULL_DOC  ((PdfDocHandle)NullHandle)
#define PDF_NULL_PAGE ((PdfPageHandle)NullHandle)


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
    PDF_ERR_BAD_CATALOG,	/* CatalogIsOk() came back false */
    PDF_ERR_NO_SCRATCH_FILE,	/* couldn't open the VM scratch file */
    PDF_ERR_OUT_OF_MEMORY,
    PDF_ERR_INVALID_PAGE,	/* page number out of range */
    PDF_ERR_CANCELLED,		/* decode aborted via PdfCancelDecode */
    PDF_ERR_INVALID_HANDLE	/* PDF_NULL_DOC/PDF_NULL_PAGE or stale handle passed in */
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
 * Page dimensions in points. Today the UI hardcodes DOCUMENT_WIDTH/
 * DOCUMENT_HEIGHT (8.5x11) for every page; this replaces that with a
 * real per-page MediaBox lookup once implemented.
 */
PdfError PdfGetPageSize(PdfDocHandle doc, word pageNum, Point *sizeOut);


/***********************************************************************
 *		C. Rendering (two-phase: decode once, draw many times)
 ***********************************************************************
 *
 * This mirrors the existing PDFGetPageGString / GrCopyGString split,
 * it is not a new design: decoding a page's content stream into a
 * GString is the expensive part and is cached; drawing is just a
 * cheap GString replay onto any target GState. PdfDrawPage is
 * therefore the single primitive behind screen redraw, printing, AND
 * clipboard copy, which today are three separate call sites.
 */

/*
 * Decode a page's content stream if not already cached, and return a
 * handle to it. The engine owns the cache and its eviction policy
 * internally (today: a fixed limit of 30 cached pages, see
 * PDFDOC_PAGE_COUNT_LIMIT) -- the UI does not manage this any more.
 *
 * Wraps: CatalogLockPage/CatalogUnlockPage, PageGetContents, ObjFree,
 *        GrCreateGString, GfxGo, and the cache-eviction loop currently
 *        inlined in PDFGetPageGString.
 */
PdfError PdfDecodePage(PdfDocHandle doc, word pageNum, PdfPageHandle *pageOut);

/*
 * Replay a decoded page onto any target GState (screen, printer,
 * clipboard transfer GString -- caller decides).
 *
 * Wraps: PDFSetupGState + GrCopyGString.
 */
PdfError PdfDrawPage(PdfPageHandle page, GStateHandle target);

/*
 * Explicitly evict one decoded page from the cache. Rarely needed
 * directly (PdfDecodePage evicts automatically under pressure); mainly
 * useful if the UI wants to free memory proactively, e.g. on
 * MSG_GEN_DOCUMENT_CONTROL_FILE_CHANGED.
 */
void PdfDiscardPage(PdfPageHandle page);

/*
 * Evict every cached page of a document without closing it.
 */
void PdfDiscardAllPages(PdfDocHandle doc);


/***********************************************************************
 *		D. Progress / cancellation
 ***********************************************************************
 *
 * Replaces the direct calls from gfx.goc into PDFUpdateProgress /
 * PDFUpdateImageProgress, and the single global stopLoadingImage flag
 * (which today is shared across all open documents -- a latent bug
 * with more than one document open at once). Callbacks and the cancel
 * flag are now per-document.
 */

typedef void (*PdfProgressCallback)(dword current, dword total, void *userData);

/*
 * Register a callback the engine invokes periodically during
 * PdfDecodePage (page-decode progress) and during image decoding
 * within a page. userData is passed back unchanged; pass NULL callback
 * to unregister.
 */
void PdfSetProgressCallback(PdfDocHandle doc, PdfProgressCallback callback, void *userData);

/*
 * Request that an in-progress PdfDecodePage stop as soon as possible.
 * The in-progress call returns PDF_ERR_CANCELLED; already-decoded
 * pages are unaffected.
 */
void PdfCancelDecode(PdfDocHandle doc);


#endif /* PDF_ENGINE_H */
