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
 * FILE:          xref.c
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
 *      Port of Derek Noonburg's "XRef" class from xpdf 0.8:
 *      the cross-reference table of a PDF file.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include <Ansi/string.h>
#include <Ansi/stdlib.h>
#include <Ansi/stdio.h>
#include "obj.h"
#include "array.h"
#include "stream.h"
#include "parser.h"
#include "dict.h"
#include "xref.h"
#include "lexer.h"
#include "gmem.h"
#include "crypt.h"
#include <vm.h>
#include <heap.h>
#include <hugearr.h>
#include <ctype.h>
#include <ec.h>

#define xrefSearchSize 1024  /* Bytes scanned for startxref near EOF. */

static word XRefClampGen(long v)
{
    return (v > 0xFFFFL) ? (word)0xFFFF : (word)v;
}

static long XRefParseVarDigits(const char *p);

static long XRefReadTrailer(XRef *xref, Stream *fs);
static void XRefSetStart(XRef *xref, Stream *fs);

static GBool XRefSectionIsStream(XRef *xref, Stream *fs, long pos);
static long XRefStreamReadField(Obj *streamObj, long width, long defaultVal);
static long XRefReadTrailerFromStream(XRef *xref, Stream *fs, long pos);
static GBool XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection);
static GBool XRefReadXRefStream(XRef *xref, Stream *fs, long *pos,
    GBool isFirstSection, GBool hybrid);
static void XRefFetchFromObjStm(XRef *xref, long objStmNum, long idxInStm,
    Obj *obj);

/********************************************************************
 *                      XRefInitNull
 ********************************************************************
 * SYNOPSIS:      Initialize an empty cross-reference table.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void XRefInitNull(XRef *xref)
{
    xref->size = 0;
    xref->entries = NULL;
    xref->vmFile = 0;
    initNull(&xref->trailerDict);
    xref->encrypted = gFalse;
    xref->needPassword = gFalse;
    xref->ownerAccess = gFalse;
    xref->objStmCacheNum = -1;
    xref->objStmCacheN = 0;
    xref->objStmCacheFirst = 0;
    xref->objStmPairOffsetsH = NullHandle;
}

/********************************************************************
 *                      XRefInit
 ********************************************************************
 * SYNOPSIS:      Read the cross-reference table of a PDF file.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                fileHan               Handle of the open PDF file.
 *                *password             Ptr to the password, NULL for none.
 *
 * RETURNS:       Boolean               TRUE if the table is valid. FALSE with
 *                                      xref->needPassword set if the password
 *                                      is wrong.
 *
 * STRATEGY:      Find the header and the last trailer, make the entry table (a
 *                HugeArray in a temporary VM file), read all sections of the
 *                /Prev chain (tables and streams), then set up the decryption.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
Boolean XRefInit(XRef *xref, FileHandle fileHan, const char *password)
{
    XRefEntry entryTemplate;
    XRefEntry *batchBuf;
    word batchMax, batchThis;
    long remaining;
    long entryIdx;
    char *p;

    Stream fs;
    Obj obj;

    long pos;

    initNull(&obj);
    FStreamInit(&fs, fileHan, 0, -1, &obj);
    ObjFree(&obj);

    xref->ok = gTrue;
    xref->size = 0;
    xref->entries = NULL;
    xref->fHan = fileHan;
    /* GEOS reuses file handles: drop data cached for an earlier file. */
    FStreamInvalidateReadCache(fileHan);

    /* Own scratch VM file: the page VM file of PdfOpen does not exist yet */
    p = xref->vmFileName;
    FileConstructFullPath(&p, sizeof(xref->vmFileName), SP_WASTE_BASKET, "",
        TRUE);
    xref->vmFile = VMOpen(xref->vmFileName,
        VMAF_FORCE_READ_WRITE | VMAF_USE_BLOCK_LEVEL_SYNCHRONIZATION,
        VMO_TEMP_FILE,
        0);
    if (!xref->vmFile)
    {
        goto err;
    }

    XRefSetStart(xref, &fs);

    pos = XRefReadTrailer(xref, &fs);

    if (pos == 0)
    {
        goto err;

    }
    else
    {
        /* One HugeArray element per entry, all free at first */
        entryTemplate.offset = -1;
        entryTemplate.gen = 0;
        entryTemplate.used = gFalse;
        entryTemplate.type = xrefEntryFree;
        xref->entries = HugeArrayCreate(xref->vmFile, sizeof(XRefEntry), 0);

        /* Write the free entries in batches */
        batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
        if (batchMax < 1)
        {
            batchMax = 1;
        }
        batchBuf = gmalloc((long)batchMax * sizeof(XRefEntry));
        if (!xref->entries || !batchBuf)
        {
            gfree(batchBuf);
            if (xref->entries)
            {
                HugeArrayDestroy(xref->vmFile, xref->entries);
            }
            xref->size = 0;
            xref->entries = NULL;
            goto err;
        }
        for (entryIdx = 0; entryIdx < batchMax; ++entryIdx)
        {
            batchBuf[entryIdx] = entryTemplate;
        }

        remaining = xref->size;
        while (remaining > 0)
        {
            batchThis = (remaining > (long)batchMax) ?
                batchMax : (word)remaining;
            HugeArrayAppend(xref->vmFile, xref->entries, batchThis, batchBuf);
            remaining -= batchThis;
        }
        gfree(batchBuf);

        /* A /Prev chain may mix classic tables and cross-reference streams. */
        {
            GBool isFirstSection = gTrue;
            while (XRefSectionIsStream(xref, &fs, pos)
                ? XRefReadXRefStream(xref, &fs, &pos, isFirstSection, gFalse)
                : XRefReadXRef(xref, &fs, &pos, isFirstSection))
            {
                isFirstSection = gFalse;
            }
        }

        if (!xref->ok)
        {
            if (xref->entries)
            {
                HugeArrayDestroy(xref->vmFile, xref->entries);
            }
            xref->size = 0;
            xref->entries = NULL;

            goto err;
        }
    }

    xref->encrypted = gFalse;
    xref->needPassword = gFalse;
    if (XRefCheckEncrypted(xref))
    {
        /* password: user or owner password, "" tries the empty one */
        switch (XRefSetupEncryption(xref, password ? password : ""))
        {
            case CRYPT_OK:
                break;
            case CRYPT_BAD_PASSWORD:
                xref->needPassword = gTrue;
                xref->ok = gFalse;
                goto err;
            default:
                xref->ok = gFalse;
                goto err;
        }
    }

    StreamFree(&fs);
    return TRUE;

    err:
    StreamFree(&fs);
    return FALSE;

}

/********************************************************************
 *                      XRefFree
 ********************************************************************
 * SYNOPSIS:      Free the cross-reference table and its VM file.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void XRefFree(XRef *xref)
{
    FStreamInvalidateReadCache(xref->fHan);

    if (xref->entries)
    {
        HugeArrayDestroy(xref->vmFile, xref->entries);
    }
    if (xref->vmFile)
    {
        VMClose(xref->vmFile, FALSE);
        FileDelete(xref->vmFileName);
    }

    ObjFree(&xref->trailerDict);

    if (xref->objStmPairOffsetsH)
    {
        MemFree(xref->objStmPairOffsetsH);
        xref->objStmPairOffsetsH = NullHandle;
    }
    xref->objStmCacheNum = -1;
}

/* Largest object stream with a cached offset table (4 bytes each) */
#define XREF_OBJSTM_CACHE_MAX 1024

/* Bytes searched for "%PDF-" (garbage before the header) */
#define headerSearchSize 512

/********************************************************************
 *                      XRefSetStart
 ********************************************************************
 * SYNOPSIS:      Find the "%PDF-" header and set xref->start.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Garbage bytes before the header are allowed (up to
 *                headerSearchSize); all offsets are relative to the header.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void XRefSetStart(XRef *xref, Stream *fs)
{
    /* Match "%PDF-" char by char (no buffer) */
    static const char pattern[] = "%PDF-";
    short matchLen = 0;
    short i;
    short c;

    StreamSetPos(fs, 0);

    for (i = 0; i < headerSearchSize; ++i)
    {
        c = FStreamGetChar(fs);
        if (c == EOF)
        {
            break;
        }
        if ((char)c == pattern[matchLen])
        {
            ++matchLen;
            if (matchLen == 5)
            {
                xref->start = i - 4;
                return;
            }
        }
        else
        {
            matchLen = ((char)c == pattern[0]) ? 1 : 0;
        }
    }
    EC_WARNING(-1);
    xref->start = 0;
}

/********************************************************************
 *                      XRefSectionIsStream
 ********************************************************************
 * SYNOPSIS:      Check if a section is a cross-reference stream.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream.
 *                pos                   Offset of the section.
 *
 * RETURNS:       GBool                 gTrue if the section does not start with
 *                                      "xref".
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static GBool XRefSectionIsStream(XRef *xref, Stream *fs, long pos)
{
    short c;
    char s[4];
    short i;

    StreamSetPos(fs, xref->start + pos);
    while ((c = StreamGetChar(fs)) != EOF && isspace(c)) ;
    s[0] = (char)c;
    for (i = 1; i < 4; ++i)
    {
        s[i] = (char)StreamGetChar(fs);
    }
    return !(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f');
}

/********************************************************************
 *                      XRefStreamReadField
 ********************************************************************
 * SYNOPSIS:      Read one field of a cross-reference stream entry.
 *
 * PARAMETERS:    *streamObj            Ptr to the decoded stream object.
 *                width                 Field width in bytes (/W).
 *                defaultVal            Value for width 0.
 *
 * RETURNS:       long                  Field value (big-endian).
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static long XRefStreamReadField(Obj *streamObj, long width, long defaultVal)
{
    long val;
    long i;
    int c;

    if (width <= 0)
    {
        return defaultVal;
    }
    val = 0;
    for (i = 0; i < width; ++i)
    {
        c = ObjStreamGetChar(streamObj);
        if (c == EOF)
        {
            c = 0;
        }
        val = (val << 8) | (c & 0xff);
    }
    return val;
}

/********************************************************************
 *                      XRefReadTrailerFromStream
 ********************************************************************
 * SYNOPSIS:      Read /Size, /Root and the trailer of a cross-reference stream.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream (not used).
 *                pos                   Offset of the stream object.
 *
 * RETURNS:       long                  pos, 0 on error.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static long XRefReadTrailerFromStream(XRef *xref, Stream *fs, long pos)
{
    Parser parser;
    Lexer *lexer;
    Stream *fs2;
    Obj numObj, genObj, objKwObj, streamObj, fieldObj;
    Dict *dict;

    (void)fs;
    initNull(&numObj);
    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));
    if (!fs2 || !lexer)
    {
        gfree(fs2);
        gfree(lexer);
        return 0;
    }
    FStreamInit(fs2, xref->fHan, xref->start + pos, -1, &numObj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&numObj);

    ParserGetObj(&parser, &numObj);
    ParserGetObj(&parser, &genObj);
    ParserGetObj(&parser, &objKwObj);
    if (!(isInt(&numObj) && isInt(&genObj) && isCmdSame(&objKwObj, "obj")))
    {
        ObjFree(&numObj);
        ObjFree(&genObj);
        ObjFree(&objKwObj);
        ParserFree(&parser);
        return 0;
    }
    ObjFree(&numObj);
    ObjFree(&genObj);
    ObjFree(&objKwObj);

    ParserGetObj(&parser, &streamObj);
    ParserFree(&parser);

    if (!isStream(&streamObj))
    {
        ObjFree(&streamObj);
        return 0;
    }
    dict = ObjStreamGetDict(&streamObj);

    DictLookupNF(dict, "Size", &fieldObj);
    if (isInt(&fieldObj))
    {
        xref->size = getInt(&fieldObj);
    }
    else
    {
        ObjFree(&fieldObj);
        ObjFree(&streamObj);
        return 0;
    }
    ObjFree(&fieldObj);

    DictLookupNF(dict, "Root", &fieldObj);
    if (isRef(&fieldObj))
    {
        xref->rootNum = getRefNum(&fieldObj);
        xref->rootGen = getRefGen(&fieldObj);
    }
    else
    {
        ObjFree(&fieldObj);
        ObjFree(&streamObj);
        return 0;
    }
    ObjFree(&fieldObj);

    /* The trailer is the stream dictionary, not the stream */
    dict = ObjStreamGetDict(&streamObj);
    ObjFree(&xref->trailerDict);
    initDictData(&xref->trailerDict, dict);
    DictIncRef(dict);

    ObjFree(&streamObj);
    return pos;
}

/********************************************************************
 *                      XRefReadXRefStream
 ********************************************************************
 * SYNOPSIS:      Read one cross-reference stream section.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream (not used).
 *                *pos                  Ptr to the offset of the section; set to
 *                                      /Prev (in/out).
 *                isFirstSection        gTrue for the newest section (written in
 *                                      batches).
 *                hybrid                gTrue for the /XRefStm of a table: it
 *                                      may also fill entries that the table
 *                                      marks as free, and its /Prev is ignored.
 *
 * RETURNS:       GBool                 gTrue if a /Prev section follows.
 *
 * STRATEGY:      Entries are read with the /W field widths for each /Index
 *                range. Type 1 is an offset, type 2 an object in an object
 *                stream. Older sections do not overwrite newer entries.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/23/26  JK        Hybrid files (/XRefStm)
 *******************************************************************/
static GBool XRefReadXRefStream(XRef *xref, Stream *fs, long *pos, GBool isFirstSection,
    GBool hybrid)
{
    Parser parser;
    Lexer *lexer;
    Stream *fs2;
    Obj numObj, genObj, objKwObj, streamObj;
    Obj wObj, indexObj, prevObj, elemObj;
    Dict *dict;
    byte w[3];
    long objNum, count, sectionIdx, numSections;
    long i;
    long type, f2, f3;
    XRefEntry *pEntry;
    word entrySize;
    GBool more;

    word batchMax, batchCount;
    dword batchStart;
    XRefEntry *batchBuf = NULL;

    if (isFirstSection)
    {
        batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
        if (batchMax < 1)
        {
            batchMax = 1;
        }
        batchBuf = gmalloc((long)batchMax * sizeof(XRefEntry));
        batchCount = 0;
        if (!batchBuf)
        {
            goto err;
        }
    }

    (void)fs;
    initNull(&numObj);
    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));
    if (!fs2 || !lexer)
    {
        gfree(fs2);
        gfree(lexer);
        goto err;
    }
    FStreamInit(fs2, xref->fHan, xref->start + *pos, -1, &numObj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&numObj);

    ParserGetObj(&parser, &numObj);
    ParserGetObj(&parser, &genObj);
    ParserGetObj(&parser, &objKwObj);
    if (!(isInt(&numObj) && isInt(&genObj) && isCmdSame(&objKwObj, "obj")))
    {
        ObjFree(&numObj);
        ObjFree(&genObj);
        ObjFree(&objKwObj);
        ParserFree(&parser);
        goto err;
    }
    ObjFree(&numObj);
    ObjFree(&genObj);
    ObjFree(&objKwObj);

    ParserGetObj(&parser, &streamObj);
    ParserFree(&parser);

    if (!isStream(&streamObj))
    {
        ObjFree(&streamObj);
        goto err;
    }
    dict = ObjStreamGetDict(&streamObj);

    DictLookup(dict, "W", &wObj, xref);
    if (!isArray(&wObj) || ObjArrayGetLength(&wObj) < 3)
    {
        ObjFree(&wObj);
        ObjFree(&streamObj);
        goto err;
    }
    for (i = 0; i < 3; ++i)
    {
        long wVal;

        ObjArrayGet(&wObj, i, &elemObj, xref);
        wVal = isInt(&elemObj) ? getInt(&elemObj) : 0;
        ObjFree(&elemObj);
        if (wVal < 0 || wVal > 4)
        {
            ObjFree(&wObj);
            ObjFree(&streamObj);
            goto err;
        }
        w[i] = (byte)wVal;
    }
    ObjFree(&wObj);

    DictLookup(dict, "Index", &indexObj, xref);
    if (isArray(&indexObj))
    {
        numSections = ObjArrayGetLength(&indexObj) >> 1;
    }
    else
    {
        numSections = 0;
    }

    ObjStreamReset(&streamObj);

    for (sectionIdx = 0; sectionIdx < (numSections > 0 ?
        numSections : 1); ++sectionIdx)
    {
        if (numSections > 0)
        {
            ObjArrayGet(&indexObj, sectionIdx * 2, &elemObj, xref);
            objNum = isInt(&elemObj) ? getInt(&elemObj) : 0;
            ObjFree(&elemObj);
            ObjArrayGet(&indexObj, sectionIdx * 2 + 1, &elemObj, xref);
            count = isInt(&elemObj) ? getInt(&elemObj) : 0;
            ObjFree(&elemObj);
        }
        else
        {
            objNum = 0;
            count = xref->size;
        }

        for (i = 0; i < count; ++i, ++objNum)
        {
            type = XRefStreamReadField(&streamObj, w[0], 1);
            f2 = XRefStreamReadField(&streamObj, w[1], 0);
            f3 = XRefStreamReadField(&streamObj, w[2], 0);

            if (objNum < 0 || objNum >= xref->size)
            {
                continue; /* malformed /Index range -- ignore */
            }

            if (isFirstSection)
            {
                XRefEntry newEntry;

                switch (type)
                {
                    case 1:
                        newEntry.type = xrefEntryClassic;
                        newEntry.used = gTrue;
                        newEntry.offset = f2;
                        newEntry.gen = XRefClampGen(f3);
                        break;
                    case 2:
                        newEntry.type = xrefEntryCompressed;
                        newEntry.used = gTrue;
                        /* Object number of the object stream */
                        newEntry.offset = f2;
                        newEntry.gen = XRefClampGen(f3); /* index in it */
                        break;
                    default:
                        /* Type 0 (free) or unknown: the entry stays free */
                        newEntry.type = xrefEntryFree;
                        newEntry.used = gFalse;
                        newEntry.offset = -1;
                        newEntry.gen = 0;
                        break;
                }

                /* /Index ranges are not always contiguous */
                if (batchCount > 0 && (dword)objNum != batchStart + batchCount)
                {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                        batchStart, batchBuf);
                    batchCount = 0;
                }
                if (batchCount == 0)
                {
                    batchStart = objNum;
                }
                batchBuf[batchCount] = newEntry;
                ++batchCount;
                if (batchCount == batchMax)
                {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                        batchStart, batchBuf);
                    batchCount = 0;
                }
            }
            else
            {
                HugeArrayLock(xref->vmFile, xref->entries, objNum,
                    (void **) & pEntry, &entrySize);
                /* Newer sections win; hybrid: "f" entries from /XRefStm */
                if (pEntry->type != xrefEntryFree &&
                    (!hybrid || pEntry->used))
                {
                    HugeArrayUnlock(pEntry);
                    continue;
                }

                switch (type)
                {
                    case 1:
                        pEntry->type = xrefEntryClassic;
                        pEntry->used = gTrue;
                        pEntry->offset = f2;
                        pEntry->gen = XRefClampGen(f3);
                        HugeArrayDirty(pEntry);
                        break;
                    case 2:
                        pEntry->type = xrefEntryCompressed;
                        pEntry->used = gTrue;
                        /* Object number of the object stream */
                        pEntry->offset = f2;
                        pEntry->gen = XRefClampGen(f3); /* index in it */
                        HugeArrayDirty(pEntry);
                        break;
                    default:
                        /* Type 0 (free) or unknown: the entry stays free */
                        break;
                }
                HugeArrayUnlock(pEntry);
            }
        }
    }

    /* Write the last partial batch */
    if (batchBuf && batchCount > 0)
    {
        HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart,
            batchBuf);
        batchCount = 0;
    }
    if (batchBuf)
    {
        gfree(batchBuf);
        batchBuf = NULL;
    }

    ObjFree(&indexObj);

    /* Follow /Prev */
    DictLookupNF(dict, "Prev", &prevObj);
    if (isInt(&prevObj))
    {
        *pos = getInt(&prevObj);
        more = gTrue;
    }
    else
    {
        more = gFalse;
    }
    ObjFree(&prevObj);

    ObjFree(&streamObj);
    return more;

    err:
    /* batchBuf is NULL if the error came before the batch loop */
    if (batchBuf)
    {
        if (batchCount > 0)
        {
            HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                batchStart, batchBuf);
        }
        gfree(batchBuf);
    }
    xref->ok = gFalse;
    EC_WARNING(-1);
    return gFalse;
}

/********************************************************************
 *                      XRefReadTrailer
 ********************************************************************
 * SYNOPSIS:      Find "startxref" and read the last trailer.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream.
 *
 * RETURNS:       long                  Offset of the last section, 0 on error.
 *
 * STRATEGY:      Search "startxref" in the last xrefSearchSize bytes. A cross-
 *                reference stream is read by XRefReadTrailerFromStream.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static long XRefReadTrailer(XRef *xref, Stream *fs)
{

    Parser parser;
    Obj obj;
    /* 40 bytes are enough for the two short reads below */
    char buf[40];
    long n, pos, pos1;
    char *p;
    short c;
    long i;
    Obj *ptrailerDict = &xref->trailerDict;
    Stream *fs2;

    Lexer *lexer;

    /* Find the last "startxref" in the last xrefSearchSize bytes */
    {
        static const char startxrefPattern[] = "startxref";
        short matchLen = 0;
        long lastMatchEnd = -1;

        StreamSetPos(fs, -xrefSearchSize);
        for (n = 0; n < xrefSearchSize; ++n)
        {
            if ((c = StreamGetChar(fs)) == EOF)
            {
                break;
            }
            if ((char)c == startxrefPattern[matchLen])
            {
                ++matchLen;
                if (matchLen == 9)
                {
                    lastMatchEnd = n + 1;
                    matchLen = 0;
                }
            }
            else
            {
                matchLen = ((char)c == startxrefPattern[0]) ? 1 : 0;
            }
        }
        if (lastMatchEnd < 0)
        {
            return 0;
        }
        StreamSetPos(fs, -xrefSearchSize);
        for (i = 0; i < lastMatchEnd; ++i)
        {
            StreamGetChar(fs); /* discard -- just repositioning */
        }
    }
    while (isspace(c = StreamLookChar(fs)))
    {
        StreamGetChar(fs);
    }
    pos = 0;
    while ((c = StreamLookChar(fs)) >= '0' && c <= '9')
    {
        pos = pos * 10 + (c - '0');
        StreamGetChar(fs);
    }

    if (XRefSectionIsStream(xref, fs, pos))
    {
        return XRefReadTrailerFromStream(xref, fs, pos);
    }

    StreamSetPos(fs, xref->start + pos);
    for (i = 0; i < 4; ++i)
    {
        buf[i] = StreamGetChar(fs);
    }
    if (strncmp(buf, "xref", 4))
    {
        return 0;
    }
    pos1 = pos + 4;
    while (1)
    {
        StreamSetPos(fs, xref->start + pos1);
        for (i = 0; i < 35; ++i)
        {
            if ((c = StreamGetChar(fs)) == EOF)
            {
                return 0;
            }
            buf[i] = c;
        }
        if (!strncmp(buf, "trailer", 7))
        {
            break;
        }
        p = buf;
        while (isspace(*p))
        {
            ++p;
        }
        while ('0' <= *p && *p <= '9')
        {
            ++p;
        }
        while (isspace(*p))
        {
            ++p;
        }
        n = XRefParseVarDigits(p);
        while ('0' <= *p && *p <= '9')
        {
            ++p;
        }
        while (isspace(*p))
        {
            ++p;
        }
        if (p == buf)
        {
            return 0;
        }
        pos1 += (p - buf) + n * 20;
    }
    pos1 += 7;

    initNull(&obj);

    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof (Lexer));
    if (!fs2 || !lexer)
    {
        gfree(fs2);
        gfree(lexer);
        return 0;
    }

    FStreamInit(fs2, xref->fHan, xref->start + pos1, -1, &obj);

    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&obj);

    ParserGetObj(&parser, ptrailerDict);
    if (isDict(ptrailerDict))
    {
        ObjDictLookupNF(ptrailerDict, "Size", &obj);
        if (isInt(&obj))
        {
            xref->size = getInt(&obj);
        }
        else
        {
            pos = 0;
        }
        ObjFree(&obj);
        ObjDictLookupNF(ptrailerDict, "Root", &obj);
        if (isRef(&obj))
        {
            xref->rootNum = getRefNum(&obj);
            xref->rootGen = getRefGen(&obj);
        }
        else
        {
            pos = 0;
        }
        ObjFree(&obj);
    }
    else
    {
        pos = 0;
    }
    ParserFree(&parser);

    return pos;

}

/********************************************************************
 *                      XRefParseDigits
 ********************************************************************
 * SYNOPSIS:      Parse a fixed number of decimal digits.
 *
 * PARAMETERS:    *p                    Ptr to the digits.
 *                count                 Number of digits.
 *
 * RETURNS:       long                  Value.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static long XRefParseDigits(const char *p, int count)
{
    long value = 0;

    while (count--)
    {
        value = value * 10 + (*p++ - '0');
    }
    return value;
}

/********************************************************************
 *                      XRefParseVarDigits
 ********************************************************************
 * SYNOPSIS:      Parse decimal digits up to the first other char.
 *
 * PARAMETERS:    *p                    Ptr to the digits.
 *
 * RETURNS:       long                  Value.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static long XRefParseVarDigits(const char *p)
{
    long value = 0;

    while (*p >= '0' && *p <= '9')
    {
        value = value * 10 + (*p++ - '0');
    }

    return value;
}

/********************************************************************
 *                      XRefReadXRef
 ********************************************************************
 * SYNOPSIS:      Read one classic cross-reference table section.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                *fs                   Ptr to the file stream.
 *                *pos                  Ptr to the offset of the section; set to
 *                                      /Prev (in/out).
 *                isFirstSection        gTrue for the newest section (written in
 *                                      batches).
 *
 * RETURNS:       GBool                 gTrue if a /Prev section follows.
 *
 * STRATEGY:      Read the 20-byte entries of each subsection. A /XRefStm of a
 *                hybrid file is read before /Prev.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static GBool XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection)
{
    Parser parser;
    Obj obj, obj2;
    char s[22];
    GBool more, ok;
    long stmPos;
    long first, n, i;
    short c;
    word j;
    Stream *fs2;
    Lexer *lexer;
    XRefEntry *pEntry;
    word entrySize;

    /* Batch buffer for a contiguous run of entries (first section) */
    word batchMax, batchCount;
    dword batchStart;
    XRefEntry *batchBuf = NULL;
    XRefEntry *pBatch;

    if (isFirstSection)
    {
        batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
        if (batchMax < 1)
        {
            batchMax = 1;
        }
        batchBuf = gmalloc((long)batchMax * sizeof(XRefEntry));
        batchCount = 0;
        if (!batchBuf)
        {
            goto err2;
        }
    }

    StreamSetPos(fs, xref->start + *pos);

    while ((c = StreamGetChar(fs)) != EOF && isspace(c))
    ;
    s[0] = (char)c;
    s[1] = StreamGetChar(fs);
    s[2] = StreamGetChar(fs);
    s[3] = StreamGetChar(fs);
    if (!(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f'))
    {
        goto err2;
    }

    while (1)
    {
        while ((c = StreamLookChar(fs)) != EOF && isspace(c))
        {
            StreamGetChar(fs);
        }
        if (c == 't')
        {
            break;
        }
        first = 0;
        for (i = 0; (c = StreamGetChar(fs)) != EOF && isdigit(c) && i < 20; ++i)
        {
            first = first * 10 + (c - '0');
        }
        if (i == 0)
        {
            goto err2;
        }
        while ((c = StreamLookChar(fs)) != EOF && isspace(c))
        {
            StreamGetChar(fs);
        }
        n = 0;
        for (i = 0; (c = StreamGetChar(fs)) != EOF && isdigit(c) && i < 20; ++i)
        {
            n = n * 10 + (c - '0');
        }
        if (i == 0)
        {
            goto err2;
        }
        while ((c = StreamLookChar(fs)) != EOF && isspace(c))
        {
            StreamGetChar(fs);
        }
        for (i = first; i < first + n; ++i)
        {
            for (j = 0; j < 20; ++j)
            {
                if ((c = StreamGetChar(fs)) == EOF)
                {
                    goto err2;
                }
                s[j] = (char)c;
            }
            if (i < 0 || i >= xref->size)
            {
                continue;
            }

            if (isFirstSection)
            {
                if (batchCount > 0 && (dword)i != batchStart + batchCount)
                {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                        batchStart, batchBuf);
                    batchCount = 0;
                }
                if (batchCount == 0)
                {
                    batchStart = i;
                }
                pBatch = &batchBuf[batchCount];
                pBatch->offset = XRefParseDigits(s, 10);
                pBatch->gen = XRefClampGen(XRefParseDigits(&s[11], 5));
                pBatch->type = xrefEntryClassic;
                if (s[17] == 'n')
                {
                    pBatch->used = gTrue;
                }
                else if (s[17] == 'f')
                {
                    pBatch->used = gFalse;
                }
                else
                {
                    /* Malformed data: write the valid entries first */
                    if (batchCount > 0)
                    {
                        HugeArrayReplace(xref->vmFile, xref->entries,
                            batchCount, batchStart, batchBuf);
                    }
                    gfree(batchBuf);
                    batchBuf = NULL;
                    goto err2;
                }
                ++batchCount;
                if (batchCount == batchMax)
                {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                        batchStart, batchBuf);
                    batchCount = 0;
                }
            }
            else
            {
                HugeArrayLock(xref->vmFile, xref->entries, i,
                    (void **) & pEntry, &entrySize);
                if (pEntry->offset < 0)
                {
                    pEntry->offset = XRefParseDigits(s, 10);
                    pEntry->gen = XRefClampGen(XRefParseDigits(&s[11], 5));
                    pEntry->type = xrefEntryClassic;
                    if (s[17] == 'n')
                    {
                        pEntry->used = gTrue;
                    }
                    else if (s[17] == 'f')
                    {
                        pEntry->used = gFalse;
                    }
                    else
                    {
                        HugeArrayDirty(pEntry);
                        HugeArrayUnlock(pEntry);
                        goto err2;
                    }
                    HugeArrayDirty(pEntry);
                }
                HugeArrayUnlock(pEntry);
            }
        }
    }

    /* Write the last partial batch */
    if (batchBuf && batchCount > 0)
    {
        HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart,
            batchBuf);
        batchCount = 0;
    }
    if (batchBuf)
    {
        gfree(batchBuf);
        batchBuf = NULL;
    }

    initNull(&obj);

    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));
    if (!fs2 || !lexer)
    {
        gfree(fs2);
        gfree(lexer);
        goto err2;
    }

    FStreamInit(fs2, xref->fHan, FStreamGetPos(fs), -1, &obj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&obj);

    ParserGetObj(&parser, &obj);
    if (!isCmdSame(&obj, "trailer"))
    {
        goto err1;
    }
    ObjFree(&obj);
    ParserGetObj(&parser, &obj);
    if (!isDict(&obj))
    {
        goto err1;
    }
    DictLookupNF(getDict(&obj), "Prev", &obj2);
    if (isInt(&obj2))
    {
        *pos = getInt(&obj2);
        more = gTrue;
    }
    else
    {
        more = gFalse;
    }
    ObjFree(&obj2);

    /* Hybrid file: read /XRefStm before /Prev; an error keeps the table */
    DictLookupNF(getDict(&obj), "XRefStm", &obj2);
    stmPos = isInt(&obj2) ? getInt(&obj2) : -1;
    ObjFree(&obj2);
    ObjFree(&obj);
    ParserFree(&parser);

    if (stmPos > 0)
    {
        ok = xref->ok;
        XRefReadXRefStream(xref, fs, &stmPos, gFalse, gTrue);
        xref->ok = ok;
    }
    return more;

    err1:
    ObjFree(&obj);
    err2:
    if (batchBuf)
    {
        if (batchCount > 0)
        {
            HugeArrayReplace(xref->vmFile, xref->entries, batchCount,
                batchStart, batchBuf);
        }
        gfree(batchBuf);
    }
    xref->ok = gFalse;
    EC_WARNING(-1);
    return gFalse;

}

/********************************************************************
 *                      XRefCheckEncrypted
 ********************************************************************
 * SYNOPSIS:      Check if the trailer has /Encrypt.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       GBool                 gTrue if the file is encrypted.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
GBool XRefCheckEncrypted(XRef *xref)
{
    Obj obj;
    GBool encrypted;

    ObjDictLookup(&xref->trailerDict, "Encrypt", &obj, xref);
    encrypted = !isNull(&obj);

    ObjFree(&obj);
    return encrypted;
}

/********************************************************************
 *                      ObjStreamGetCharCounted
 ********************************************************************
 * SYNOPSIS:      Read a byte from a stream object and count it.
 *
 * PARAMETERS:    *objP                 Ptr to the stream object.
 *                *consumed             Ptr to the byte count (in/out).
 *
 * RETURNS:       int                   Byte, EOF at the end.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static int ObjStreamGetCharCounted(Obj *objP, long *consumed)
{
    int c;

    c = ObjStreamGetChar(objP);
    if (c != EOF)
    {
        ++(*consumed);
    }
    return c;
}

/********************************************************************
 *                      XRefFetchFromObjStm
 ********************************************************************
 * SYNOPSIS:      Fetch an object from an object stream.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                objStmNum             Object number of the object stream.
 *                idxInStm              Index of the object in the object
 *                                      stream.
 *                *obj                  Ptr to the object to fill; null on
 *                                      error.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The data is compressed: read forward through the decoded
 *                stream. The offset table of the last object stream is cached
 *                in a movable block.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
static void
XRefFetchFromObjStm(XRef *xref, long objStmNum, long idxInStm, Obj *obj)
{
    Obj objStmObj, fieldObj;
    Obj dummyDict;
    Dict *dict;
    long n, first, i, pairOffset, targetOffset, consumed;
    long *offsetsP;
    short c;
    short j;
    Parser parser;
    Lexer *lexer;
    Stream *str;
    Stream *subStr;
    GBool cacheHit;

    /* One cached object stream header per document; large ones are not */

    XRefFetch(xref, objStmNum, 0, &objStmObj);
    if (!isStream(&objStmObj))
    {
        ObjFree(&objStmObj);
        initNull(obj);
        return;
    }
    dict = ObjStreamGetDict(&objStmObj);

    DictLookup(dict, "N", &fieldObj, xref);
    n = isInt(&fieldObj) ? getInt(&fieldObj) : 0;
    ObjFree(&fieldObj);

    DictLookup(dict, "First", &fieldObj, xref);
    first = isInt(&fieldObj) ? getInt(&fieldObj) : 0;
    ObjFree(&fieldObj);

    if (idxInStm < 0 || idxInStm >= n)
    {
        ObjFree(&objStmObj);
        initNull(obj);
        return;
    }

    ObjStreamReset(&objStmObj);
    consumed = 0;
    targetOffset = -1;

    cacheHit = (objStmNum == xref->objStmCacheNum
        && n == xref->objStmCacheN
        && first == xref->objStmCacheFirst
        && xref->objStmPairOffsetsH != NullHandle);

    if (cacheHit)
    {
        /* Same object stream as last time: offsets from the cache */
        offsetsP = MemLock(xref->objStmPairOffsetsH);
        EC_ERROR_IF(offsetsP == NULL, -1);
        targetOffset = offsetsP[idxInStm];
        MemUnlock(xref->objStmPairOffsetsH);
    }
    else
    {
        /* Cache miss: scan the header and fill the cache */
        if (xref->objStmPairOffsetsH)
        {
            MemFree(xref->objStmPairOffsetsH);
            xref->objStmPairOffsetsH = NullHandle;
        }
        offsetsP = NULL;
        if (n > 0 && n <= XREF_OBJSTM_CACHE_MAX)
        {
            /* No memory: no cache, the header is scanned every time. */
            xref->objStmPairOffsetsH = MemAlloc((word)(n * sizeof(long)),
                HF_SWAPABLE, HAF_ZERO_INIT | HAF_LOCK);
            if (xref->objStmPairOffsetsH)
            {
                offsetsP = MemDeref(xref->objStmPairOffsetsH);
            }
        }
        xref->objStmCacheNum = objStmNum;
        xref->objStmCacheN = n;
        xref->objStmCacheFirst = first;

        for (i = 0; i < n; ++i)
        {
            /* Skip the object number: the index selects the object */
            c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            while (c != EOF && isspace(c))
            {
                c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            }
            while (c != EOF && isdigit(c))
            {
                c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            }

            c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            while (c != EOF && isspace(c))
            {
                c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            }
            pairOffset = 0;
            for (j = 0; c != EOF && isdigit(c) && j < 31; ++j)
            {
                pairOffset = pairOffset * 10 + (c - '0');
                c = ObjStreamGetCharCounted(&objStmObj, &consumed);
            }

            if (offsetsP)
            {
                offsetsP[i] = pairOffset;
            }
            if (i == idxInStm)
            {
                targetOffset = pairOffset;
            }
        }
        if (offsetsP)
        {
            MemUnlock(xref->objStmPairOffsetsH);
        }
    }

    if (targetOffset < 0)
    {
        ObjFree(&objStmObj);
        initNull(obj);
        return;
    }

    while (consumed < first + targetOffset)
    {
        if (ObjStreamGetChar(&objStmObj) == EOF)
        {
            break;
        }
        ++consumed;
    }

    str = getStream(&objStmObj);

    subStr = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));
    if (!subStr || !lexer)
    {
        gfree(subStr);
        gfree(lexer);
        ObjFree(&objStmObj);
        initNull(obj);
        return;
    }
    initNull(&dummyDict);
    SubStreamInit(subStr, str, &dummyDict);
    ObjFree(&dummyDict);

    LexerInitFromStream(lexer, subStr, xref);
    ParserInit(&parser, lexer);
    ParserGetObj(&parser, obj);
    ParserFree(&parser);

    ObjFree(&objStmObj);
}

/*
 * Parser and scratch objects of XRefFetch. Kept on the heap: XRefFetch runs
 * at the deepest point of every object lookup, also inside nested Form
 * XObjects, and the process stack is small.
 */
typedef struct XRefFetchFrame
{
    Parser parser;
    Obj obj1, obj2, obj3;
} XRefFetchFrame;

/********************************************************************
 *                      XRefFetch
 ********************************************************************
 * SYNOPSIS:      Fetch an object by its number and generation.
 *
 * PARAMETERS:    *xref                 Ptr to the cross-reference table.
 *                num                   Object number.
 *                gen                   Generation number.
 *                *obj                  Ptr to the object to fill; null if the
 *                                      object is not found.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Parser and scratch objects are on the heap (XRefFetchFrame):
 *                XRefFetch runs at the deepest point of every lookup. Objects
 *                of encrypted files are decrypted by their parser.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      08/22/26  JK        Initial Revision
 *******************************************************************/
void XRefFetch(XRef *xref, long num, long gen, Obj *obj)
{
    XRefEntry *e;
    XRefFetchFrame *f;
    word entrySize;
    byte entryType;
    long entryOffset;
    word entryGen;

    Stream *fs2;

    Lexer *lexer;

    /* check for bogus ref - this can happen in corrupted PDF files */
    if (num < 0 || num >= xref->size)
    {
        initNull(obj);
        return ;
    }

    /* Lock only this entry: XRefFetchFromObjStm calls XRefFetch again */

    HugeArrayLock(xref->vmFile, xref->entries, num, (void **) & e, &entrySize);
    entryType = e->type;
    entryOffset = e->offset;
    entryGen = e->gen;
    HugeArrayUnlock(e);

    if (entryType == xrefEntryCompressed)
    {
        if (gen != 0)
        {
            initNull(obj);
            return;
        }
        XRefFetchFromObjStm(xref, entryOffset, entryGen, obj);
        return;
    }

    if (entryType == xrefEntryClassic && entryGen == gen && entryOffset >= 0)
    {
        f = gmalloc(sizeof(XRefFetchFrame));
        fs2 = gmalloc(sizeof(Stream));
        lexer = gmalloc(sizeof (Lexer));
        if (!f || !fs2 || !lexer)
        {
            gfree(f);
            gfree(fs2);
            gfree(lexer);
            initNull(obj);
            return;
        }

        initNull(&f->obj1);
        FStreamInit(fs2, xref->fHan, xref->start + entryOffset, -1, &f->obj1);
        LexerInitFromStream(lexer, fs2, xref);
        ParserInit(&f->parser, lexer);
        /* Only this parser decrypts (see ParserInit) */
        f->parser.cryptNum = num;
        f->parser.cryptGen = gen;
        f->parser.noCrypt = !xref->encrypted;
        ObjFree(&f->obj1);

        ParserGetObj(&f->parser, &f->obj1);
        ParserGetObj(&f->parser, &f->obj2);
        ParserGetObj(&f->parser, &f->obj3);
        if (isInt(&f->obj1) && getInt(&f->obj1) == num &&
            isInt(&f->obj2) && getInt(&f->obj2) == gen &&
            isCmdSame(&f->obj3, "obj"))
        {
            ParserGetObj(&f->parser, obj);
        }
        else
        {
            initNull(obj);
        }
        ObjFree(&f->obj1);
        ObjFree(&f->obj2);
        ObjFree(&f->obj3);
        ParserFree(&f->parser);
        gfree(f);
    }
    else
    {
        initNull(obj);
    }
}

void XRefGetDocInfo(XRef *xref, Obj *obj)
{

    ObjDictLookup(&xref->trailerDict, "Info", obj, xref);
}

GBool XRefIsOk(XRef *xref)
{
    return xref->ok;
}

void XRefGetCatalog(XRef *xref, Obj *obj)
{

    XRefFetch(xref, xref->rootNum, xref->rootGen, obj);
}

GBool XRefOkToPrint(XRef *xref)
{
    return !xref->encrypted || xref->ownerAccess ||
        (xref->permissions & 0x0004L) != 0;
}

GBool XRefOkToCopy(XRef *xref)
{
    return !xref->encrypted || xref->ownerAccess ||
        (xref->permissions & 0x0010L) != 0;
}

