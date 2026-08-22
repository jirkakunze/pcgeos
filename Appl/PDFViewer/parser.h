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
 * FILE:          parser.h
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
 *      Port of Derek Noonburg's "Parser" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifndef PARSER_H
#define PARSER_H

#ifdef __GNUC__
#pragma interface
#endif

/* Parser */

/* Constructor. */
extern void
ParserInit(Parser *parser, Lexer *lexer1);

/* Destructor. */
extern void
ParserFree(Parser *parser);

/* Get object. */
extern void
ParserGetObj(Parser *parser, Obj *obj);

/* Get position. */
extern long
ParserGetPos(Parser *parser);

/* Get length. */
extern long
ParserGetLength(Parser *parser);

/* Process parser shift. */
extern void
ParserShift(Parser *parser);

#endif  /* PARSER_H */

