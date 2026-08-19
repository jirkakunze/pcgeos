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
 * FILE:          lexer.h
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
 *      Port of Derek Noonburg's "Lexer" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef _LEXER_H
#define _LEXER_H

#ifdef __GNUC__
#pragma interface
#endif


//------------------------------------------------------------------------
// Lexer
//------------------------------------------------------------------------


  // Construct a lexer for a single stream.  Deletes the stream when
  // lexer is deleted.
extern void
  LexerInitFromStream(Lexer *lexer, Stream *str, XRef *xref);

  // Construct a lexer for a stream or array of streams (assumes obj
  // is either a stream or array of streams).
extern void
  LexerInitFromObj(Lexer *lexer, Obj *obj, XRef *xref);

  // Destructor.
extern void
  LexerFree(Lexer *lexer);

  // Get the next object from the input stream.
extern void
  LexerGetObj (Lexer *lexer, Obj *obj);

  // Skip to the beginning of the next line in the input stream.
extern void
  LexerSkipToNextLine (Lexer *lexer);

  // Get stream.
extern Stream *
  LexerGetStream(Lexer *lexer);

  // Get current position in file.
extern long
  LexerGetPos(Lexer *lexer);

  // Set position in file.
extern void
  LexerSetPos(Lexer *lexer, long pos);


extern long
  LexerGetChar(Lexer *lexer);

extern long
  LexerLookChar(Lexer *lexer);

extern XRef *
  LexerGetXRef(Lexer *lexer);


#endif  /* _LEXER_H */
