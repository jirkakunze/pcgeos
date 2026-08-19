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
 * FILE:          parser.c
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

#ifdef __GNUC__
#pragma implementation
#endif


#include "obj.h"
#include "array.h"
#include "dict.h"
#include "parser.h"
#include "lexer.h"
#include "stream.h"
#include "gmem.h"
#include "crypt.h"
#include <ec.h>

/*
 * Forward decls
 */
GBool ParserMakeStream (Parser *parser, Stream **str, Obj *dict);

/*
 * Code
 */

void ParserInit(Parser *parser, Lexer *lexer1) {
  parser->lexer = lexer1;
  parser->inlineImg = 0;
  /* safe default: no decryption. Only XRefFetch's classic path (the
   * one Parser that actually corresponds to one specific indirect
   * object) overrides this, right after ParserInit, once it knows
   * that object's num/gen -- see main/xref.goc and crypt.goh's notes
   * on why every other Parser in the codebase must NOT decrypt. */
  parser->noCrypt = gTrue;
  parser->cryptNum = parser->cryptGen = -1;
  LexerGetObj(parser->lexer, &parser->buf1);
  LexerGetObj(parser->lexer, &parser->buf2);
}

void ParserFree(Parser *parser) {

  LexerFree(parser->lexer);
  gfree(parser->lexer);
  ObjFree(&parser->buf1);
  ObjFree(&parser->buf2);
}

/*
 * Defensive recursion-depth cap (see project optimization analysis
 * S8): nested PDF arrays/dictionaries deeper than this aren't
 * something any legitimate PDF content stream or object would
 * contain (real documents rarely exceed single-digit nesting), but
 * a malformed or deliberately malicious one could otherwise drive
 * this recursive function to a real stack overflow on this
 * platform's limited stack.
 *
 * IMPORTANT: an earlier value of 100 here was NOT low enough --
 * confirmed by an actual crash (far call stack overflow) on real
 * hardware with a 200-level-deep test array, well before reaching
 * 100 nested calls. That was a guess, not a measurement, exactly
 * the mistake the project's own S5 analysis warned against for a
 * similarly-sized DCT stack array ("don't just pick a value, measure
 * the real high-water mark first"). Lowered substantially as a
 * result. No hardware stack measurement is available to derive a
 * precise safe number, so this is deliberately conservative rather
 * than "as high as possible" -- legitimate PDFs essentially never
 * nest anywhere close to this deep, so a low limit costs nothing in
 * practice while giving a much larger safety margin against however
 * many bytes each far-called recursive frame actually costs here.
 */
#define parserGetObjMaxDepth 20

/***********************************************************************
 *		ParserSkipValue
 ***********************************************************************
 * SYNOPSIS:	    Consume exactly one PDF value from the token
 *		    stream, however deeply nested it is, WITHOUT
 *		    recursing and without building any Obj structure
 *		    for it -- just tracking bracket depth with a plain
 *		    counter and shifting through tokens iteratively.
 *		    Used when ParserGetObj's own recursion depth limit
 *		    is hit (see project optimization analysis S8): a
 *		    single ParserShift() only consumes one token, which
 *		    leaves the rest of a deeply-nested array/dict's
 *		    tokens (including a pile of unmatched closing
 *		    brackets) sitting in the stream for whatever reads
 *		    next to trip over -- confirmed on real hardware as
 *		    spurious "dictionary key must be a name" warnings
 *		    once the leftover closing brackets reached back up
 *		    to an enclosing dictionary's key/value loop. This
 *		    properly balances brackets instead, so the token
 *		    stream is correctly aligned for whatever comes
 *		    after regardless of how deep the skipped value was
 *		    nested.
 * PARAMETERS:	    Parser *parser -- pbuf1 is the value's first token
 * SIDE EFFECTS:    consumes exactly the tokens belonging to one value
 ***********************************************************************/
static void
ParserSkipValue(Parser *parser)
{
    Obj *pbuf1 = &parser->buf1;
    short bracketDepth;

    if (isCmdSame(pbuf1, "[") || isCmdSame(pbuf1, "<<")) {
	bracketDepth = 1;
	ParserShift(parser);
	while (bracketDepth > 0 && !isEOF(pbuf1)) {
	    if (isCmdSame(pbuf1, "[") || isCmdSame(pbuf1, "<<")) {
		++bracketDepth;
	    } else if (isCmdSame(pbuf1, "]") || isCmdSame(pbuf1, ">>")) {
		--bracketDepth;
	    }
	    ParserShift(parser);
	}
    } else {
	ParserShift(parser);
    }
}

void
ParserGetObj (Parser *parser, Obj *obj)
{
/* Object *Parser::getObj(Object *obj) {
*/
  static short depth = 0;
  static GBool alreadyWarned = gFalse;
  char *key;
  Stream *str = NULL;
  Obj obj2;
  long num;
  Obj *pbuf1 = &parser->buf1;
  Obj *pbuf2 = &parser->buf2;

  if (++depth > parserGetObjMaxDepth) {
    /*
     * Only the FIRST time the limit is hit within a given top-level
     * parse gets an actual EC_WARNING -- see ParserSkipValue's
     * comment for why a naive one-shift-per-call approach doesn't
     * work; under an attached debugger in an EC build, each
     * EC_WARNING call stops execution and waits for a manual
     * continue, so many repeated warnings looks exactly like a
     * freeze even though nothing is actually stuck. alreadyWarned
     * resets back to gFalse once depth returns to 0 (see both
     * decrement sites below), so a later, separate deeply-nested
     * value still gets its own single warning.
     */
    if (!alreadyWarned) {
      EC_WARNING(-1);
      alreadyWarned = gTrue;
    }
    --depth;
    if (depth == 0) {
      alreadyWarned = gFalse;
    }
    /*
     * Skip the ENTIRE remaining nested value (properly bracket-
     * matched, see ParserSkipValue), not just one token -- a single
     * ParserShift here left the token stream misaligned by however
     * many closing brackets remained unconsumed, corrupting
     * whatever came next (confirmed on real hardware).
     */
    ParserSkipValue(parser);
    initError(obj);
    return;
  }

  // refill buffer after inline image data
  if (parser->inlineImg == 2) {
    ObjFree(pbuf1);
    ObjFree(pbuf2);
    LexerGetObj(parser->lexer, pbuf1);
    LexerGetObj(parser->lexer, pbuf2);
    parser->inlineImg = 0;
  }

  // array
  if (isCmdSame(pbuf1, "[")) {
    ParserShift(parser);
    initArray(obj);
    if (GMemHadError()) {
      while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1))
        ParserSkipValue(parser);
      if (isCmdSame(pbuf1, "]"))
        ParserShift(parser);
      goto parserGetObjDone;
    }
    while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1)) {
      ParserGetObj(parser, &obj2);
      ObjArrayAdd(obj, &obj2);
      ObjFree(&obj2);
      if (GMemHadError()) {
        while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1))
          ParserSkipValue(parser);
        break;
      }
    }
    if (isEOF(pbuf1))
      EC_WARNING(-1);
//      error(getPos(), "End of file inside array");
    ParserShift(parser);

  // dictionary or stream
  } else if (isCmdSame(pbuf1, "<<")) {
    ParserShift(parser);
    initDict(obj);
    if (GMemHadError()) {
      while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
        ParserSkipValue(parser);
      if (isCmdSame(pbuf1, ">>"))
        ParserShift(parser);
      goto parserGetObjDone;
    }
    while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1)) {
      if (!isName(pbuf1)) {
	EC_WARNING(-1);
//	error(getPos(), "Dictionary key must be a name object");
	ParserShift(parser);
      } else {
	key = copyString(getName(pbuf1));
	if (!key) {
	  ParserShift(parser);
	  while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
	    ParserSkipValue(parser);
	  break;
	}
	ParserShift(parser);
	if (isEOF(pbuf1) || isError(pbuf1))
	  break;
	ParserGetObj(parser, &obj2);
	DictAdd(obj->u.dict, key, &obj2);
	ObjFree(&obj2);
	if (GMemHadError()) {
	  while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
	    ParserSkipValue(parser);
	  break;
	}
      }
    }
    if (isEOF(pbuf1))
	EC_WARNING(-1);
//      error(getPos(), "End of file inside dictionary");
    if (isCmdSame(pbuf2, "stream")) {
	if (ParserMakeStream(parser, &str, obj)) {
	    ObjFree(obj);
	    initStream(obj, str);
	} else {
	    ObjFree(obj);
	    initError(obj);
	}
    } else {
	ParserShift(parser);
    }

  // indirect reference or integer
  } else if (isInt(pbuf1)) {
    num = getInt(pbuf1);
    ParserShift(parser);
    if (isInt(pbuf1) && isCmdSame(pbuf2, "R")) {
      initRef(obj, num, getInt(pbuf1));
      ParserShift(parser);
      ParserShift(parser);
    } else {
      initInt(obj, num);
    }

  // simple object
  } else {
    ObjMove(obj, pbuf1);
    ParserShift(parser);
  }
parserGetObjDone:
  --depth;
  if (depth == 0) {
    alreadyWarned = gFalse;
  }
}


GBool
ParserMakeStream (Parser *parser, Stream **str, Obj *dict)
{
//Stream *Parser::makeStream(Object *dict) {
  Obj obj, obj2;
  long pos, length;


  // get stream start position
  LexerSkipToNextLine(parser->lexer);
  pos = LexerGetPos(parser->lexer);

  // get length
  ObjDictLookup(dict, "Length", &obj, LexerGetXRef(parser->lexer));
  if (isInt(&obj)) {
    length = getInt(&obj);
    ObjFree(&obj);

  } else if (isRef(&obj)) {
      ObjFetch(&obj2, &obj, LexerGetXRef(parser->lexer));
      if (isInt(&obj2)) {
	  length = getInt(&obj2);
	  ObjFree(&obj);
	  ObjFree(&obj2);
      } else {
	  EC_WARNING(-1); /* bad length attr */
	  ObjFree(&obj);
	  ObjFree(&obj2);
	  return gFalse;
      }
  } else {
    EC_WARNING(-1);
//    error(getPos(), "Bad 'Length' attribute in stream");
    ObjFree(&obj);
    return gFalse;
  }

  // make base stream
//  str = new FileStream(lexer->getStream()->getFile(), pos, length, dict);

  *str = gmalloc( sizeof(Stream) );
  if (!*str)
    return gFalse;
  FStreamInit(*str,
	      StreamGetFile(LexerGetStream(parser->lexer)),
	      pos,
	      length,
	      dict);

  /* XXX: needs to not access stream data directly */
//  str->fHan = StreamGetFile(LexerGetStream(parser->lexer));
//  str->pos = pos;
//  str->length = length;
//  ObjCopy(&str->oDict, dict);

  /*
   * Decrypt (RC4), if this object is one XRefFetch flagged as such
   * via parser->cryptNum/cryptGen -- has to happen here, on the raw
   * bytes straight out of FStreamInit, strictly BEFORE StreamAddFilters
   * hands them to Flate/DCT/whatever /Filter says: those filters
   * expect their own well-formed compressed data, not RC4-scrambled
   * bytes standing in for it. See crypt.goh/main/crypt.goc.
   */
  if (!parser->noCrypt) {
    XRef *xref = LexerGetXRef(parser->lexer);
    if (xref && xref->encrypted) {
      Guchar objKey[16];
      short objKeyLen;
      Stream *rc4Str;

      XRefDeriveObjectKey(xref, parser->cryptNum, parser->cryptGen,
			   objKey, &objKeyLen);
      rc4Str = gmalloc( sizeof(Stream) );
      if (!rc4Str) {
        StreamFree(*str);
        gfree(*str);
        *str = NULL;
        return gFalse;
      }
      RC4StreamInit(rc4Str, *str, objKey, objKeyLen);
      *str = rc4Str;
    }
  }

  // get filters
  *str = StreamAddFilters(*str, dict, LexerGetXRef(parser->lexer));

  // skip over stream data
  LexerSetPos(parser->lexer, pos + length);

  // refill token buffers and check for 'endstream'
  ParserShift(parser);  // kill '>>'
  ParserShift(parser);  // kill 'stream'
  if (isCmdSame(&parser->buf1, "endstream"))
    ParserShift(parser);
  else
    EC_WARNING(-1);
//    error(getPos(), "Missing 'endstream'");

  return gTrue;

}	/* End of ParserMakeStream.	*/

void
ParserShift (Parser *parser)
{
/* void Parser::shift() {
*/
  if (parser->inlineImg > 0) {
    ++parser->inlineImg;
  } else if (isCmdSame(&parser->buf2, "ID")) {
    LexerGetChar(parser->lexer);	// skip char after 'ID' command
    parser->inlineImg = 1;
  }
  ObjFree(&parser->buf1);
  ObjMove(&parser->buf1, &parser->buf2);
  if (parser->inlineImg > 0)		// don't buffer inline image data'
    initNull(&parser->buf2);
  else
    LexerGetObj(parser->lexer, &parser->buf2);

}	/* End of ParserShift.	*/

long ParserGetPos (Parser *parser) {

    return StreamGetPos(LexerGetStream(parser->lexer));
}

long ParserGetLength (Parser *parser)
{
  return StreamGetLength(LexerGetStream(parser->lexer));
}
