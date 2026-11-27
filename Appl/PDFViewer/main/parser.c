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
#include "gstr.h"
#include <ec.h>

static GBool ParserMakeStream(Parser *parser, Stream **str, Obj *dict);
static void ParserShift(Parser *parser);


/********************************************************************
 *                      ParserInit
 ********************************************************************
 * SYNOPSIS:      Initialize a parser on a lexer and read the first two tokens.
 *
 * PARAMETERS:    *parser               Ptr to the parser.
 *                *lexer1               Ptr to the lexer. The parser owns it.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Decryption is off (noCrypt). XRefFetch turns it on for one
 *                object of an encrypted file.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void ParserInit(Parser *parser, Lexer *lexer1)
{
    parser->lexer = lexer1;
    parser->inlineImg = 0;
    /* XRefFetch turns decryption on for one object */
    parser->noCrypt = gTrue;
    parser->cryptNum = parser->cryptGen = -1;
    LexerGetObj(parser->lexer, &parser->buf1);
    LexerGetObj(parser->lexer, &parser->buf2);
}

/********************************************************************
 *                      ParserFree
 ********************************************************************
 * SYNOPSIS:      Free the lexer and the two token buffers.
 *
 * PARAMETERS:    *parser               Ptr to the parser.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
void ParserFree(Parser *parser)
{

    LexerFree(parser->lexer);
    gfree(parser->lexer);
    ObjFree(&parser->buf1);
    ObjFree(&parser->buf2);
}

/*
 * Defensive recursion-depth cap : nested PDF arrays/dictionaries deeper than
 * this aren't something any legitimate PDF content stream or object would
 * contain.
 */
#define parserGetObjMaxDepth 20

/********************************************************************
 *                      ParserSkipValue
 ********************************************************************
 * SYNOPSIS:      Skip one value, also a complete nested array or dictionary.
 *
 * PARAMETERS:    *parser               Ptr to the parser.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Count the brackets without making objects (no memory).
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
static void ParserSkipValue(Parser *parser)
{
    Obj *pbuf1 = &parser->buf1;
    short bracketDepth;

    if (isCmdSame(pbuf1, "[") || isCmdSame(pbuf1, "<<"))
    {
        bracketDepth = 1;
        ParserShift(parser);
        while (bracketDepth > 0 && !isEOF(pbuf1))
        {
            if (isCmdSame(pbuf1, "[") || isCmdSame(pbuf1, "<<"))
            {
                ++bracketDepth;
            }
            else if (isCmdSame(pbuf1, "]") || isCmdSame(pbuf1, ">>"))
            {
                --bracketDepth;
            }
            ParserShift(parser);
        }
    }
    else
    {
        ParserShift(parser);
    }
}

/********************************************************************
 *                      ParserDecryptString
 ********************************************************************
 * SYNOPSIS:      Decrypt a string object in place with RC4.
 *
 * PARAMETERS:    *parser               Ptr to the parser of an encrypted
 *                                      object.
 *                *obj                  Ptr to the string object.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Strings use the same object key as streams (ISO 32000 7.6.2).
 *                Objects in object streams are not decrypted again.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/23/26  JK        Initial version (strings were left encrypted)
 *******************************************************************/
static void ParserDecryptString(Parser *parser, Obj *obj)
{
    XRef *xref = LexerGetXRef(parser->lexer);
    Guchar objKey[16];
    short objKeyLen;
    long len;

    if (!xref || !xref->encrypted)
    {
        return;
    }
    len = GStrGetLength(getString(obj));
    if (len <= 0)
    {
        return;
    }
    XRefDeriveObjectKey(xref, parser->cryptNum, parser->cryptGen,
        objKey, &objKeyLen);
    RC4Apply(objKey, objKeyLen, (Guchar *)GStrGetCString(getString(obj)),
        len);
}

/********************************************************************
 *                      ParserGetObj
 ********************************************************************
 * SYNOPSIS:      Read the next object: array, dictionary, stream, reference or
 *                simple object.
 *
 * PARAMETERS:    *parser               Ptr to the parser.
 *                *obj                  Ptr to the object to fill. An error
 *                                      object on error.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Arrays and dictionaries are read recursively up to
 *                parserGetObjMaxDepth levels; deeper values are skipped. Out of
 *                memory: the rest of the array or dictionary is skipped, so
 *                that the token stream stays in sync.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void ParserGetObj(Parser *parser, Obj *obj)
{
    /* Nesting depth of the current top-level parse */
    static short depth = 0;
    static GBool alreadyWarned = gFalse;
    char *key;
    Stream *str = NULL;
    Obj obj2;
    long num;
    Obj *pbuf1 = &parser->buf1;
    Obj *pbuf2 = &parser->buf2;

    if (++depth > parserGetObjMaxDepth)
    {
        /* EC_WARNING only once per top-level parse */
        if (!alreadyWarned)
        {
            EC_WARNING(-1);
            alreadyWarned = gTrue;
        }
        --depth;
        if (depth == 0)
        {
            alreadyWarned = gFalse;
        }
        /* Skip the entire nested value to keep the token stream aligned. */
        ParserSkipValue(parser);
        initError(obj);
        return;
    }

    /* Inline image data was skipped: refill both token buffers */
    if (parser->inlineImg == 2)
    {
        ObjFree(pbuf1);
        ObjFree(pbuf2);
        LexerGetObj(parser->lexer, pbuf1);
        LexerGetObj(parser->lexer, pbuf2);
        parser->inlineImg = 0;
    }

    if (isCmdSame(pbuf1, "["))
    {
        ParserShift(parser);
        initArray(obj);
        /* Out of memory: skip the array */
        if (GMemHadError())
        {
            while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1))
            {
                ParserSkipValue(parser);
            }
            if (isCmdSame(pbuf1, "]"))
            {
                ParserShift(parser);
            }
            goto parserGetObjDone;
        }
        while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1))
        {
            ParserGetObj(parser, &obj2);
            ObjArrayAdd(obj, &obj2);
            ObjFree(&obj2);
            if (GMemHadError())
            {
                while (!isCmdSame(pbuf1, "]") && !isEOF(pbuf1))
                {
                    ParserSkipValue(parser);
                }
                break;
            }
        }
        if (isEOF(pbuf1))
        {
            EC_WARNING(-1);
        }

        ParserShift(parser);

    }
    else if (isCmdSame(pbuf1, "<<"))
    {
        ParserShift(parser);
        initDict(obj);
        /* Out of memory: skip the dictionary */
        if (GMemHadError())
        {
            while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
            {
                ParserSkipValue(parser);
            }
            if (isCmdSame(pbuf1, ">>"))
            {
                ParserShift(parser);
            }
            goto parserGetObjDone;
        }
        while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
        {
            if (!isName(pbuf1))
            {
                EC_WARNING(-1);
                ParserShift(parser);
            }
            else
            {
                key = copyString(getName(pbuf1));
                if (!key)
                {
                    ParserShift(parser);
                    while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
                    {
                        ParserSkipValue(parser);
                    }
                    break;
                }
                ParserShift(parser);
                if (isEOF(pbuf1) || isError(pbuf1))
                {
                    break;
                }
                ParserGetObj(parser, &obj2);
                DictAdd(obj->u.dict, key, &obj2);
                ObjFree(&obj2);
                if (GMemHadError())
                {
                    while (!isCmdSame(pbuf1, ">>") && !isEOF(pbuf1))
                    {
                        ParserSkipValue(parser);
                    }
                    break;
                }
            }
        }
        if (isEOF(pbuf1))
        {
            EC_WARNING(-1);
        }

        /* A dictionary followed by "stream" is a stream object */
        if (isCmdSame(pbuf2, "stream"))
        {
            if (ParserMakeStream(parser, &str, obj))
            {
                ObjFree(obj);
                initStream(obj, str);
            }
            else
            {
                ObjFree(obj);
                initError(obj);
            }
        }
        else
        {
            ParserShift(parser);
        }

    }
    /* An integer can be the start of "num gen R" */
    else if (isInt(pbuf1))
    {
        num = getInt(pbuf1);
        ParserShift(parser);
        if (isInt(pbuf1) && isCmdSame(pbuf2, "R"))
        {
            initRef(obj, num, getInt(pbuf1));
            ParserShift(parser);
            ParserShift(parser);
        }
        else
        {
            initInt(obj, num);
        }

    }
    else
    {
        ObjMove(obj, pbuf1);
        ParserShift(parser);
        /* Strings of encrypted files are decrypted here */
        if (isString(obj) && !parser->noCrypt)
        {
            ParserDecryptString(parser, obj);
        }
    }
    parserGetObjDone:
    --depth;
    if (depth == 0)
    {
        alreadyWarned = gFalse;
    }
}

/********************************************************************
 *                      ParserMakeStream
 ********************************************************************
 * SYNOPSIS:      Make the stream of a stream object.
 *
 * PARAMETERS:    *parser               Ptr to the parser; the next token is
 *                                      "stream".
 *                *str                  Ptr to the new stream (out).
 *                *dict                 Ptr to the stream dictionary.
 *
 * RETURNS:       GBool                 gFalse if /Length is bad or there is no
 *                                      memory.
 *
 * STRATEGY:      A substream of the file with /Length bytes, then RC4
 *                decryption (encrypted files) and the filters of the
 *                dictionary. The filters are not reset here (see
 *                StreamAddFilters).
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
static GBool ParserMakeStream(Parser *parser, Stream **str, Obj *dict)
{
    Obj obj, obj2;
    long pos, length;

    LexerSkipToNextLine(parser->lexer);
    pos = LexerGetPos(parser->lexer);

    ObjDictLookup(dict, "Length", &obj, LexerGetXRef(parser->lexer));
    if (isInt(&obj))
    {
        length = getInt(&obj);
        ObjFree(&obj);

    }
    else if (isRef(&obj))
    {
        ObjFetch(&obj2, &obj, LexerGetXRef(parser->lexer));
        if (isInt(&obj2))
        {
            length = getInt(&obj2);
            ObjFree(&obj);
            ObjFree(&obj2);
        }
        else
        {
            EC_WARNING(-1);
            ObjFree(&obj);
            ObjFree(&obj2);
            return gFalse;
        }
    }
    else
    {
        EC_WARNING(-1);

        ObjFree(&obj);
        return gFalse;
    }

    *str = gmalloc(sizeof(Stream));
    if (!*str)
    {
        return gFalse;
    }
    FStreamInit(*str,
        StreamGetFile(LexerGetStream(parser->lexer)),
        pos,
        length,
        dict);

    /* XXX: needs to not access stream data directly */

    /* Encrypted file: decrypt with the key of this object */
    if (!parser->noCrypt)
    {
        XRef *xref = LexerGetXRef(parser->lexer);
        if (xref && xref->encrypted)
        {
            Guchar objKey[16];
            short objKeyLen;
            Stream *rc4Str;

            XRefDeriveObjectKey(xref, parser->cryptNum, parser->cryptGen,
                objKey, &objKeyLen);
            rc4Str = gmalloc(sizeof(Stream));
            if (!rc4Str)
            {
                StreamFree(*str);
                gfree(*str);
                *str = NULL;
                return gFalse;
            }
            RC4StreamInit(rc4Str, *str, objKey, objKeyLen);
            *str = rc4Str;
        }
    }

    *str = StreamAddFilters(*str, dict, LexerGetXRef(parser->lexer));

    LexerSetPos(parser->lexer, pos + length);

    /* Skip ">>" and "stream", then expect "endstream" */
    ParserShift(parser);
    ParserShift(parser);
    if (isCmdSame(&parser->buf1, "endstream"))
    {
        ParserShift(parser);
    }
    else
    {
        EC_WARNING(-1);
    }

    return gTrue;
}

/********************************************************************
 *                      ParserShift
 ********************************************************************
 * SYNOPSIS:      Move the token buffers on by one token.
 *
 * PARAMETERS:    *parser               Ptr to the parser.
 *
 * RETURNS:       void
 *
 * STRATEGY:      After the inline image operator ID the data is binary: buf2 is
 *                not read until the image data is skipped.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Style cleanup
 *******************************************************************/
static void ParserShift(Parser *parser)
{
    if (parser->inlineImg > 0)
    {
        ++parser->inlineImg;
    }
    else if (isCmdSame(&parser->buf2, "ID"))
    {
        LexerGetChar(parser->lexer); /* skip char after 'ID' command */
        parser->inlineImg = 1;
    }
    ObjFree(&parser->buf1);
    ObjMove(&parser->buf1, &parser->buf2);
    if (parser->inlineImg > 0) /* do not read binary inline image data */
    {
        initNull(&parser->buf2);
    }
    else
    {
        LexerGetObj(parser->lexer, &parser->buf2);
    }
}

long ParserGetPos(Parser *parser)
{
    return StreamGetPos(LexerGetStream(parser->lexer));
}

long ParserGetLength(Parser *parser)
{
    return StreamGetLength(LexerGetStream(parser->lexer));
}

