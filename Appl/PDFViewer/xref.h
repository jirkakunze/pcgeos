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
 * FILE:          xref.h
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
 *      Port of Derek Noonburg's "XRef" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef _XREF_H
#define _XREF_H

#ifdef __GNUC__
#pragma interface
#endif


//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------


extern 
void XRefInitNull(XRef *xref);


  // Constructor.  Read xref table from stream.
extern 
Boolean XRefInit(XRef *xref, FileHandle fileHan);

  // Destructor.
extern
void XRefFree(XRef *xref);

  // Is xref table valid?
extern
  GBool XRefIsOk(XRef *xref);

  // Is the file encrypted?
extern
  GBool XRefIsEncrypted(XRef *xref);


  // Are printing and copying allowed?  If not, print an error message.
extern
 GBool XRefOkToPrint(XRef *xref);

extern
 GBool XRefOkToCopy(XRef *xref);


  // Get catalog object.
extern
  void XRefGetCatalog(XRef *xref, Obj *obj);

  // Fetch an indirect reference.
extern
void XRefFetch(XRef *xref, long num, long gen, Obj *obj);

  // Return the document''s Info dictionary (if any).
extern
void XRefGetDocInfo(XRef *xref, Obj *obj);

GBool XRefReadXRef (XRef *xref, Stream *fs, long *pos, GBool isFirstSection);

GBool XRefCheckEncrypted(XRef *xref);


#endif  /* _XREF_H */
