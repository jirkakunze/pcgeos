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

/***********************************************************************
 *      Growable record vector (movable block, locked while building)
 ***********************************************************************/

typedef struct
{
    MemHandle han;
    byte *data;
    word count;
    word alloc;
    word recSize;
    word maxCount;
} CidVec;

static void CidVecInit(CidVec *v, word recSize, word maxCount)
{
    v->han = NullHandle;
    v->data = NULL;
    v->count = v->alloc = 0;
    v->recSize = recSize;
    v->maxCount = maxCount;
}

/* RETURNS: pointer to the new (uninitialized) record, NULL if full/OOM */
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

/* Unlock and shrink to the used size; returns the handle (0 if empty). */
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

/***********************************************************************
 *      Builder state
 ***********************************************************************/

typedef struct
{
    CidVec entries;             /* GfxCidEntry */
    CidVec ranges;              /* GfxCidRange */
} CidBuilder;

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

/***********************************************************************
 *      Unicode -> GEOS char
 ***********************************************************************/

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

static byte CidUnicodeToChar(word u)
{
    byte ch = CidLigatureFromUnicode(u);

    if (ch == 0)
    {
        ch = (byte)GfxUnicodeToGeosChar(u);
    }
    return ch;
}

/*
 * A ToUnicode destination: UTF-16BE, one or more chars. Single chars
 * map directly; "ff", "fi", "fl", "ffi", "ffl" become ligature codes;
 * other sequences use their first char.
 */
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

/* GEOS char (or ligature) for index i, 0 <= i < CID_ALL_CHARS */
#define CID_ALL_CHARS (256 + 5)
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

/***********************************************************************
 *      /W
 ***********************************************************************/

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

/***********************************************************************
 *      /ToUnicode
 ***********************************************************************/

/* 1 or 2 byte source code of a 2 byte (Identity) codespace -> CID */
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

/*
 * <lo> <hi> <dst>: dst is the first destination, the last char is
 * incremented per code. For a single char only the codes whose value
 * is a GEOS char matter, so walk the GEOS chars instead of the range
 * (an <0000> <FFFF> <0000> range would be 64K lookups otherwise).
 */
static void CidMapContiguous(CidBuilder *b, word lo, word hi, Guchar *dst,
    word dstLen)
{
    word base, u, i, k;
    byte ch;
    Guchar seq[6];

    if (dstLen == 2)
    {
        base = ((word)dst[0] << 8) | dst[1];
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

/***********************************************************************
 *      Sorting / merging
 ***********************************************************************/

/* Shell sort by first word (cid / first); records of recSize bytes. */
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

/*
 * Sort the entries by CID and merge the width and char records of a
 * CID into one (the first width and the first char win).
 */
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

/***********************************************************************
 *      GfxCidFontInit
 ***********************************************************************
 * SYNOPSIS:        Set up a Type0 font.
 * PARAMETERS:      GfxFont *this    font (strings, flags initialized)
 *                  Dict *fontDict   the Type0 font dictionary
 *                  XRef *xref
 ***********************************************************************/
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

    /*
     * Substitute font: the descendant's FontDescriptor holds the flags,
     * the name mapping (same as for simple fonts) has the last word.
     */
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

/***********************************************************************
 *      GfxCidFontFree
 ***********************************************************************/
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

/***********************************************************************
 *      GfxCidFontLock / GfxCidFontUnlock
 ***********************************************************************/
void GfxCidFontLock(GfxFont *this, GfxCidLock *lock)
{
    lock->entries = this->cidMap ? MemLock(this->cidMap) : NULL;
    lock->entryCount = lock->entries ? this->cidCount : 0;
    lock->ranges = this->cidRanges ? MemLock(this->cidRanges) : NULL;
    lock->rangeCount = lock->ranges ? this->cidRangeCount : 0;
    lock->defaultWidth = this->cidDefaultWidth;
}

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

/***********************************************************************
 *      GfxCidFontLookup
 ***********************************************************************
 * SYNOPSIS:        Width and GEOS char of a CID.
 * STRATEGY:        Binary search in the entries; a width not given per
 *                  CID comes from the last range containing the CID
 *                  (ranges sorted by first CID), else /DW.
 ***********************************************************************/
void GfxCidFontLookup(GfxCidLock *lock, word cid, word *width, byte *ch)
{
    word lo, hi, mid;
    GfxCidEntry *e = NULL;

    *ch = 0;
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
