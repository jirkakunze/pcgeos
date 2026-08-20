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
 * FILE:          page.h
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
 *      Port of Derek Noonburg's "Page" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef _PAGE_H
#define _PAGE_H

#ifdef __GNUC__
#pragma interface
#endif


//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------


  // Construct a new PageAttrs object by merging a dictionary
  // (of type Pages or Page) into another PageAttrs object.  If
  // <attrs> is NULL, uses defaults.
extern void
  PageAttrsInit(PageAttrs *this, PageAttrs *attrs, Dict *dict, XRef *xref);

  // Destructor.
extern
void PageAttrsFree(PageAttrs *this);

  // Accessors.
extern
  gdouble PageAttrsGetX1(PageAttrs *this);
extern
  gdouble PageAttrsGetY1(PageAttrs *this);
extern
  gdouble PageAttrsGetX2(PageAttrs *this);
extern
  gdouble PageAttrsGetY2(PageAttrs *this);
extern
  GBool PageAttrsIsCropped(PageAttrs *this);
extern
  gdouble PageAttrsGetCropX1(PageAttrs *this);
extern
  gdouble PageAttrsGetCropY1(PageAttrs *this);
extern
  gdouble PageAttrsGetCropX2(PageAttrs *this);
extern
  gdouble PageAttrsGetCropY2(PageAttrs *this);
extern
  long PageAttrsGetRotate(PageAttrs *this);


//extern
//Dict *PageAttrsGetResourceDict(PageAttrs *this);
extern
void PageAttrsCopyResourceDict(PageAttrs *this, Obj *dest, XRef *xref);


//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------


  // Constructor.
extern
void PageInit(Page *page, long num1, Dict *pageDict, PageAttrs *attrs1);

  // Destructor.
extern
void PageFree(Page *page);

  // Is page valid?
extern
  GBool PageIsOk(Page *page);

  // Get page parameters.
extern
  gdouble PageGetX1(Page *page);
extern
  gdouble PageGetY1(Page *page);
extern
  GBool  PageIsCropped(Page *page);
extern
  gdouble PageGetCropX1(Page *page);
extern
  gdouble PageGetCropY1(Page *page);
extern
  gdouble PageGetCropX2(Page *page);
extern
  gdouble PageGetCropY2(Page *page);
extern
  gdouble PageGetWidth(Page *page);
extern
  gdouble PageGetHeight(Page *page);
extern
  long PageGetRotate(Page *page);


  // Get resource
//extern
//  Dict *PageGetResourceDict(Page *page);
extern
void PageCopyResourceDict(Page *page, Obj *dest, XRef *xref);

  // Get annotations array.
extern
  void PageGetAnnots(Page *page, Obj *obj, XRef *xref);

  // Get contents.
extern
  void PageGetContents(Page *page, Obj *obj, XRef *xref);


#endif  /* _PAGE_H */
