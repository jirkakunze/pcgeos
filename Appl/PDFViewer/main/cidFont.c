/***********************************************************************
 *
 *                      Copyright FreeGEOS-Project
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * PROJECT:       FreeGEOS
 * MODULE:        PDF Viewer
 * FILE:          main/cidFont.c
 *
 * DESCRIPTION:
 *      Type0 (composite) fonts: /Encoding /Identity-H with a CIDFontType0
 *      or CIDFontType2 descendant -- what Word, LibreOffice, browsers and
 *      most PDF libraries write for ordinary (western) text today. Other
 *      CMaps (Identity-V, the predefined CJK CMaps, embedded CMap
 *      streams) are not supported: the text is skipped, but advanced.
 *
 *      Like simple fonts, the text is drawn with a GEOS substitute font,
 *      so every CID needs
 *        - its width: /W (both forms) or /DW,
 *        - its GEOS char from /ToUnicode (bfchar, bfrange incl. the
 *          array form).
 *      The codes are glyph ids of the embedded font, without meaning of
 *      their own. Without /ToUnicode the text is not drawn (the
 *      position still advances by the widths). Reading the char
 *      mapping back from the embedded font file (TrueType cmap, CFF
 *      glyph names) was left out on purpose: rare in practice (every
 *      common producer writes /ToUnicode) and too expensive in real mode
 *      (~9 KB code, temporary 64 KB tables, decompressing the font).
 *
 *      Tables: GfxCidEntry records (cid, width, char) sorted by CID, and
 *      the "cFirst cLast w" ranges of /W, each in a movable block that is
 *      locked only while a string is drawn (GfxCidFontLock). Both are
 *      built unsorted and sorted once at the end.
 *
 *      Ligatures: Unicode FB00..FB04 and the sequences "ff", "fi", "fl",
 *      "ffi", "ffl" map to C_xx_LIGATURE, which doShowText draws as text.
 ***********************************************************************/

#include <geos.h>
#include <heap.h>
#include <ec.h>
#include <char.h>
#include <Ansi/string.h>
#include "obj.h"
#include "array.h"
#include "dict.h"
#include "stream.h"
#include "lexer.h"
#include "parser.h"
#include "xref.h"
#include "gfxFont.h"
#include "cidFont.h"
#include "gstr.h"
#include "gmem.h"

#ifdef __WATCOMC__
#pragma code_seg("CIDFont")
#endif

/* Upper limits of the tables (both fit into one 64 KB block). */
#define CID_MAX_ENTRIES 10000
#define CID_MAX_RANGES  10000
#define CID_GROW_STEP   256

/* bfrange with multi-char destination: largest range that is expanded */
#define CID_MAX_SEQ_RANGE 256

/* Unicode values of the ligature codes */
#define UNI_FF  0xFB00
#define UNI_FI  0xFB01
#define UNI_FL  0xFB02
#define UNI_FFI 0xFB03
#define UNI_FFL 0xFB04

/* Growable record vector in a movable block, locked while it is built */

typedef struct
{
    MemHandle han;
    byte *data;
    word count;
    word alloc;
    word recSize;
    word maxCount;
} CidVec;

/********************************************************************
 *                      CidVecInit
 ********************************************************************
 * SYNOPSIS:      Initialize an empty record vector.
 *
 * PARAMETERS:    *v                    Ptr to the vector.
 *                recSize               Record size in bytes.
 *                maxCount              Maximum number of records.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidVecInit(CidVec *v, word recSize, word maxCount)
{
    v->han = NullHandle;
    v->data = NULL;
    v->count = v->alloc = 0;
    v->recSize = recSize;
    v->maxCount = maxCount;
}

/********************************************************************
 *                      CidVecAppend
 ********************************************************************
 * SYNOPSIS:      Append a record to the vector.
 *
 * PARAMETERS:    *v                    Ptr to the vector.
 *
 * RETURNS:       void *                Ptr to the new record (not initialized),
 *                                      NULL if the vector is full or there is
 *                                      no memory.
 *
 * STRATEGY:      The block grows by CID_GROW_STEP records and stays locked
 *                while the tables are built.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void *CidVecAppend(CidVec *v)
{
    word newAlloc;

    if (v->count >= v->maxCount)
    {
        return NULL;
    }
    if (v->count == v->alloc)
    {
        newAlloc = v->alloc + CID_GROW_STEP;
        if (newAlloc > v->maxCount)
        {
            newAlloc = v->maxCount;
        }
        if (v->han == NullHandle)
        {
            v->han = MemAlloc(newAlloc * v->recSize, HF_SWAPABLE, HAF_NO_ERR);
            if (v->han == NullHandle)
            {
                EC_WARNING(-1);
                GMemSetError();
                return NULL;
            }
        }
        else
        {
            MemUnlock(v->han);
            if (!MemReAlloc(v->han, newAlloc * v->recSize, HAF_NO_ERR))
            {
                v->data = MemLock(v->han);
                EC_WARNING(-1);
                GMemSetError();
                return NULL;
            }
        }
        v->data = MemLock(v->han);
        v->alloc = newAlloc;
    }
    return v->data + (v->count++) * v->recSize;
}

/********************************************************************
 *                      CidVecFinish
 ********************************************************************
 * SYNOPSIS:      Unlock the vector and shrink it to the used size.
 *
 * PARAMETERS:    *v                    Ptr to the vector.
 *
 * RETURNS:       MemHandle             Handle of the block, 0 if the vector is
 *                                      empty.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static MemHandle CidVecFinish(CidVec *v)
{
    if (v->han == NullHandle)
    {
        return NullHandle;
    }
    MemUnlock(v->han);
    if (v->count == 0)
    {
        MemFree(v->han);
        v->han = NullHandle;
    }
    else if (v->count < v->alloc)
    {
        MemReAlloc(v->han, v->count * v->recSize, HAF_NO_ERR);
    }
    v->data = NULL;
    return v->han;
}

/********************************************************************
 *                      CidVecFree
 ********************************************************************
 * SYNOPSIS:      Free the block of the vector.
 *
 * PARAMETERS:    *v                    Ptr to the vector.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidVecFree(CidVec *v)
{
    if (v->han != NullHandle)
    {
        MemUnlock(v->han);
        MemFree(v->han);
    }
    v->han = NullHandle;
    v->data = NULL;
    v->count = 0;
}

/* State while the CID tables are built */

typedef struct
{
    CidVec entries;             /* GfxCidEntry */
    CidVec ranges;              /* GfxCidRange */
} CidBuilder;

/********************************************************************
 *                      CidAddWidth
 ********************************************************************
 * SYNOPSIS:      Add a width record for a CID.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                cid                   CID.
 *                width                 Width (1/1000 em).
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidAddWidth(CidBuilder *b, word cid, word width)
{
    GfxCidEntry *e = CidVecAppend(&b->entries);

    if (e)
    {
        e->cid = cid;
        e->width = width;
        e->ch = 0;
        e->flags = CIDE_HAS_WIDTH;
    }
}

/********************************************************************
 *                      CidAddChar
 ********************************************************************
 * SYNOPSIS:      Add a char record for a CID.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                cid                   CID.
 *                ch                    GEOS char; 0 adds nothing.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidAddChar(CidBuilder *b, word cid, byte ch)
{
    GfxCidEntry *e;

    if (ch == 0)
    {
        return;
    }
    e = CidVecAppend(&b->entries);
    if (e)
    {
        e->cid = cid;
        e->width = 0;
        e->ch = ch;
        e->flags = CIDE_HAS_CHAR;
    }
}

/********************************************************************
 *                      CidAddRange
 ********************************************************************
 * SYNOPSIS:      Add a "cFirst cLast w" width range.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                first                 First CID.
 *                last                  Last CID.
 *                width                 Width (1/1000 em).
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidAddRange(CidBuilder *b, word first, word last, word width)
{
    GfxCidRange *r = CidVecAppend(&b->ranges);

    if (r)
    {
        r->first = first;
        r->last = last;
        r->width = width;
    }
}

/********************************************************************
 *                      CidLigatureFromUnicode
 ********************************************************************
 * SYNOPSIS:      Return the GEOS ligature char of a Unicode ligature.
 *
 * PARAMETERS:    u                     Unicode value.
 *
 * RETURNS:       byte                  C_xx_LIGATURE, 0 if u is not a ligature.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static byte CidLigatureFromUnicode(word u)
{
    switch (u)
    {
        case UNI_FF:  return C_FF_LIGATURE;
        case UNI_FI:  return C_FI_LIGATURE;
        case UNI_FL:  return C_FL_LIGATURE;
        case UNI_FFI: return C_FFI_LIGATURE;
        case UNI_FFL: return C_FFL_LIGATURE;
    }
    return 0;
}

/********************************************************************
 *                      CidUnicodeToChar
 ********************************************************************
 * SYNOPSIS:      Return the GEOS char of a Unicode value.
 *
 * PARAMETERS:    u                     Unicode value.
 *
 * RETURNS:       byte                  GEOS char or ligature, 0 if there is
 *                                      none.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static byte CidUnicodeToChar(word u)
{
    byte ch = CidLigatureFromUnicode(u);

    if (ch == 0)
    {
        ch = (byte)GfxUnicodeToGeosChar(u);
    }
    return ch;
}

/********************************************************************
 *                      CidSeqToChar
 ********************************************************************
 * SYNOPSIS:      Return the GEOS char of a /ToUnicode destination.
 *
 * PARAMETERS:    *p                    Ptr to the UTF-16BE destination.
 *                len                   Length in bytes.
 *
 * RETURNS:       byte                  GEOS char, 0 if there is none.
 *
 * STRATEGY:      "ff", "fi", "fl", "ffi", "ffl" become ligature chars; other
 *                sequences use their first char.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static byte CidSeqToChar(const Guchar *p, word len)
{
    word u[3];
    word n = 0;

    while (n < 3 && len >= 2)
    {
        u[n++] = ((word)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (n == 0)
    {
        return 0;
    }
    if (len == 0 && n >= 2 && u[0] == 'f')
    {
        if (n == 2)
        {
            if (u[1] == 'f') return C_FF_LIGATURE;
            if (u[1] == 'i') return C_FI_LIGATURE;
            if (u[1] == 'l') return C_FL_LIGATURE;
        }
        else if (u[1] == 'f')
        {
            if (u[2] == 'i') return C_FFI_LIGATURE;
            if (u[2] == 'l') return C_FFL_LIGATURE;
        }
    }
    return CidUnicodeToChar(u[0]);
}

#define CID_ALL_CHARS (256 + 5)
/********************************************************************
 *                      CidIndexToUnicode
 ********************************************************************
 * SYNOPSIS:      Return the Unicode value of GEOS char index i.
 *
 * PARAMETERS:    i                     Index: 0-255 GEOS chars, 256-260
 *                                      ligatures (CID_ALL_CHARS).
 *                *ch                   Ptr to the GEOS char (out).
 *
 * RETURNS:       word                  Unicode value, 0 for control codes.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static word CidIndexToUnicode(word i, byte *ch)
{
    static const word ligUni[5] = {UNI_FF, UNI_FI, UNI_FL, UNI_FFI, UNI_FFL};

    if (i < 256)
    {
        *ch = (byte)i;
        return GfxGeosCharToUnicode((byte)i);
    }
    *ch = CidLigatureFromUnicode(ligUni[i - 256]);
    return ligUni[i - 256];
}

/********************************************************************
 *                      CidReadWidths
 ********************************************************************
 * SYNOPSIS:      Read the /W widths of a CID font.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                *desc                 Ptr to the descendant font dictionary.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Both forms: "c [w1 w2 ...]" gives width records, "cFirst cLast
 *                w" gives a range.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidReadWidths(CidBuilder *b, Dict *desc, XRef *xref)
{
    Obj wObj, aObj, bObj, cObj;
    word n, i, j, len;
    long first, last;

    DictLookup(desc, "W", &wObj, xref);
    if (!isArray(&wObj))
    {
        ObjFree(&wObj);
        return;
    }
    n = ObjArrayGetLength(&wObj);
    i = 0;
    while (i + 1 < n && !GMemHadError())
    {
        ObjArrayGet(&wObj, i, &aObj, xref);
        ObjArrayGet(&wObj, i + 1, &bObj, xref);
        if (!isInt(&aObj))
        {
            ObjFree(&aObj);
            ObjFree(&bObj);
            break;
        }
        first = getInt(&aObj);
        ObjFree(&aObj);
        if (isArray(&bObj))
        {
            /* c [w1 w2 ...] */
            len = ObjArrayGetLength(&bObj);
            for (j = 0; j < len; ++j)
            {
                ObjArrayGet(&bObj, j, &cObj, xref);
                if (isNum(&cObj) && first + j >= 0 && first + j <= 0xFFFFL)
                {
                    CidAddWidth(b, (word)(first + j),
                        (word)GdoubleToWord(getNum(&cObj)));
                }
                ObjFree(&cObj);
            }
            ObjFree(&bObj);
            i += 2;
        }
        else if (isInt(&bObj) && i + 2 < n)
        {
            /* cFirst cLast w */
            last = getInt(&bObj);
            ObjFree(&bObj);
            ObjArrayGet(&wObj, i + 2, &cObj, xref);
            if (isNum(&cObj) && first >= 0 && last >= first && last <= 0xFFFFL)
            {
                CidAddRange(b, (word)first, (word)last,
                    (word)GdoubleToWord(getNum(&cObj)));
            }
            ObjFree(&cObj);
            i += 3;
        }
        else
        {
            ObjFree(&bObj);
            break;
        }
    }
    ObjFree(&wObj);
}

/********************************************************************
 *                      CidStringToCode
 ********************************************************************
 * SYNOPSIS:      Return the code of a CMap source string.
 *
 * PARAMETERS:    *obj                  Ptr to the string object (1 or 2 bytes).
 *
 * RETURNS:       long                  Code, -1 for other lengths.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static long CidStringToCode(Obj *obj)
{
    GooString *str = getString(obj);
    Guchar *p = (Guchar *)GStrGetCString(str);
    word len = GStrGetLength(str);

    if (len == 1)
    {
        return p[0];
    }
    if (len == 2)
    {
        return ((word)p[0] << 8) | p[1];
    }
    return -1;
}

/********************************************************************
 *                      CidParseBfChar
 ********************************************************************
 * SYNOPSIS:      Read the entries of a bfchar section.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                *parser               Ptr to the parser after "beginbfchar".
 *
 * RETURNS:       void
 *
 * STRATEGY:      The destination is a UTF-16BE string or, in old CMaps, a glyph
 *                name.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidParseBfChar(CidBuilder *b, Parser *parser)
{
    Obj codeObj, dstObj;
    long code;
    GooString *dst;

    ParserGetObj(parser, &codeObj);
    while (!isEOF(&codeObj) && !isCmdSame(&codeObj, "endbfchar"))
    {
        if (!isString(&codeObj))
        {
            ObjFree(&codeObj);
            return;
        }
        code = CidStringToCode(&codeObj);
        ObjFree(&codeObj);

        ParserGetObj(parser, &dstObj);
        if (isString(&dstObj) && code >= 0)
        {
            dst = getString(&dstObj);
            CidAddChar(b, (word)code, CidSeqToChar(
                (Guchar *)GStrGetCString(dst), GStrGetLength(dst)));
        }
        else if (isName(&dstObj) && code >= 0)
        {
            /* older CMaps: glyph name as destination */
            CidAddChar(b, (word)code, GfxGlyphNameToGeosChar(getName(&dstObj)));
        }
        ObjFree(&dstObj);
        ParserGetObj(parser, &codeObj);
    }
    ObjFree(&codeObj);
}

/********************************************************************
 *                      CidMapContiguous
 ********************************************************************
 * SYNOPSIS:      Map a bfrange with one destination string.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                lo                    First code.
 *                hi                    Last code.
 *                *dst                  Ptr to the destination of the first
 *                                      code.
 *                dstLen                Length of dst in bytes.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The last char of dst counts up per code. For one char only
 *                codes whose value is a GEOS char matter: walk the GEOS chars,
 *                not the range (<0000> <FFFF> would be 64K steps). Multi-char
 *                ranges are cut at CID_MAX_SEQ_RANGE.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidMapContiguous(CidBuilder *b, word lo, word hi, Guchar *dst,
    word dstLen)
{
    word base, u, i, k;
    byte ch;
    Guchar seq[6];

    if (dstLen == 2)
    {
        base = ((word)dst[0] << 8) | dst[1];
        /* Walk the GEOS chars, not the range */
        for (i = 0; i < CID_ALL_CHARS; ++i)
        {
            u = CidIndexToUnicode(i, &ch);
            if (ch != 0 && u != 0 && u >= base && (u - base) <= (hi - lo))
            {
                CidAddChar(b, lo + (u - base), ch);
            }
        }
    }
    else if (dstLen >= 4 && dstLen <= 6)
    {
        memcpy(seq, dst, dstLen);
        /* Multi-char destination: the last byte counts up */
        for (k = 0; k <= hi - lo && k < CID_MAX_SEQ_RANGE; ++k)
        {
            CidAddChar(b, lo + k, CidSeqToChar(seq, dstLen));
            if (++seq[dstLen - 1] == 0)
            {
                ++seq[dstLen - 2];
            }
        }
    }
}

/********************************************************************
 *                      CidParseBfRange
 ********************************************************************
 * SYNOPSIS:      Read the entries of a bfrange section.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                *parser               Ptr to the parser after "beginbfrange".
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The destination is one string (counts up) or an array of
 *                strings.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidParseBfRange(CidBuilder *b, Parser *parser, XRef *xref)
{
    Obj loObj, hiObj, dstObj, elemObj;
    long lo, hi;
    word i, n;
    GooString *dst;

    ParserGetObj(parser, &loObj);
    while (!isEOF(&loObj) && !isCmdSame(&loObj, "endbfrange"))
    {
        if (!isString(&loObj))
        {
            ObjFree(&loObj);
            return;
        }
        lo = CidStringToCode(&loObj);
        ObjFree(&loObj);

        ParserGetObj(parser, &hiObj);
        if (!isString(&hiObj))
        {
            ObjFree(&hiObj);
            return;
        }
        hi = CidStringToCode(&hiObj);
        ObjFree(&hiObj);

        ParserGetObj(parser, &dstObj);
        if (lo >= 0 && hi >= lo)
        {
            if (isString(&dstObj))
            {
                dst = getString(&dstObj);
                CidMapContiguous(b, (word)lo, (word)hi,
                    (Guchar *)GStrGetCString(dst), GStrGetLength(dst));
            }
            else if (isArray(&dstObj))
            {
                n = ObjArrayGetLength(&dstObj);
                for (i = 0; i < n && i <= (word)(hi - lo); ++i)
                {
                    ObjArrayGet(&dstObj, i, &elemObj, xref);
                    if (isString(&elemObj))
                    {
                        dst = getString(&elemObj);
                        CidAddChar(b, (word)lo + i, CidSeqToChar(
                            (Guchar *)GStrGetCString(dst),
                            GStrGetLength(dst)));
                    }
                    ObjFree(&elemObj);
                }
            }
        }
        ObjFree(&dstObj);
        ParserGetObj(parser, &loObj);
    }
    ObjFree(&loObj);
}

/********************************************************************
 *                      CidReadToUnicode
 ********************************************************************
 * SYNOPSIS:      Read the /ToUnicode CMap of a CID font.
 *
 * PARAMETERS:    *b                    Ptr to the table builder.
 *                *fontDict             Ptr to the Type0 font dictionary.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Parse the CMap with the normal PDF parser and read only the
 *                bfchar and bfrange sections.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidReadToUnicode(CidBuilder *b, Dict *fontDict, XRef *xref)
{
    Obj streamObj, tokObj;
    Lexer *lexer;
    Parser parser;

    DictLookup(fontDict, "ToUnicode", &streamObj, xref);
    if (!isStream(&streamObj))
    {
        /* also /Identity-H as a name: no information */
        ObjFree(&streamObj);
        return;
    }

    lexer = gmalloc(sizeof(Lexer));
    if (!lexer)
    {
        ObjFree(&streamObj);
        return;
    }
    LexerInitFromStream(lexer, getStream(&streamObj), xref);
    if (GMemHadError())
    {
        LexerFree(lexer);
        gfree(lexer);
        return;
    }
    ParserInit(&parser, lexer);

    ParserGetObj(&parser, &tokObj);
    while (!isEOF(&tokObj) && !GMemHadError())
    {
        if (isCmdSame(&tokObj, "beginbfchar"))
        {
            ObjFree(&tokObj);
            CidParseBfChar(b, &parser);
        }
        else if (isCmdSame(&tokObj, "beginbfrange"))
        {
            ObjFree(&tokObj);
            CidParseBfRange(b, &parser, xref);
        }
        else
        {
            ObjFree(&tokObj);
        }
        ParserGetObj(&parser, &tokObj);
    }
    ObjFree(&tokObj);

    /* LexerInitFromStream took ownership of streamObj's stream. */
    ParserFree(&parser);
}

/********************************************************************
 *                      CidSortByFirstWord
 ********************************************************************
 * SYNOPSIS:      Sort records by their first word (CID or first CID).
 *
 * PARAMETERS:    *data                 Ptr to the records.
 *                count                 Number of records.
 *                recSize               Record size in bytes.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Shell sort: small code, no recursion.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidSortByFirstWord(byte *data, word count, word recSize)
{
    word gap, i, j;
    byte tmp[sizeof(GfxCidEntry) > sizeof(GfxCidRange) ?
             sizeof(GfxCidEntry) : sizeof(GfxCidRange)];

    for (gap = count / 2; gap > 0; gap /= 2)
    {
        for (i = gap; i < count; ++i)
        {
            memcpy(tmp, data + i * recSize, recSize);
            for (j = i; j >= gap &&
                *(word *)(data + (j - gap) * recSize) > *(word *)tmp;
                j -= gap)
            {
                memcpy(data + j * recSize, data + (j - gap) * recSize,
                    recSize);
            }
            memcpy(data + j * recSize, tmp, recSize);
        }
    }
}

/********************************************************************
 *                      CidMergeEntries
 ********************************************************************
 * SYNOPSIS:      Sort the CID entries and merge the records of each CID.
 *
 * PARAMETERS:    *v                    Ptr to the entry vector.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The first width and the first char of a CID win.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void CidMergeEntries(CidVec *v)
{
    GfxCidEntry *e = (GfxCidEntry *)v->data;
    word in, out;

    if (v->count == 0)
    {
        return;
    }
    CidSortByFirstWord(v->data, v->count, sizeof(GfxCidEntry));
    out = 0;
    for (in = 1; in < v->count; ++in)
    {
        if (e[in].cid == e[out].cid)
        {
            if (!(e[out].flags & CIDE_HAS_WIDTH) &&
                (e[in].flags & CIDE_HAS_WIDTH))
            {
                e[out].width = e[in].width;
                e[out].flags |= CIDE_HAS_WIDTH;
            }
            if (!(e[out].flags & CIDE_HAS_CHAR) &&
                (e[in].flags & CIDE_HAS_CHAR))
            {
                e[out].ch = e[in].ch;
                e[out].flags |= CIDE_HAS_CHAR;
            }
        }
        else
        {
            e[++out] = e[in];
        }
    }
    v->count = out + 1;
}

/********************************************************************
 *                      GfxCidFontInit
 ********************************************************************
 * SYNOPSIS:      Set up a Type0 font (Identity-H only).
 *
 * PARAMETERS:    *this                 Ptr to the font.
 *                *fontDict             Ptr to the Type0 font dictionary.
 *                *xref                 Ptr to the cross-reference table.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Read /DW, the flags of the descendant font descriptor, /W and
 *                /ToUnicode into the CID tables. Other encodings are not
 *                supported: the text is not drawn.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void GfxCidFontInit(GfxFont *this, Dict *fontDict, XRef *xref)
{
    Obj encObj, descArr, descObj, fdObj, obj;
    Dict *desc;
    CidBuilder b;

    this->is16 = gTrue;
    /* text comes out as GEOS chars, never as the Symbol font */
    this->flags &= ~fontSymbolic;

    DictLookup(fontDict, "Encoding", &encObj, xref);
    if (!isNameSame(&encObj, "Identity-H"))
    {
        /* Identity-V, CJK CMaps, embedded CMaps: not supported */
        EC_WARNING(-1);
        ObjFree(&encObj);
        return;
    }
    ObjFree(&encObj);

    DictLookup(fontDict, "DescendantFonts", &descArr, xref);
    if (!isArray(&descArr) || ObjArrayGetLength(&descArr) < 1)
    {
        ObjFree(&descArr);
        return;
    }
    ObjArrayGet(&descArr, 0, &descObj, xref);
    ObjFree(&descArr);
    if (!isDict(&descObj))
    {
        ObjFree(&descObj);
        return;
    }
    desc = getDict(&descObj);

    DictLookup(desc, "DW", &obj, xref);
    if (isNum(&obj))
    {
        this->cidDefaultWidth = (word)GdoubleToWord(getNum(&obj));
    }
    ObjFree(&obj);

    /* Substitute font: flags of the descendant, then the name mapping */
    DictLookup(desc, "FontDescriptor", &fdObj, xref);
    if (isDict(&fdObj))
    {
        DictLookup(getDict(&fdObj), "Flags", &obj, xref);
        if (isInt(&obj))
        {
            this->flags = getInt(&obj);
        }
        ObjFree(&obj);
    }
    ObjFree(&fdObj);
    GfxFontMapName(this, GStrGetCString(&this->name));
    this->flags &= ~fontSymbolic;

    CidVecInit(&b.entries, sizeof(GfxCidEntry), CID_MAX_ENTRIES);
    CidVecInit(&b.ranges, sizeof(GfxCidRange), CID_MAX_RANGES);
    CidReadWidths(&b, desc, xref);
    CidReadToUnicode(&b, fontDict, xref);
    if (GMemHadError())
    {
        CidVecFree(&b.entries);
        CidVecFree(&b.ranges);
    }
    else
    {
        CidMergeEntries(&b.entries);
        if (b.ranges.count > 1)
        {
            CidSortByFirstWord(b.ranges.data, b.ranges.count,
                sizeof(GfxCidRange));
        }
        this->cidCount = b.entries.count;
        this->cidMap = CidVecFinish(&b.entries);
        this->cidRangeCount = b.ranges.count;
        this->cidRanges = CidVecFinish(&b.ranges);
    }

    ObjFree(&descObj);
}

/********************************************************************
 *                      GfxCidFontFree
 ********************************************************************
 * SYNOPSIS:      Free the CID tables of a font.
 *
 * PARAMETERS:    *this                 Ptr to the font.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void GfxCidFontFree(GfxFont *this)
{
    if (this->cidMap)
    {
        MemFree(this->cidMap);
        this->cidMap = NullHandle;
    }
    if (this->cidRanges)
    {
        MemFree(this->cidRanges);
        this->cidRanges = NullHandle;
    }
    this->cidCount = this->cidRangeCount = 0;
}

/********************************************************************
 *                      GfxCidFontLock
 ********************************************************************
 * SYNOPSIS:      Lock the CID tables for drawing a string.
 *
 * PARAMETERS:    *this                 Ptr to the font.
 *                *lock                 Ptr to the lock data (out).
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void GfxCidFontLock(GfxFont *this, GfxCidLock *lock)
{
    lock->entries = this->cidMap ? MemLock(this->cidMap) : NULL;
    lock->entryCount = lock->entries ? this->cidCount : 0;
    lock->ranges = this->cidRanges ? MemLock(this->cidRanges) : NULL;
    lock->rangeCount = lock->ranges ? this->cidRangeCount : 0;
    lock->defaultWidth = this->cidDefaultWidth;
}

/********************************************************************
 *                      GfxCidFontUnlock
 ********************************************************************
 * SYNOPSIS:      Unlock the CID tables.
 *
 * PARAMETERS:    *this                 Ptr to the font.
 *
 * RETURNS:       void
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void GfxCidFontUnlock(GfxFont *this)
{
    if (this->cidMap)
    {
        MemUnlock(this->cidMap);
    }
    if (this->cidRanges)
    {
        MemUnlock(this->cidRanges);
    }
}

/********************************************************************
 *                      GfxCidFontLookup
 ********************************************************************
 * SYNOPSIS:      Return the width and the GEOS char of a CID.
 *
 * PARAMETERS:    *lock                 Ptr to the locked tables.
 *                cid                   CID.
 *                *width                Ptr to the width (out).
 *                *ch                   Ptr to the GEOS char (out), 0 if there
 *                                      is none.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Binary search in the entries, then in the ranges, else /DW.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void GfxCidFontLookup(GfxCidLock *lock, word cid, word *width, byte *ch)
{
    word lo, hi, mid;
    GfxCidEntry *e = NULL;

    *ch = 0;
    /* Binary search for the CID in the entries */
    lo = 0;
    hi = lock->entryCount;
    while (lo < hi)
    {
        mid = (lo + hi) / 2;
        if (lock->entries[mid].cid < cid)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo < lock->entryCount && lock->entries[lo].cid == cid)
    {
        e = &lock->entries[lo];
        *ch = e->ch;
        if (e->flags & CIDE_HAS_WIDTH)
        {
            *width = e->width;
            return;
        }
    }

    /* last range with first <= cid */
    lo = 0;
    hi = lock->rangeCount;
    while (lo < hi)
    {
        mid = (lo + hi) / 2;
        if (lock->ranges[mid].first <= cid)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    /* ranges normally don't overlap: the closest one decides */
    if (lo > 0 && lock->ranges[lo - 1].last >= cid)
    {
        *width = lock->ranges[lo - 1].width;
        return;
    }
    *width = lock->defaultWidth;
}
