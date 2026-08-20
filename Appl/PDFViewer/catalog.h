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
 * FILE:          catalog.h
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
 *
 ***********************************************************************/

#ifndef _CATALOG_H
#define _CATALOG_H

#ifdef __GNUC__
#pragma interface
#endif


/* Parse the document catalog dictionary and locate the page tree root. */
extern void CatalogInit(Catalog *cat, Obj *catDict, VMFileHandle vmFile, XRef *xref);

/* Zero-initialize a Catalog into an invalid, empty state. */
extern void CatalogInitNull(Catalog *cat);

/* Release all objects owned by a Catalog. */
extern void CatalogFree(Catalog *cat);

/* Is the catalog valid? */
extern GBool CatalogIsOk(Catalog *cat);

/* Get the number of pages in the document. */
extern long CatalogGetNumPages(Catalog *cat);

/* Materialize one page into caller-owned storage.  The caller must
 * call PageFree() exactly once if this returns gTrue. */
extern GBool CatalogGetPage(Catalog *cat, long i, XRef *xref, Page *page);


#ifdef KEEP_PAGE_REFS

/* Find a page by its object ID.  Returns the page number, or 0 if not found. */
extern long CatalogFindPage(Catalog *cat, long num, long gen, XRef *xref);

#endif


#endif  /* _CATALOG_H */
