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
 *      Port of Derek Noonburg's "XRef" class from xpdf 0.8.
 *      Simple variable-length string type.
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
#include <hugearr.h>
#include <ctype.h>
#include <ec.h>

#define xrefSearchSize 1024  /* Bytes scanned for startxref near EOF. */

static long XRefParseVarDigits(const char *p);

long XRefReadTrailer(XRef *xref, Stream *fs);
static void XRefSetStart(XRef *xref, Stream *fs);

/* PDF 1.5+ cross-reference streams. */
static GBool XRefSectionIsStream(XRef *xref, Stream *fs, long pos);
static long XRefStreamReadField(Obj *streamObj, long width, long defaultVal);
static long XRefReadTrailerFromStream(XRef *xref, Stream *fs, long pos);
GBool XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection);
GBool XRefReadXRefStream(XRef *xref, Stream *fs, long *pos,
    GBool isFirstSection);
static void XRefFetchFromObjStm(XRef *xref, long objStmNum, long idxInStm,
    Obj *obj);

/***********************************************************************
 *      XRefInitNull
 ***********************************************************************
 * SYNOPSIS:        Initialize an empty cross-reference table.
 * PARAMETERS:      XRef *xref    cross-reference table
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

void XRefInitNull(XRef *xref)
{
    xref->size = 0;
    xref->entries = NULL;
    xref->vmFile = 0;
    initNull(&xref->trailerDict);
    xref->encrypted = gFalse;
}

/***********************************************************************
 *      XRefInit
 ***********************************************************************
 * SYNOPSIS:        Initialize a cross-reference table from a PDF.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  FileHandle fileHan    file handle
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

Boolean XRefInit(XRef *xref, FileHandle fileHan)
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

    /*
     * Own scratch VM file for `entries`, separate from PdfDocInternal's
     * gstringFile (decoded pages): that one doesn't exist yet at this point in
     * PdfOpen's sequence .
     */
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
        /*
         * One HugeArray element per XRefEntry, pre-filled to "free/ unused"
         * via a template.
         */
        entryTemplate.offset = -1;
        entryTemplate.gen = 0;
        entryTemplate.used = gFalse;
        entryTemplate.type = xrefEntryFree;
        xref->entries = HugeArrayCreate(xref->vmFile, sizeof(XRefEntry), 0);

        /* Fill explicitly because HugeArray template replication is unclear. */
        batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
        if (batchMax < 1)
        {
            batchMax = 1;
        }
        batchBuf = gmalloc((long)batchMax * sizeof(XRefEntry));
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
                ? XRefReadXRefStream(xref, &fs, &pos, isFirstSection)
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
    if (XRefCheckEncrypted(xref))
    {
        /* Try the empty user password for encrypted PDFs. */
        if (!XRefSetupEncryption(xref))
        {
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

/***********************************************************************
 *      XRefFree
 ***********************************************************************
 * SYNOPSIS:        Release a cross-reference table.
 * PARAMETERS:      XRef *xref    cross-reference table
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

void XRefFree(XRef *xref)
{

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
}

/* Find garbage bytes at start of file */
#define headerSearchSize 512

/***********************************************************************
 *      XRefSetStart
 ***********************************************************************
 * SYNOPSIS:        Locate the startxref offset.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
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

static void XRefSetStart(XRef *xref, Stream *fs)
{
    /*
     * Streaming character-by-character match against "%PDF-" instead of
     * reading a 512-byte buffer and then searching it.
     */
    static const char pattern[] = "%PDF-";
    short matchLen = 0;
    short i;
    long c;

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

/***********************************************************************
 *      XRefSectionIsStream
 ***********************************************************************
 * SYNOPSIS:        Check whether an xref section is a stream.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
 *                  long pos    file position
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

static GBool XRefSectionIsStream(XRef *xref, Stream *fs, long pos)
{
    long c;
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

/***********************************************************************
 *      XRefStreamReadField
 ***********************************************************************
 * SYNOPSIS:        Read one cross-reference stream field.
 * PARAMETERS:      Obj *streamObj    stream obj
 *                  long width    width
 *                  long defaultVal    default val
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

/***********************************************************************
 *      XRefReadTrailerFromStream
 ***********************************************************************
 * SYNOPSIS:        Read trailer data from an xref stream.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
 *                  long pos    file position
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

    /*
     * But xref->trailerDict must end up objDict-typed, NOT a copy of the
     * stream object itself.
     */
    dict = ObjStreamGetDict(&streamObj);
    ObjFree(&xref->trailerDict);
    initDictData(&xref->trailerDict, dict);
    DictIncRef(dict);

    ObjFree(&streamObj);
    return pos;
}

/***********************************************************************
 *      XRefReadXRefStream
 ***********************************************************************
 * SYNOPSIS:        Read a cross-reference stream section.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
 *                  long *pos    file position
 *                  GBool isFirstSection    is first section
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

GBool XRefReadXRefStream(XRef *xref, Stream *fs, long *pos, GBool isFirstSection)
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
    }

    (void)fs;
    initNull(&numObj);
    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));
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
                        newEntry.gen = f3;
                        break;
                    case 2:
                        newEntry.type = xrefEntryCompressed;
                        newEntry.used = gTrue;
                        /* containing ObjStm's object number */
                        newEntry.offset = f2;
                        newEntry.gen = f3; /* index within that ObjStm */
                        break;
                    default:
                        /*
                         * type 0 (free) or anything unrecognized -- same
                         * "leave free" value the array was already batch-
                         * initialized with further up this file.
                         */
                        newEntry.type = xrefEntryFree;
                        newEntry.used = gFalse;
                        newEntry.offset = -1;
                        newEntry.gen = 0;
                        break;
                }

                /*
                 * Same contiguity check as XRefReadXRef's own isFirstSection
                 * path (see its comment) -- /Index can (and often does) list
                 * non-contiguous sections.
                 */
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
                if (pEntry->type != xrefEntryFree)
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
                        pEntry->gen = f3;
                        HugeArrayDirty(pEntry);
                        break;
                    case 2:
                        pEntry->type = xrefEntryCompressed;
                        pEntry->used = gTrue;
                        /* containing ObjStm's object number */
                        pEntry->offset = f2;
                        pEntry->gen = f3; /* index within that ObjStm */
                        HugeArrayDirty(pEntry);
                        break;
                    default:
                        /*
                         * type 0 (free) or anything unrecognized -- leave as
                         * xrefEntryFree, nothing to fetch either way
                         */
                        break;
                }
                HugeArrayUnlock(pEntry);
            }
        }
    }

    /* flush any final partial batch left over after the last section */
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

    /* /Prev chases the same way XRefReadXRef's does */
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
    /*
     * NULL-guarded, matching XRefReadXRef's own err2 cleanup -- all four goto
     * err sites above happen before the batching loop runs.
     */
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

/***********************************************************************
 *      XRefReadTrailer
 ***********************************************************************
 * SYNOPSIS:        Locate and read the trailer section.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
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

long XRefReadTrailer(XRef *xref, Stream *fs)
{

    Parser parser;
    Obj obj;
    /*
     * 40 bytes covers this function's two remaining small, bounded reads --
     * see project optimization analysis S2.
     */
    char buf[40];
    long n, pos, pos1;
    char *p;
    long c;
    long i;
    Obj *ptrailerDict = &xref->trailerDict;
    Stream *fs2;

    Lexer *lexer;

    /*
     * Find the LAST occurrence of "startxref" within the last xrefSearchSize
     * bytes of the file.
     */
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

/***********************************************************************
 *      XRefParseDigits
 ***********************************************************************
 * SYNOPSIS:        Parse a fixed number of decimal digits.
 * PARAMETERS:      const char *p    pointer
 *                  int count    count
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

static long XRefParseDigits(const char *p, int count)
{
    long value = 0;

    while (count--)
    {
        value = value * 10 + (*p++ - '0');
    }
    return value;
}

/***********************************************************************
 *      XRefParseVarDigits
 ***********************************************************************
 * SYNOPSIS:        Parse decimal digits up to a delimiter.
 * PARAMETERS:      const char *p    pointer
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

static long XRefParseVarDigits(const char *p)
{
    long value = 0;

    while (*p >= '0' && *p <= '9')
    {
        value = value * 10 + (*p++ - '0');
    }

    return value;
}

/***********************************************************************
 *      XRefReadXRef
 ***********************************************************************
 * SYNOPSIS:        Read a classic cross-reference section.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  Stream *fs    input stream
 *                  long *pos    file position
 *                  GBool isFirstSection    is first section
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

GBool XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection)
{
    Parser parser;
    Obj obj, obj2;
    char s[22];
    GBool more;
    long first, n, i;
    long c;
    word j;
    Stream *fs2;
    Lexer *lexer;
    XRefEntry *pEntry;
    word entrySize;

    /*
     * Batch-write support for the isFirstSection fast path -- batchBuf
     * accumulates real, filled-in XRefEntry copies for a contiguous run of
     * entries.
     */
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
    }

    /* seek to xref in stream */
    StreamSetPos(fs, xref->start + *pos);

    /* make sure it is an xref table */
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
                pBatch->gen = XRefParseDigits(&s[11], 5);
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
                    /*
                     * Flush valid batch entries before aborting malformed
                     * data.
                     */
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
                    pEntry->gen = XRefParseDigits(&s[11], 5);
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

    /* flush any final partial batch left over after the last subsection */
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
    ObjFree(&obj);
    ObjFree(&obj2);

    ParserFree(&parser);
    return more;

    err1:
    ObjFree(&obj);
    err2:
    /* The NULL guard makes shared err2 cleanup safe. */
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

/***********************************************************************
 *      XRefCheckEncrypted
 ***********************************************************************
 * SYNOPSIS:        Check whether the trailer declares encryption.
 * PARAMETERS:      XRef *xref    cross-reference table
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

GBool XRefCheckEncrypted(XRef *xref)
{
    Obj obj;
    GBool encrypted;

    ObjDictLookup(&xref->trailerDict, "Encrypt", &obj, xref);
    encrypted = !isNull(&obj);

    ObjFree(&obj);
    return encrypted;
}

/***********************************************************************
 *      ObjStreamGetCharCounted
 ***********************************************************************
 * SYNOPSIS:        Read a byte and update the consumed count.
 * PARAMETERS:      Obj *objP    obj p
 *                  long *consumed    consumed
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

/***********************************************************************
 *      XRefFetchFromObjStm
 ***********************************************************************
 * SYNOPSIS:        Fetch an object from an Object Stream.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  long objStmNum    obj stm num
 *                  long idxInStm    idx in stm
 *                  Obj *obj    object
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:

 *      Read forward through decoded ObjStm data; compressed offsets
 *      cannot be used as raw file seeks. *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

static void
XRefFetchFromObjStm(XRef *xref, long objStmNum, long idxInStm, Obj *obj)
{
    Obj objStmObj, fieldObj;
    Obj dummyDict;
    Dict *dict;
    long n, first, i, pairOffset, targetOffset, consumed;
    long c;
    short j;
    Parser parser;
    Lexer *lexer;
    Stream *str;
    Stream *subStr;
    GBool cacheHit;

    /*
     * Cache the last ObjStm header because its objects share one offset table.
     */
    static XRef *cachedXref = NULL;
    static long cachedObjStmNum = -1;
    static long cachedN = 0;
    static long cachedFirst = 0;
    static long *cachedPairOffsets = NULL;

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

    cacheHit = (xref == cachedXref && objStmNum == cachedObjStmNum
        && n == cachedN
        && first == cachedFirst && cachedPairOffsets != NULL);

    if (cacheHit)
    {
        /*
         * Same ObjStm as last time, with a matching element count -- already
         * know every pair's offset from the cache.
         */
        targetOffset = cachedPairOffsets[idxInStm];
    }
    else
    {
        /*
         * Cache miss : scan the header for real, same as always -- but also
         * record every pair's offset into a freshly (re)allocated cache array.
         */
        if (cachedPairOffsets)
        {
            gfree(cachedPairOffsets);
        }
        cachedPairOffsets = (n > 0) ? (long *)gmalloc(n * sizeof(long)) : NULL;
        cachedXref = xref;
        cachedObjStmNum = objStmNum;
        cachedN = n;
        cachedFirst = first;

        for (i = 0; i < n; ++i)
        {
            /*
             * objNum of this pair -- not needed, we already know which object
             * we want by its position (idxInStm), not by matching numbers, so
             * just skip past it
             */
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

            if (cachedPairOffsets)
            {
                cachedPairOffsets[i] = pairOffset;
            }
            if (i == idxInStm)
            {
                targetOffset = pairOffset;
            }
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
    initNull(&dummyDict);
    SubStreamInit(subStr, str, &dummyDict);
    ObjFree(&dummyDict);

    lexer = gmalloc(sizeof(Lexer));
    LexerInitFromStream(lexer, subStr, xref);
    ParserInit(&parser, lexer);
    ParserGetObj(&parser, obj);
    ParserFree(&parser);

    ObjFree(&objStmObj);
}

/***********************************************************************
 *      XRefFetch
 ***********************************************************************
 * SYNOPSIS:        Fetch an object by number and generation.
 * PARAMETERS:      XRef *xref    cross-reference table
 *                  long num    number
 *                  long gen    gen
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

void XRefFetch(XRef *xref, long num, long gen, Obj *obj)
{
    XRefEntry *e;
    Parser parser;
    Obj obj1, obj2, obj3;
    word entrySize;
    short entryType;
    long entryOffset, entryGen;

    Stream *fs2;

    Lexer *lexer;

    /* check for bogus ref - this can happen in corrupted PDF files */
    if (num < 0 || num >= xref->size)
    {
        initNull(obj);
        return ;
    }

    /*
     * lock down just this one entry -- XRefFetchFromObjStm below recurses back
     * into XRefFetch , which locks its own entry independently.
     */

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
        initNull(&obj1);

        fs2 = gmalloc(sizeof(Stream));
        lexer = gmalloc(sizeof (Lexer));

        FStreamInit(fs2, xref->fHan, xref->start + entryOffset, -1, &obj1);
        LexerInitFromStream(lexer, fs2, xref);
        ParserInit(&parser, lexer);
        /*
         * See crypt.goh/ParserInit's comment for why every other Parser must
         * stay noCrypt.
         */
        parser.cryptNum = num;
        parser.cryptGen = gen;
        parser.noCrypt = !xref->encrypted;
        ObjFree(&obj1);

        ParserGetObj(&parser, &obj1);
        ParserGetObj(&parser, &obj2);
        ParserGetObj(&parser, &obj3);
        if (isInt(&obj1) && getInt(&obj1) == num &&
            isInt(&obj2) && getInt(&obj2) == gen &&
            isCmdSame(&obj3, "obj"))
        {
            ParserGetObj(&parser, obj);
        }
        else
        {
            initNull(obj);
        }
        ObjFree(&obj1);
        ObjFree(&obj2);
        ObjFree(&obj3);
        ParserFree(&parser);
    }
    else
    {
        initNull(obj);
    }
}

/***********************************************************************
 *      XRefGetDocInfo
 ***********************************************************************
 * SYNOPSIS:        Fetch the trailer Info dictionary.
 * PARAMETERS:      XRef *xref    cross-reference table
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

void XRefGetDocInfo(XRef *xref, Obj *obj)
{

    ObjDictLookup(&xref->trailerDict, "Info", obj, xref);
}

/***********************************************************************
 *      XRefIsOk
 ***********************************************************************
 * SYNOPSIS:        Return the cross-reference validity flag.
 * PARAMETERS:      XRef *xref    cross-reference table
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

GBool XRefIsOk(XRef *xref)
{
    return xref->ok;
}

/***********************************************************************
 *      XRefGetCatalog
 ***********************************************************************
 * SYNOPSIS:        Fetch the root catalog object.
 * PARAMETERS:      XRef *xref    cross-reference table
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

void XRefGetCatalog(XRef *xref, Obj *obj)
{

    XRefFetch(xref, xref->rootNum, xref->rootGen, obj);
}

/***********************************************************************
 *      XRefOkToPrint
 ***********************************************************************
 * SYNOPSIS:        Check whether printing is allowed.
 * PARAMETERS:      XRef *xref    cross-reference table
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

GBool XRefOkToPrint(XRef *xref)
{
    (void)xref;
    return gTrue;
}

/***********************************************************************
 *      XRefOkToCopy
 ***********************************************************************
 * SYNOPSIS:        Check whether copying is allowed.
 * PARAMETERS:      XRef *xref    cross-reference table
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

GBool XRefOkToCopy(XRef *xref)
{
    (void)xref;
    return gTrue;
}

