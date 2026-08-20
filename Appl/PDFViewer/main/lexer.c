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
 * FILE:          lexer.c
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

#ifdef __GNUC__
#pragma implementation
#endif


#include "pdfGeode.h"
#include <char.h>
#include <ctype.h>
#include <ec.h>
#include <Ansi/string.h>
#include <Ansi/stdio.h>
#include "obj.h"
#include "array.h"
#include "stream.h"
#include "lexer.h"
#include "gmem.h"
#include "gstr.h"

/* A '1' in this array means the corresponding character ends a name or command. */
static char endOfNameChars[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,   /* 0x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 1x */
    1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1,   /* 2x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,   /* 3x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 4x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,   /* 5x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 6x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0    /* 7x */
};

#define tokBufSize 128  /* size of token buffer */


/***********************************************************************
 *    Lexer
 ***********************************************************************/

/***********************************************************************
 *      LexerInitFromStream
 ***********************************************************************
 * SYNOPSIS:        construct a lexer for a single stream
 * PARAMETERS:      Lexer *lexer  lexer to initialize
 *                  Stream *str   stream to lex; owned by lexer afterward
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called to lex a single content stream (e.g. a page's content
 *      or a Type 2 function's data), as opposed to the multi-stream
 *      case used for indirect content-stream arrays.
 *
 * STRATEGY:
 *      Wrap str in a single-element Array so it can be driven through
 *      the same multi-stream traversal as LexerInitFromObj(), then
 *      reset the stream to its start.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerInitFromStream(Lexer *lexer, Stream *str, XRef *xref)
{
    lexer->xref = xref;

    initStream(&lexer->curStr, str);
    lexer->streams = gmalloc(sizeof(Array));
    if (!lexer->streams) {
        lexer->curStr.type = objNone;
        lexer->strPtr = 0;
        lexer->freeArray = gFalse;
        return;
    }
    ArrayInit(lexer->streams);
    ArrayAdd(lexer->streams, &lexer->curStr);
    lexer->strPtr = 0;
    lexer->freeArray = gTrue;
    ObjStreamReset(&lexer->curStr);
}

/***********************************************************************
 *      LexerInitFromObj
 ***********************************************************************
 * SYNOPSIS:        construct a lexer for a stream or array of streams
 * PARAMETERS:      Lexer *lexer  lexer to initialize
 *                  Obj *obj      a stream, or an array of streams
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called for content that may span multiple indirect streams
 *      concatenated end to end (PDF allows a page's /Contents to be
 *      an array of stream references).
 *
 * STRATEGY:
 *      A single stream is wrapped in its own owned Array, matching
 *      LexerInitFromStream(); an existing array is used directly and
 *      not owned. Position at the first stream, if any, and reset it.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerInitFromObj(Lexer *lexer, Obj *obj, XRef *xref)
{
    lexer->xref = xref;
    lexer->curStr.type = objNone;

    if (isStream(obj)) {
        lexer->streams = gmalloc(sizeof(Array));
        if (!lexer->streams) {
            lexer->curStr.type = objNone;
            lexer->strPtr = 0;
            lexer->freeArray = gFalse;
            return;
        }
        ArrayInit(lexer->streams);
        lexer->freeArray = gTrue;
        ArrayAdd(lexer->streams, obj);
    } else {
        lexer->streams = getArray(obj);
        lexer->freeArray = gFalse;
    }
    lexer->strPtr = 0;
    if (ArrayGetLength(lexer->streams) > 0) {
        ArrayGet(lexer->streams, lexer->strPtr, &lexer->curStr, xref);
        ObjStreamReset(&lexer->curStr);
    }
}

/***********************************************************************
 *      LexerFree
 ***********************************************************************
 * SYNOPSIS:        release storage owned by a lexer
 * PARAMETERS:      Lexer *lexer  lexer to free
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called when parsing of a stream or content array is complete.
 *
 * STRATEGY:
 *      Free the current stream object, then the backing stream array
 *      if this lexer owns it (see freeArray in LexerInitFromObj()).
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerFree(Lexer *lexer)
{
    if (!isNone(&lexer->curStr)) {
        ObjFree(&lexer->curStr);
    }
    if (lexer->freeArray && lexer->streams) {
        ArrayFree(lexer->streams);
        gfree(lexer->streams);
    }
}

/***********************************************************************
 *      LexerGetChar
 ***********************************************************************
 * SYNOPSIS:        read and consume the next raw character
 * PARAMETERS:      Lexer *lexer  lexer to read from
 *
 * RETURNS:         long  the character, or EOF once all streams are
 *                        exhausted
 *
 * CONTEXT:
 *      Called by LexerGetObj() and LexerSkipToNextLine() for every
 *      character consumed.
 *
 * STRATEGY:
 *      Read from the current stream; on EOF, advance to the next
 *      stream in the array (if any) and retry, so multi-stream
 *      content reads as one continuous character sequence.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
long LexerGetChar(Lexer *lexer)
{
    long c;

    c = EOF;
    while (!isNone(&lexer->curStr) && (c = ObjStreamGetChar(&lexer->curStr)) == EOF) {
        ObjFree(&lexer->curStr);
        ++lexer->strPtr;
        if (lexer->strPtr < ArrayGetLength(lexer->streams)) {
            ArrayGet(lexer->streams, lexer->strPtr, &lexer->curStr, lexer->xref);
            ObjStreamReset(&lexer->curStr);
        }
    }
    return c;
}

/***********************************************************************
 *      LexerLookChar
 ***********************************************************************
 * SYNOPSIS:        peek at the next raw character without consuming it
 * PARAMETERS:      Lexer *lexer  lexer to read from
 *
 * RETURNS:         long  the character, or EOF once all streams are
 *                        exhausted
 *
 * CONTEXT:
 *      Called throughout LexerGetObj() wherever the tokenizer needs
 *      one character of lookahead before deciding to consume it.
 *
 * STRATEGY:
 *      Same stream-boundary-crossing logic as LexerGetChar(), but
 *      via ObjStreamLookChar() so the character isn't consumed.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
long LexerLookChar(Lexer *lexer)
{
    long c;

    c = EOF;
    while (!isNone(&lexer->curStr) && (c = ObjStreamLookChar(&lexer->curStr)) == EOF) {
        ObjFree(&lexer->curStr);
        ++lexer->strPtr;
        if (lexer->strPtr < ArrayGetLength(lexer->streams)) {
            ArrayGet(lexer->streams, lexer->strPtr, &lexer->curStr, lexer->xref);
            ObjStreamReset(&lexer->curStr);
        }
    }
    return c;
}

/***********************************************************************
 *      LexerGetObj
 ***********************************************************************
 * SYNOPSIS:        get the next PDF object (token) from the input
 * PARAMETERS:      Lexer *lexer  lexer to read from
 *                  Obj *obj      out: the parsed token
 *
 * RETURNS:         void (obj is set to objEOF at end of input, or
 *                        objError on a malformed token)
 *
 * CONTEXT:
 *      Called by the Parser for every token in a content stream or
 *      object definition: numbers, strings, names, arrays/dict
 *      punctuation, hex strings, and commands/keywords.
 *
 * STRATEGY:
 *      Skip whitespace/comments, then dispatch on the first non-blank
 *      character into the appropriate token reader. Numbers accumulate
 *      unsigned to tolerate out-of-range two's-complement encodings;
 *      real numbers use a manual fixed-point long-division routine
 *      when USE_NATIVE_FLOAT_TYPE is set, since plain (tmp << 16)
 *      overflows for larger fractional parts.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerGetObj(Lexer *lexer, Obj *obj)
{
    char *p;
    long c, c2;
    GBool comment, neg, done;
    long numParen;
    long xi;
    dword uxi;	/* unsigned accumulator for the digit-reading loop
		 * below -- some PDF generators represent a negative
		 * integer (e.g. /P in an encryption dict) via its
		 * 32-bit unsigned two's-complement bit pattern
		 * instead of a leading '-' (seen in the wild:
		 * "4294967292" for what should read as -4). Accumulating
		 * straight into a signed long overflows partway through
		 * such a value (undefined behavior in C, and in practice
		 * silently wrong on this compiler); accumulating
		 * unsigned first and reinterpreting via cast at the end
		 * handles both ordinary in-range integers and this
		 * convention correctly, since x86 long is two's
		 * complement. */
#ifdef USE_NATIVE_FLOAT_TYPE
    sdword xf;
#else
    double xf;
#endif
    long scale;
    GooString s;
    long n, m;
    long tmp;
    char tokBuf[tokBufSize];	/* temporary token buffer */

    /* skip whitespace and comments */
    comment = gFalse;
    while (1) {
        if ((c = LexerGetChar(lexer)) == EOF) {
            initEOF(obj);
            return;
        }

        if (comment) {
            if (c == '\r' || c == '\n') {
                comment = gFalse;
            }
        } else if (c == '%') {
            comment = gTrue;
        } else if (!isspace(c)) {
            break;
        }
    }

    /* start reading token */
    switch (c) {

    /* number */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '-': case '.':
        neg = gFalse;
        uxi = 0;
        if (c == '-') {
            neg = gTrue;
        } else if (c == '.') {
            xi = 0;
            goto doReal;
        } else {
            uxi = c - '0';
        }
        while (1) {
            c = LexerLookChar(lexer);
            if (isdigit(c)) {
                LexerGetChar(lexer);
                uxi = uxi * 10 + (c - '0');
            } else if (c == '.') {
                LexerGetChar(lexer);
                xi = (long) uxi;
                goto doReal;
            } else {
                break;
            }
        }
        xi = (long) uxi;
        if (neg) {
            xi = -xi;
        }
        initInt(obj, xi);
        break;
    doReal:
#ifdef USE_NATIVE_FLOAT_TYPE
        xf = xi << 16;
#else
        xf = xi;
#endif
        scale = 1;
        tmp = 0;
        while (1) {
            c = LexerLookChar(lexer);
            if (!isdigit(c)) {
                break;
            }
            LexerGetChar(lexer);
            tmp = tmp * 10 + (c - '0');
            scale *= 10;
        }
#ifdef USE_NATIVE_FLOAT_TYPE
        /* (tmp << 16) / scale, computed one bit at a time instead of
         * directly -- tmp<<16 alone overflows a 32-bit signed value
         * whenever tmp (the fractional digits taken as a plain integer,
         * e.g. 82745 for ".82745") is 32768 or higher, corrupting the
         * result before the division even runs (confirmed: this exact
         * case silently turned a bright yellow PDF fill color into a
         * visibly wrong red). The running remainder here never exceeds
         * 2*scale, comfortably within range for any realistic fractional
         * digit count -- same idea as a textbook binary long division. */
        {
            long fracRemainder, fracResult, fracBit;
            fracRemainder = tmp;
            fracResult = 0;
            for (fracBit = 0; fracBit < 16; ++fracBit) {
                fracRemainder = fracRemainder << 1;
                fracResult = fracResult << 1;
                if (fracRemainder >= scale) {
                    fracRemainder -= scale;
                    fracResult += 1;
                }
            }
            xf += fracResult;
        }
#else
        xf += ((double)tmp / scale);
#endif
        if (neg) {
            xf = -xf;
        }
        initReal(obj, xf);
        break;

    /* string */
    case '(':
        p = tokBuf;
        n = 0;
        numParen = 1;
        done = gFalse;
        GStrInit(&s);
        if (GMemHadError()) {
            initError(obj);
            break;
        }
        do {
            c2 = EOF;
            switch (c = LexerGetChar(lexer)) {

            case EOF:
            case '\r':
            case '\n':
                EC_WARNING(-1);
                done = gTrue;
                break;

            case '(':
                ++numParen;
                break;

            case ')':
                if (--numParen == 0) {
                    done = gTrue;
                }
                break;

            case '\\':
                c = LexerGetChar(lexer);
                switch (c) {
                case 'n':
                    c2 = '\n';
                    break;
                case 'r':
                    c2 = '\r';
                    break;
                case 't':
                    c2 = '\t';
                    break;
                case 'b':
                    c2 = '\b';
                    break;
                case 'f':
                    c2 = '\f';
                    break;
                case '\\':
                case '(':
                case ')':
                    c2 = c;
                    break;
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7':
                    c2 = c - '0';
                    c = LexerLookChar(lexer);
                    if (c >= '0' && c <= '7') {
                        LexerGetChar(lexer);
                        c2 = (c2 << 3) + (c - '0');
                        c = LexerLookChar(lexer);
                        if (c >= '0' && c <= '7') {
                            LexerGetChar(lexer);
                            c2 = (c2 << 3) + (c - '0');
                        }
                    }
                    break;
                case '\r':
                    c = LexerLookChar(lexer);
                    if (c == '\n') {
                        LexerGetChar(lexer);
                    }
                    break;
                case '\n':
                    break;
                case EOF:
                    EC_WARNING(-1);
                    done = gTrue;
                    break;
                default:
                    c2 = c;
                    break;
                }
                break;

            default:
                c2 = c;
                break;
            }

            if (c2 != EOF) {
                if (n == tokBufSize) {
                    GStrAppendStringN(&s, tokBuf, tokBufSize);
                    p = tokBuf;
                    n = 0;
                }
                *p++ = (char)c2;
                ++n;
            }
        } while (!done);
        GStrAppendStringN(&s, tokBuf, n);
        if (GMemHadError()) {
            GStrFree(&s);
            initError(obj);
            break;
        }
        initString(obj, &s);
        GStrFree(&s);
        break;

    /* name */
    case '/':
        p = tokBuf;
        n = 0;
        while ((c = LexerLookChar(lexer)) != EOF && !(c < 128 && endOfNameChars[c])) {
            LexerGetChar(lexer);
            if (c == C_NUMBER_SIGN) {
                c2 = LexerLookChar(lexer);
                if (c2 >= '0' && c2 <= '9') {
                    c = c2 - '0';
                } else if (c2 >= 'A' && c2 <= 'F') {
                    c = c2 - 'A' + 10;
                } else if (c2 >= 'a' && c2 <= 'f') {
                    c = c2 - 'a' + 10;
                } else {
                    goto notEscChar;
                }
                LexerGetChar(lexer);
                c <<= 4;
                c2 = LexerGetChar(lexer);
                if (c2 >= '0' && c2 <= '9') {
                    c += c2 - '0';
                } else if (c2 >= 'A' && c2 <= 'F') {
                    c += c2 - 'A' + 10;
                } else if (c2 >= 'a' && c2 <= 'f') {
                    c += c2 - 'a' + 10;
                } else {
                    EC_WARNING(-1);
                }
            }
        notEscChar:
            if (++n == tokBufSize) {
                EC_WARNING(-1);
                break;
            }
            *p++ = c;
        }
        *p = '\0';
        initName(obj, tokBuf);
        break;

    /* array punctuation */
    case '[':
    case ']':
        tokBuf[0] = c;
        tokBuf[1] = '\0';
        initCmd(obj, tokBuf);
        break;

    /* hex string or dict punctuation */
    case '<':
        c = LexerLookChar(lexer);

        if (c == '<') {
            LexerGetChar(lexer);
            tokBuf[0] = tokBuf[1] = '<';
            tokBuf[2] = '\0';
            initCmd(obj, tokBuf);
        } else {
            p = tokBuf;
            m = n = 0;
            c2 = 0;
            GStrInit(&s);
            if (GMemHadError()) {
                initError(obj);
                break;
            }
            while (1) {
                c = LexerGetChar(lexer);
                if (c == '>') {
                    break;
                } else if (c == EOF) {
                    EC_WARNING(-1);
                    break;
                } else if (!isspace(c)) {
                    c2 = c2 << 4;
                    if (c >= '0' && c <= '9') {
                        c2 += c - '0';
                    } else if (c >= 'A' && c <= 'F') {
                        c2 += c - 'A' + 10;
                    } else if (c >= 'a' && c <= 'f') {
                        c2 += c - 'a' + 10;
                    } else {
                        EC_WARNING(-1);
                    }
                    if (++m == 2) {
                        if (n == tokBufSize) {
                            GStrAppendStringN(&s, tokBuf, tokBufSize);
                            p = tokBuf;
                            n = 0;
                        }
                        *p++ = (char)c2;
                        ++n;
                        c2 = 0;
                        m = 0;
                    }
                }
            }
            GStrAppendStringN(&s, tokBuf, n);
            if (m == 1) {
                GStrAppendChar(&s, (char)(c2 << 4));
            }
            if (GMemHadError()) {
                GStrFree(&s);
                initError(obj);
                break;
            }
            initString(obj, &s);
            GStrFree(&s);
        }
        break;

    /* dict punctuation */
    case '>':
        c = LexerLookChar(lexer);
        if (c == '>') {
            LexerGetChar(lexer);
            tokBuf[0] = tokBuf[1] = '>';
            tokBuf[2] = '\0';
            initCmd(obj, tokBuf);
        } else {
            EC_WARNING(-1);
            initError(obj);
        }
        break;

    /* error */
    case ')':
    case '{':
    case '}':
        initError(obj);
        break;

    /* command */
    default:
        p = tokBuf;
        *p++ = c;
        n = 1;
        while ((c = LexerLookChar(lexer)) != EOF && !(c < 128 && endOfNameChars[c])) {
            LexerGetChar(lexer);
            if (++n == tokBufSize) {
                EC_WARNING(-1);
                break;
            }
            *p++ = c;
        }
        *p = '\0';
        if (tokBuf[0] == 't' && !strcmp(tokBuf, "true")) {
            initBool(obj, gTrue);
        } else if (tokBuf[0] == 'f' && !strcmp(tokBuf, "false")) {
            initBool(obj, gFalse);
        } else if (tokBuf[0] == 'n' && !strcmp(tokBuf, "null")) {
            initNull(obj);
        } else {
            initCmd(obj, tokBuf);
        }
        break;
    }
}

/***********************************************************************
 *      LexerSkipToNextLine
 ***********************************************************************
 * SYNOPSIS:        skip to the beginning of the next line
 * PARAMETERS:      Lexer *lexer  lexer to advance
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called after reading a stream's "stream" keyword line, to
 *      position at the first byte of raw stream data.
 *
 * STRATEGY:
 *      Consume characters until a bare '\n', or a '\r' optionally
 *      followed by '\n', or EOF.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerSkipToNextLine(Lexer *lexer)
{
    long c;

    while (1) {
        c = LexerGetChar(lexer);
        if (c == EOF || c == '\n') {
            return;
        }
        if (c == '\r') {
            if (LexerLookChar(lexer) == '\n') {
                LexerGetChar(lexer);
            }
            return;
        }
    }
}

/***********************************************************************
 *      LexerGetStream
 ***********************************************************************
 * SYNOPSIS:        get the current underlying stream
 * PARAMETERS:      Lexer *lexer  lexer to query
 *
 * RETURNS:         Stream *  the current stream, or NULL if exhausted
 *
 * CONTEXT:
 *      Called by callers needing direct stream access, e.g. to switch
 *      from tokenized parsing to raw stream data reading.
 *
 * STRATEGY:
 *      Return the stream wrapped by the current Obj, if any.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
Stream *LexerGetStream(Lexer *lexer)
{
    return isNone(&lexer->curStr) ? (Stream *)NULL : getStream(&lexer->curStr);
}

/***********************************************************************
 *      LexerGetPos
 ***********************************************************************
 * SYNOPSIS:        get the current position in the current stream
 * PARAMETERS:      Lexer *lexer  lexer to query
 *
 * RETURNS:         long  current byte position, or -1 if exhausted
 *
 * CONTEXT:
 *      Called to record a position for later seeking, e.g. before
 *      reading an object so its offset can be cached.
 *
 * STRATEGY:
 *      Delegate to ObjStreamGetPos() on the current stream.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
long LexerGetPos(Lexer *lexer)
{
    return isNone(&lexer->curStr) ? -1 : ObjStreamGetPos(&lexer->curStr);
}

/***********************************************************************
 *      LexerSetPos
 ***********************************************************************
 * SYNOPSIS:        set the position in the current stream
 * PARAMETERS:      Lexer *lexer  lexer to modify
 *                  long pos      byte position to seek to
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *      Called to resume lexing from a previously recorded position.
 *
 * STRATEGY:
 *      Delegate to ObjStreamSetPos() on the current stream, if any.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
void LexerSetPos(Lexer *lexer, long pos)
{
    if (!isNone(&lexer->curStr)) {
        ObjStreamSetPos(&lexer->curStr, pos);
    }
}

/***********************************************************************
 *      LexerGetXRef
 ***********************************************************************
 * SYNOPSIS:        get the cross-reference table this lexer uses
 * PARAMETERS:      Lexer *lexer  lexer to query
 *
 * RETURNS:         XRef *  lexer->xref
 *
 * CONTEXT:
 *      Called by the Parser to resolve indirect objects while lexing.
 *
 * STRATEGY:
 *      Return the field directly.
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/18/26    Initial Revision
 *
 ***********************************************************************/
XRef *LexerGetXRef(Lexer *lexer)
{
    return lexer->xref;
}
