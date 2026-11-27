/***********************************************************************
 *
 *                      Copyright FreeGEOS-Project
 *              Portions Copyright (c) GlobalPC 1999
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * PROJECT:       FreeGEOS
 * MODULE:        PDF Viewer
 * FILE:          catalog.c
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
 *
 ***********************************************************************/

#ifndef PDFGEODE_H
#define PDFGEODE_H

#include <geos.h>
#include <file.h>
#include "gtypes.h"

/*
 * There's round-off error in setting the current point after drawing very
 * small (e.g.
 */

#define FONT_SCALING_FACTOR 32

/* Keep batches below the HugeArray path affected by InitHAChain. */
#define HUGEARRAY_BATCH_MAX_BYTES 2000

typedef struct GooString
{
    long length;
    char *s;
} GooString;

typedef struct Ref
{
    long num; /* object number */
    long gen; /* generation number */
} Ref;

/* object types */

typedef enum ObjType
{
    /* simple objects */
    objBool, /* boolean */
        objInt, /* integer */
        objReal, /* real */
        objString, /* string */
        objName, /* name */
        objNull, /* null */

    /* complex objects */
        objArray, /* array */
        objDict, /* dictionary */
        objStream, /* stream */
        objRef, /* indirect reference */

    /* special objects */
        objCmd, /* command name */
        objError, /* error return from Lexer */
        objEOF, /* end of file return from Lexer */
        objNone /* uninitialized object */
} ObjType;

#define numObjTypes 14  /* total number of object types */

/* Object */

typedef struct Obj
{

    ObjType type; /* object type */
    union
    { /* value for each type: */
        GBool booln; /* boolean */
        long intg; /* integer */
        gdouble real; /* real */
        GooString string; /* string */
        char *name; /* name */
        struct Array *array; /* array */
        struct Dict *dict; /* dictionary */
        struct Stream *stream; /* stream */
        Ref ref; /* indirect reference */
        char *cmd; /* command */
    } u;
} Obj;

/* Array */

typedef struct Array
{

    Obj *elems; /* array of elements */
    word size; /* size of <elems> array, max. ARRAY_MAX_ELEMS */
    word length; /* number of elements in array */
    word ref; /* reference count */

} Array;

/* Catalog */

typedef struct Catalog
{
    Obj pagesRoot; /* top-level /Pages ref or direct dict */
    word numPages; /* number of pages, max. PDF_MAX_PAGE_COUNT */
    Obj dests; /* named destination dictionary */
    Obj nameTree; /* name tree */
    GBool ok; /* true if catalog is valid */

} Catalog;

/* Dict */

typedef struct DictEntry
{
    char *key;
    Obj val;
} DictEntry;

typedef struct Dict
{
    DictEntry *entries; /* array of entries */
    word size; /* size of <entries> array, max. DICT_MAX_ENTRIES */
    word length; /* number of entries in dictionary */
    word ref; /* reference count */

} Dict;

/* PageAttrs */

typedef struct PageAttrs
{

#ifdef USE_FULL_PAGE_ATTRS
    gdouble x1, y1, x2, y2;
    gdouble cropX1, cropY1, cropX2, cropY2;
    sword rotate; /* 0, 90, 180 or 270 */
#endif

    Obj resources;
} PageAttrs;

typedef struct Page
{
    word num; /* page number */
    PageAttrs attrs; /* page attributes */
    Obj annots; /* annotations array */
    Obj contents; /* page contents */
    GBool ok; /* true if page is valid */
} Page;

/* Parser */

typedef struct Parser
{
    Obj buf1, buf2; /* next two tokens */
    short inlineImg; /* set when inline image data is encountered */
    struct Lexer *lexer;

    /*
     * noCrypt (the safe default set by ParserInit) covers every other Parser
     * in the codebase.
     */
    GBool noCrypt;
    long cryptNum, cryptGen;

} Parser;

/*
 * One entry per object, kept in a HugeArray in the scratch VM file. Keep it
 * small (8 bytes): documents can have tens of thousands of objects.
 */
typedef struct XRefEntry
{
    long offset;    /* file offset; compressed: number of the ObjStm */
    word gen;       /* generation (max. 65535); compressed: index in ObjStm */
    byte used;      /* gTrue or gFalse */
    /*
     * 0 = free/unused, 1 = classic , 2 = compressed inside an Object Stream
     * (PDF 1.5+ cross-reference streams).
     */
    byte type;
} XRefEntry;

#define xrefEntryFree 0
#define xrefEntryClassic 1
#define xrefEntryCompressed 2

typedef struct XRef
{
    FileHandle fHan;
    VMFileHandle vmFile;
    char vmFileName[PATH_LENGTH_ZT];

    long start; /* offset in file(to allow for garbage */
    /* at beginning of file) */
    VMBlockHandle entries;
    long size; /* size of <entries> array */
    long rootNum, rootGen; /* catalog dict */
    GBool ok; /* true if xref table is valid */
    Obj trailerDict; /* trailer dictionary */

    /*
     * `encrypted` is set once XRefSetupEncryption has verified the empty user
     * password actually opens the file (PDF spec Algorithm 6).
     */
    GBool encrypted;
    GBool needPassword; /* supported encryption, password did not match */
    Guchar fileKey[16];
    short fileKeyLen;
    short encryptRevision;
    long permissions; /* Standard Security Handler /P flags */
    GBool ownerAccess; /* opened with the owner password: no restrictions */

    /*
     * One-entry Object Stream header cache. Keeping it here gives the cache
     * exactly the lifetime of its owning document.
     */
    long objStmCacheNum;
    long objStmCacheN;
    long objStmCacheFirst;
    MemHandle objStmPairOffsetsH; /* long[objStmCacheN], swapable, 0 = none */
} XRef;

/* Lexer */

/* #define tokBufSize 128 // size of token buffer */

typedef struct Lexer
{
    Array *streams; /* array of input streams */
    word strPtr; /* index of current stream */
    Obj curStr; /* current stream */
    GBool freeArray; /* should lexer free the streams array? */

    XRef *xref;
} Lexer;

/* GfxState */

/* Geos needs to be told when a path is starting, and when to set a clip path */

typedef enum PathType
{
    PATH_NONE,
        PATH_NORMAL,
        PATH_CLIP,
        PATH_CLIP_EO,
} PathType;

/* GfxColor */

typedef struct GfxColor
{
    short r, g, b;
} GfxColor;

/* GfxColorSpace */

typedef enum GfxColorMode
{
    colorGray,
        colorCMYK,
        colorRGB
} GfxColorMode;

typedef struct GfxColorSpace
{

    GfxColorMode mode; /* color mode */
    GBool indexed; /* set for indexed colorspaces */
    short numComps; /* number of components in colors */
    short indexHigh; /* max pixel for indexed colorspace */
    Guchar(*lookup)[4]; /* lookup table(only for indexed */
    /* colorspaces) */
    GBool ok; /* is color space valid? */

    /* This is now only the FALLBACK for cases we can't evaluate for real . */
    GBool tintApprox;

    /*
     * numComps stays 1 here ; altMode/altNumComps describe the REAL alternate
     * colorspace the function's output lands in.
     */
    GBool hasFunction;
    GfxColorMode altMode;
    short altNumComps;
    gdouble funcC0[4];
    gdouble funcC1[4];
    gdouble funcN;

} GfxColorSpace;

typedef struct GfxState
{

    /*
     * Only wrinkle: an Indexed colorspace's `lookup` table pointer would be
     * shallow-copied (shared, not duplicated) across a q/Q pair.
     */
    GfxColorSpace fillColorSpace; /* fill color space */
    GfxColorSpace strokeColorSpace; /* stroke color space */
    GfxColor fillColor; /* current fill color  */

    /*
     * Constant opacity from an ExtGState's /ca (fill) and /CA (stroke) -- see
     * opSetExtGState and GfxAlphaMask in main/gfx.goc (dither masks).
     */
    gdouble fillAlpha;
    gdouble strokeAlpha;

    /* double miterLimit; // line miter limit */

    struct GfxFont *font; /* font */
    gdouble fontSize; /* font size */
    gdouble textMat[6]; /* text matrix */
    sdword charSpace; /* character spacing */
    gdouble wordSpace; /* word spacing */
    sdword horizScaling; /* horizontal scaling */
    gdouble leading; /* text leading */
    sdword rise; /* text rise */
    short render; /* text rendering mode */

    gdouble lineX, lineY; /* start of current text line(text coords) */
    WWFixedAsDWord curTextX; /* current drawing position on x-axis */

    struct GfxState *saved; /* next GfxState on stack */

    Handle gstring; /* gstring we're writing to */
    PathType pathType; /* whether there's a current path or clipping */

    /*
     * Needed because GrDrawLineTo/GrDrawCurveTo/GrDrawRect only take integer
     * (sword) coordinates.
     */
    gdouble pathRawMinX, pathRawMinY, pathRawMaxX, pathRawMaxY;
    GBool pathRawBoundsValid;

    /* PDF fill closes each open subpath implicitly. */
    sword pathSubpathStartX, pathSubpathStartY;
    GBool pathSubpathStartValid;

    /*
     * What GEOS keeps to itself: the CTM (user space -> page space,
     * [a b c d e f]) and a page space box around the current clip, for the
     * replacement fill of 'sh' (opShFill).
     */
    gdouble ctm[6];
    gdouble clipBox[4];

} GfxState;

/* GfxFontDict */

typedef struct GfxFontDict
{
    struct GfxFont **fonts; /* list of fonts */
    word numFonts; /* number of fonts */
    /*
     * 1-entry "last lookup" cache : GfxFontDictLookup runs a linear, string-
     * comparing scan over every font in the page's /Resources on EVERY /Tf
     * operator.
     */
    struct GfxFont *lastFont; /* NULL until the first successful */
    /* lookup */
} GfxFontDict;

/* GfxFont */

#define fontFixedWidth (1L << 0)
#define fontSerif (1L << 1)
#define fontSymbolic (1L << 2)
#define fontItalic (1L << 6)
#define fontBold (1L << 18)

typedef enum GfxFontType
{
    fontUnknownType,
        fontType1,
        fontType3,
        fontTrueType,
        fontType0
} GfxFontType;

typedef struct GfxFont
{
    GooString tag; /* PDF font tag */
    Ref id;
    GooString name; /* font name */
    long flags; /* font descriptor flags */
    GfxFontType type; /* type of font */
    Ref embFontID; /* ref to embedded font file stream */

    GBool is16; /* set if font uses 16-bit chars */
    GBool hasCharSet; /* true if font descriptor has 'charset' entry */
    sdword fontWidthFactor; /* Used to tweak font width based on char widths */

    /*
     * Type0 fonts (is16): tables built by GfxCidFontInit (main/cidFont.c),
     * movable blocks locked only while a string is drawn.
     */
    MemHandle cidMap; /* GfxCidEntry[cidCount], 0 = none */
    word cidCount;
    MemHandle cidRanges; /* GfxCidRange[cidRangeCount], 0 = none */
    word cidRangeCount;
    word cidDefaultWidth; /* /DW */

    unsigned short widths[256]; /* char widths(x1000) */
    unsigned char charMap[256]; /* char encoding map */
    /*
     * Bit set: charMap holds a code of the GEOS Symbol font (Greek letters
     * missing in the GEOS character set), see GfxFontSymbolChar.
     */
    byte symChars[32];
    /*
     * Vertical offset of every glyph, 1/1000 em (0 = none). TeX CMEX glyphs
     * hang below the baseline, the substitute glyphs stand on it.
     */
    short charRise;
    /* Widths are computed inline; charNames[256] is no longer needed. */

} GfxFont;

typedef struct GfxResources
{
    GfxFontDict *fonts;
    Obj xObjDict;
    Obj colorSpaceDict;
    Obj extGStateDict;
    Obj patternDict;
    Obj shadingDict;
    struct GfxResources *next;
} GfxResources;

typedef struct Gfx
{

    GfxResources *res; /* resource stack */

    GfxState *state; /* current graphics state */
    GBool fontChanged; /* set if font or text matrix has changed */
    GBool textClip;    /* clip text (Tr 4-7) in the kernel path until ET */
    word saveDepth;    /* open q levels (GFX_MAX_SAVE_DEPTH) */
    word skippedSaves; /* q beyond the limit, matched by Q */

    int ignoreUndef; /* current BX/EX nesting level */

    Parser *parser; /* parser for page content stream(s) */
    XRef *xref; /* current xref pointer */
    VMFileHandle vmFile; /* scratch VM file */
    word nImages; /* # of images drawn so far on this page */
    GBool resourceLimitExceeded; /* OPT-15: page image guard fired */

    dword pageResourceBytes; /* image bytes copied into the page GString */

    void(*progressCallback)(dword kind, dword current, dword total,
        void *userData);
    void *progressUserData;
    GBool *cancelFlag;
    word formDepth; /* current Form XObject nesting level */
    void *pathBuf; /* stroke path buffer (GfxPathBuf, gfx.goc), lazily allocated */
} Gfx;

/* GfxPathBuf entry types */
#define GFX_SEG_MOVE  0
#define GFX_SEG_LINE  1
#define GFX_SEG_CURVE 2
#define GFX_SEG_CLOSE 3

/* GfxImageColorMap */

typedef struct GfxImageColorMap
{

    GfxColorSpace *colorSpace; /* the image colorspace */
    short bits; /* bits per component */
    short numComps; /* number of components in a pixel */
    GBool indexed; /* set for indexed color space */
    GfxColorMode mode; /* color mode */
    short(*lookup)[4]; /* lookup table */
    gdouble decodeLow[4]; /* minimum values for each component */
    gdouble decodeRange[4]; /* max - min value for each component */
    GBool ok;
    GBool isIdentity;

} GfxImageColorMap;

typedef enum StreamKind
{
    strFile,
        strASCIIHex,
        strASCII85,
        strLZW,
        strRunLength,
        strCCITTFax,
        strDCT,
        strFlate,
        strEOF,
        strWeird, /* internal-use stream types */
        strSubStream,
        strRC4,
        strPredictor /* PNG/TIFF predictor behind Flate/LZW */
} StreamKind;

typedef struct Stream
{
    word ref; /* reference count */

    StreamKind kind;

    struct Stream *str; /* pointer to generic stream feeding this stream */

    /* ----- image stuff (StreamResetImage/StreamGetImagePixel) */
    short nComps; /* components per pixel */
    short nBits; /* bits per component */
    word width; /* pixels per line */
    word nVals; /* components per line, max. 65528 */
    word rowBytes; /* bytes per line */
    Guchar *rawLine; /* raw line buffer */
    Guchar *pixLine; /* pixel line buffer */
    word pixIdx; /* current index in line buffer */

    short buf; /* asciiHex data: char or EOF */
    Obj dict; /* SubStream data */
    GBool eof;

    union
    {
        struct FStream *fs;
        struct LZWData *lzw;
        struct FlateData *flate;
        struct ASCII85Data *a85;
        struct RunLengthData *runLength;
        struct CCITTFaxData *fax;
        struct DCTData *dct;
        struct RC4Data *rc4;
        struct PredictorData *pred;
    } u;

} Stream;

/* FileStream */

typedef struct FStream
{
    FileHandle f;
    long start;
    long length;
    /*
     * 256 -> 512 : fewer, larger FileRead() calls for the same total bytes
     * read.
     */
    char buf[512];
    char *bufPtr;
    char *bufEnd;
    long bufPos;
    long savePos;
    Obj dict;
} FStream;

/*
 * RC4Stream.
 */

typedef struct RC4Data
{
    Guchar sBox[256]; /* RC4 permutation state */
    Guchar keyBuf[16]; /* per-object key(Algorithm 1), 5-16 bytes */
    short keyLen;
    Guchar rc4i, rc4j; /* RC4 PRGA indices */
    GBool peeked; /* one-byte lookahead, for *LookChar */
    short peekedChar;
} RC4Data;

/*
 * PredictorStream: undoes a PNG (10-15) or TIFF (2) predictor on the
 * bytes coming out of a Flate/LZW stream, so every reader (images, xref
 * streams, object streams) sees plain sample data.
 */
#define PDF_PREDICTOR_MAX_COMPS 32
typedef struct PredictorData
{
    short predictor;
    short nComps;
    short nBits;
    short pixBytes; /* bytes per pixel, at least 1 */
    word width; /* pixels per row, max. PDF_MAX_PREDICTOR_VALUES */
    word rowBytes; /* bytes per row, without the PNG tag byte */
    word lineIdx; /* next byte to hand out from curLine */
    GBool eof;
    Guchar *lines; /* 2 * rowBytes: previous and current row */
    Guchar *prevLine;
    Guchar *curLine;
    Guchar compBuf[PDF_PREDICTOR_MAX_COMPS]; /* TIFF: running sums */
} PredictorData;

/* LZWStream */

/*
 * String table of an LZW stream (12 KB). It is allocated on the first
 * reset and not with the filter (H3c): the parser builds the filter chain
 * of every stream object it reads, also of streams that are never decoded.
 * It is a swapable block that is locked only in LZWStreamReset and
 * LZWStreamFillBuf, so that it does not stay fixed in the heap.
 */
typedef struct LZWTables
{
    Guchar htab[4096]; /* only ever holds single decoded bytes */

    /* (16-bit range never needed) AND wrong */

    /* the old declared bound, a pre-existing */
    /* off-by-2 overflow independent of the */
    /* type) */
    short codetab[4096];
} LZWTables;

typedef struct LZWData
{

    short early;
    long inputBuf; /* input buffer */
    short inputBits; /* number of bits in input buffer */
    short inCodeBits; /* size of input code */
    char buf[1024]; /* buffer */
    char *bufPtr; /* next char to read */
    char *bufEnd; /* end of buffer */
    MemHandle tabH; /* LZWTables, swapable, 0 until the first reset */
    short oldcode;
    short free_ent;
    short finchar;
} LZWData;

/* FlateStream: FlateData is private to main/stream.goc (wraps zlib) */

/* ASCII85Stream */

typedef struct ASCII85Data
{
    short c[5]; /* char or EOF */
    Guchar b[4];
    short index, n;
} ASCII85Data;

/* RunLengthStream */

typedef struct RunLengthData
{
    char buf[128]; /* buffer */
    char *bufPtr; /* next char to read */
    char *bufEnd; /* end of buffer */
} RunLengthData;

/* CCITTFaxStream */

typedef struct CCITTFaxData
{
    long encoding; /* 'K' parameter */
    GBool byteAlign; /* 'EncodedByteAlign' parameter */
    short columns; /* 'Columns' parameter, max. PDF_MAX_CCITT_COLUMNS */
    long rows; /* 'Rows' parameter */
    GBool black; /* 'BlackIs1' parameter */
    GBool nextLine2D; /* true if next line uses 2D encoding */
    long inputBuf; /* input buffer */
    short inputBits; /* number of bits in input buffer */
    short *refLine; /* reference line changing elements */
    short b1; /* index into refLine */
    short *codingLine; /* coding line changing elements */
    short a0; /* index into codingLine */
    short outputBits; /* remaining ouput bits */
    short buf; /* character buffer */
} CCITTFaxData;

/* DCTStream: DCTData is private to main/dctStream.c (wraps ijgjpeg) */

#endif  /* PDFGEODE_H */

