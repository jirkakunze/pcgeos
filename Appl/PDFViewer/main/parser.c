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

GBool ParserMakeStream(Parser *parser, Stream **str, Obj *dict);


/***********************************************************************
 *      ParserInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      Parser *parser    parser
 *                  Lexer *lexer1    lexer1
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void ParserInit(Parser *parser, Lexer *lexer1)
{
    parser->lexer = lexer1;
    parser->inlineImg = 0;
    /*
     * Only XRefFetch's classic path overrides this, right after ParserInit,
     * once it knows that object's num/gen.
     */
    parser->noCrypt = gTrue;
    parser->cryptNum = parser->cryptGen = -1;
    LexerGetObj(parser->lexer, &parser->buf1);
    LexerGetObj(parser->lexer, &parser->buf2);
}

/***********************************************************************
 *      ParserFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      Parser *parser    parser
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

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

/***********************************************************************
 *      ParserSkipValue
 ***********************************************************************
 * SYNOPSIS:        Skip value.
 * PARAMETERS:      Parser *parser    parser
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/
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

/***********************************************************************
 *      ParserGetObj
 ***********************************************************************
 * SYNOPSIS:        Get object.
 * PARAMETERS:      Parser *parser    parser
 *                  Obj *obj    object
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void ParserGetObj(Parser *parser, Obj *obj)
{
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
        /*
         * Only the FIRST time the limit is hit within a given top-level parse
         * gets an actual EC_WARNING.
         */
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

    /* refill buffer after inline image data */
    if (parser->inlineImg == 2)
    {
        ObjFree(pbuf1);
        ObjFree(pbuf2);
        LexerGetObj(parser->lexer, pbuf1);
        LexerGetObj(parser->lexer, pbuf2);
        parser->inlineImg = 0;
    }

    /* array */
    if (isCmdSame(pbuf1, "["))
    {
        ParserShift(parser);
        initArray(obj);
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

        /* dictionary or stream */
    }
    else if (isCmdSame(pbuf1, "<<"))
    {
        ParserShift(parser);
        initDict(obj);
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
                /* error(getPos(), "Dictionary key must be a name object"); */
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

        /* indirect reference or integer */
    }
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

        /* simple object */
    }
    else
    {
        ObjMove(obj, pbuf1);
        ParserShift(parser);
    }
    parserGetObjDone:
    --depth;
    if (depth == 0)
    {
        alreadyWarned = gFalse;
    }
}

/***********************************************************************
 *      ParserMakeStream
 ***********************************************************************
 * SYNOPSIS:        Create stream.
 * PARAMETERS:      Parser *parser    parser
 *                  Stream **str    string
 *                  Obj *dict    dictionary
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

GBool ParserMakeStream(Parser *parser, Stream **str, Obj *dict)
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
            EC_WARNING(-1); /* bad length attr */
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

    /* make base stream */

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

    /*
     * Decrypt (RC4), if this object is one XRefFetch flagged as such via
     * parser->cryptNum/cryptGen.
     */
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

    /* refill token buffers and check for 'endstream' */
    ParserShift(parser); /* kill '>>' */
    ParserShift(parser); /* kill 'stream' */
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

/***********************************************************************
 *      ParserShift
 ***********************************************************************
 * SYNOPSIS:        Process parser shift.
 * PARAMETERS:      Parser *parser    parser
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void ParserShift(Parser *parser)
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
    if (parser->inlineImg > 0) /* don't buffer inline image data' */
    {
        initNull(&parser->buf2);
    }
    else
    {
        LexerGetObj(parser->lexer, &parser->buf2);
    }
}

/***********************************************************************
 *      ParserGetPos
 ***********************************************************************
 * SYNOPSIS:        Get position.
 * PARAMETERS:      Parser *parser    parser
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

long ParserGetPos(Parser *parser)
{
    return StreamGetPos(LexerGetStream(parser->lexer));
}

/***********************************************************************
 *      ParserGetLength
 ***********************************************************************
 * SYNOPSIS:        Get length.
 * PARAMETERS:      Parser *parser    parser
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

long ParserGetLength(Parser *parser)
{
    return StreamGetLength(LexerGetStream(parser->lexer));
}

