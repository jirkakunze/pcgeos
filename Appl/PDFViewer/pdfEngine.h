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
        PDF_ERR_INVALID_HANDLE,
        PDF_ERR_PASSWORD       /* encrypted, password missing or wrong */
} PdfError;

/* A. */

/* Open a PDF document from an already-open GEOS file handle . */
/*
 * password: user or owner password of an encrypted file, NULL or "" for
 * none; bytes as stored in the PDF (Latin-1). PDF_ERR_PASSWORD means the
 * file can be opened, but needs (another) password.
 */
PdfError PdfOpen(FileHandle file, const char *password, PdfDocHandle *docOut);

/* buffer size for a password: PDF passwords have at most 32 bytes */
#define PDF_PASSWORD_BUF_SIZE 33

/*
 * Close a document and free everything associated with it, including any pages
 * still decoded/cached .
 */
void PdfClose(PdfDocHandle doc);

/* Cheap validity check, e.g. */
Boolean PdfIsValid(PdfDocHandle doc);

/*
 * Entry of the document information dictionary (/Info), e.g. "Title",
 * converted to the GEOS character set; dates ("CreationDate", "ModDate")
 * are formatted with the system's short date and time format. Returns
 * FALSE (and an empty buffer) if the entry is missing.
 */
Boolean PdfGetInfoField(PdfDocHandle doc, const char *key, char *buf,
                        word bufSize);

/* Permissions of an encrypted document (/P); TRUE for unencrypted ones. */
Boolean PdfOkToPrint(PdfDocHandle doc);
Boolean PdfOkToCopy(PdfDocHandle doc);

/*
 * Internal link at a click point (document coordinates of the page as
 * drawn on screen): target page (1-based) and target y in document
 * coordinates (-1 = top of page). FALSE if no link is hit.
 */
Boolean PdfFindLink(PdfDocHandle doc, word pageNum, sword docX, sword docY,
    word *destPage, sdword *destDocY);

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

/*
 * Draw page. printing = FALSE: screen (rotation, crop box clip).
 * printing = TRUE and fitArea = NULL: copy at 1:1 (clipboard).
 * printing = TRUE and fitArea set: fit into the printable area
 * (width, height in points), centered, turned by 90 degrees when the page
 * fits larger that way, clipped to the crop box.
 */
PdfError PdfDrawPage(PdfDocHandle doc, word pageNum, GStateHandle target,
    Boolean printing, const Point *fitArea, Point *pageSizeOut);

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

