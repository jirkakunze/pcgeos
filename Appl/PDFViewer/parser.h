/***********************************************************************
 *
 * parser.h
 *
 * Copyright 1996 Derek B. Noonburg
 * Modifications Copyright 2026 Jirka Kunze/FreeGEOS Project
 *
 * This file is derived from the original Xpdf source code and has been
 * modified for use in the PC/GEOS PDF Viewer.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***********************************************************************/

#ifndef _PARSER_H
#define _PARSER_H

#ifdef __GNUC__
#pragma interface
#endif


//------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------


  // Constructor.
extern void
  ParserInit(Parser *parser, Lexer *lexer1);

  // Destructor.
extern void
  ParserFree(Parser *parser);

  // Get the next object from the input stream.
extern void
  ParserGetObj(Parser *parser, Obj *obj);

  // Get current position in file.
extern long
  ParserGetPos (Parser *parser);

extern long
  ParserGetLength (Parser *parser);

extern void
  ParserShift (Parser *parser);


#endif  /* _PARSER_H */
