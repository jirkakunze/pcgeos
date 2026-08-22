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

#ifndef PDFENGINE_H
#define PDFENGINE_H

#include <geos.h>
#include <graphics.h>
#include <heap.h>
#include <char.h>

/*
 * Ownership is unambiguous: whoever gets the handle from PdfOpen is
 * responsible for handing the document handle back to PdfClose exactly once.
 */

/* An open PDF document . */
typedef MemHandle PdfDocHandle;

#define PDF_NULL_DOC  ((PdfDocHandle)NullHandle)

/*
 * Error codes
 * ********************************************************************** Every
 * engine call that can fail returns one of these instead of the scattered.
 */
typedef enum {
    PDF_OK = 0,
        PDF_ERR_NOT_A_PDF,     /* xref/header didn't parse */
        PDF_ERR_BAD_XREF,      /* XRefIsOk() came back false */
        PDF_ERR_ENCRYPTED,     /* file has an /Encrypt entry */
        PDF_ERR_BAD_CATALOG,   /* CatalogIsOk() came back false */
        PDF_ERR_NO_SCRATCH_FILE, /* couldn't open the VM scratch file */
        PDF_ERR_OUT_OF_MEMORY,
        PDF_ERR_INVALID_PAGE,  /* page number out of range */
        PDF_ERR_CANCELLED,     /* decode aborted via PdfCancelDecode */
        PDF_ERR_INVALID_HANDLE
} PdfError;

/* A. */

/* Open a PDF document from an already-open GEOS file handle . */
PdfError PdfOpen(FileHandle file, PdfDocHandle *docOut);

/*
 * Close a document and free everything associated with it, including any pages
 * still decoded/cached .
 */
void PdfClose(PdfDocHandle doc);

/* Cheap validity check, e.g. */
Boolean PdfIsValid(PdfDocHandle doc);

/* B. */

/* Wraps: CatalogGetNumPages. */
word PdfGetPageCount(PdfDocHandle doc);

/* Page dimensions in points, post-rotation . */
PdfError PdfGetPageSize(PdfDocHandle doc, word pageNum, Point *sizeOut);

/*
 * No transient page handle is allocated; the document cache owns the decoded
 * GString and all page resources.
 */
PdfError PdfDecodePage(PdfDocHandle doc, word pageNum);

/* Draw page. */
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
    Boolean printing, Point *pageSizeOut);

/* Explicitly evict one decoded page from the document cache. */
void PdfDiscardPage(PdfDocHandle doc, word pageNum);

/* Evict every cached page of a document without closing it. */
void PdfDiscardAllPages(PdfDocHandle doc);

/* D. */

#define PDF_PROGRESS_PAGE       0
#define PDF_PROGRESS_IMAGE_BEGIN    1
#define PDF_PROGRESS_IMAGE_UPDATE   2   /* that image's decode progress */
#define PDF_PROGRESS_IMAGE_END      3

typedef void(*PdfProgressCallback)(dword kind, dword current, dword total,
    void *userData);

/* Set progress callback. */
void PdfSetProgressCallback(PdfDocHandle doc, PdfProgressCallback callback,
    void *userData);

/* Cancel decode. */
void PdfCancelDecode(PdfDocHandle doc);

#endif  /* PDFENGINE_H */

