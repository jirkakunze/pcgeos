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
    long size; /* size of <elems> array */
    long length; /* number of elements in array */
    long ref; /* reference count */

} Array;

/* Catalog */

typedef struct Catalog
{
    Obj pagesRoot; /* top-level /Pages ref or direct dict */
    long numPages; /* number of pages */
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
    long size; /* size of <entries> array */
    long length; /* number of entries in dictionary */
    long ref; /* reference count */

} Dict;

/* PageAttrs */

typedef struct PageAttrs
{

#ifdef USE_FULL_PAGE_ATTRS
    gdouble x1, y1, x2, y2;
    gdouble cropX1, cropY1, cropX2, cropY2;
    long rotate;
#endif

    Obj resources;
} PageAttrs;

typedef struct Page
{
    long num; /* page number */
    PageAttrs attrs; /* page attributes */
    Obj annots; /* annotations array */
    Obj contents; /* page contents */
    GBool ok; /* true if page is valid */
} Page;

/* Parser */

typedef struct Parser
{
    Obj buf1, buf2; /* next two tokens */
    long inlineImg; /* set when inline image data is encountered */
    struct Lexer *lexer;

    /*
     * noCrypt (the safe default set by ParserInit) covers every other Parser
     * in the codebase.
     */
    GBool noCrypt;
    long cryptNum, cryptGen;

} Parser;

typedef struct XRefEntry
{
    long offset;
    long gen;
    GBool used;
    /*
     * 0 = free/unused, 1 = classic , 2 = compressed inside an Object Stream
     * (PDF 1.5+ cross-reference streams).
     */
    short type;
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
    Guchar fileKey[16];
    short fileKeyLen;
    short encryptRevision;
    long permissions; /* Standard Security Handler /P flags */

    /*
     * One-entry Object Stream header cache. Keeping it here gives the cache
     * exactly the lifetime of its owning document.
     */
    long objStmCacheNum;
    long objStmCacheN;
    long objStmCacheFirst;
    long *objStmPairOffsets;
    VMBlockHandle objStmDecoded; /* HugeArray<byte>, complete decoded ObjStm */
    long objStmDecodedLength;

    struct XRefFetchContext *fetchContexts;

} XRef;

/* Lexer */

/* #define tokBufSize 128 // size of token buffer */

typedef struct Lexer
{
    Array *streams; /* array of input streams */
    long strPtr; /* index of current stream */
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
     * opSetExtGState and GfxBlendAlpha in main/gfx.goc.
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

} GfxState;

/* GfxFontDict */

typedef struct GfxFontDict
{
    struct GfxFont **fonts; /* list of fonts */
    long numFonts; /* number of fonts */
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
    GooString embFontName; /* name of embedded font */
    Ref embFontID; /* ref to embedded font file stream */
    GooString extFontFile; /* external font file name */

    GBool is16; /* set if font uses 16-bit chars */
    GBool hasCharSet; /* true if font descriptor has 'charset' entry */
    sdword fontWidthFactor; /* Used to tweak font width based on char widths */

    unsigned short widths[256]; /* char widths(x1000) */
    unsigned char charMap[256]; /* char encoding map */
    /* Widths are computed inline; charNames[256] is no longer needed. */

} GfxFont;

/* GfxFontEncoding */

#define gfxFontEncHashSize 419

typedef struct GfxFontEncoding
{
    char **encoding; /* code --> name mapping */
    GBool freeEnc; /* should we free the encoding array? */
    short /* name --> code hash table */
    hashTab[gfxFontEncHashSize];
} GfxFontEncoding;

typedef struct GfxResources
{
    GfxFontDict *fonts;
    Obj xObjDict;
    Obj colorSpaceDict;
    Obj extGStateDict;
    struct GfxResources *next;
} GfxResources;

typedef struct Gfx
{

    GfxResources *res; /* resource stack */

    GfxState *state; /* current graphics state */
    GBool fontChanged; /* set if font or text matrix has changed */

    int ignoreUndef; /* current BX/EX nesting level */

    Parser *parser; /* parser for page content stream(s) */
    XRef *xref; /* current xref pointer */
    VMFileHandle vmFile; /* scratch VM file */
    word nImages; /* # of images drawn so far on this page */
    GBool resourceLimitExceeded; /* OPT-15: page image guard fired */

    VMBlockHandle pageResourceList;
    word pageResourceCount;
    dword pageResourceBytes;

    void(*progressCallback)(dword kind, dword current, dword total,
        void *userData);
    void *progressUserData;
    GBool *cancelFlag;
} Gfx;

/* Built-in font table. */

typedef struct BuiltinFont
{
    char *name;
    Gushort *widths;
    struct GfxFontEncoding *encoding;
} BuiltinFont;

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
        strObjStmCache, /* VM-backed decoded Object Stream cache */
        strEOF,
        strWeird, /* internal-use stream types */
        strSubStream,
        strRC4
} StreamKind;

typedef struct Stream
{
    long ref; /* reference count */

    StreamKind kind;

    struct Stream *str; /* pointer to generic stream feeding this stream */

    /* ----- image stuff */
    short predictor; /* predictor */
    short nComps; /* components per pixel */
    short nBits; /* bits per component */
    short pixBytes; /* bytes per pixel */
    long width; /* pixels per line */
    long nVals; /* components per line */
    long rowBytes; /* bytes per line */
    Guchar *rawLine; /* raw line buffer */
    Guchar *pixLine; /* pixel line buffer */
    long pixIdx; /* current index in line buffer */

    long buf; /* asciiHex data */
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
        struct ObjStmCacheStreamData *objStmCache;
    } u;

} Stream;

/* FileStream */

/*
 * Small standalone stream over a byte HugeArray in the XRef scratch VM. Used
 * by the Object Stream decode cache (OPT-16).
 */
#define objStmCacheReadBufSize 256
typedef struct ObjStmCacheStreamData
{
    VMFileHandle vmFile;
    VMBlockHandle data;
    long start;
    long pos;
    long length;
    word bufIndex;
    word bufCount;
    Guchar buf[objStmCacheReadBufSize];
} ObjStmCacheStreamData;

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
    long peekedChar;
} RC4Data;

/* LZWStream */

typedef struct LZWData
{

    long predictor; /* parameters */
    long columns;
    long colors;
    long bits;
    long early;
    long inputBuf; /* input buffer */
    long inputBits; /* number of bits in input buffer */
    long inCodeBits; /* size of input code */
    char buf[1024]; /* buffer */
    char *bufPtr; /* next char to read */
    char *bufEnd; /* end of buffer */
    Guchar htab[4096]; /* only ever holds single decoded bytes */

    /* (16-bit range never needed) AND wrong */

    /* the old declared bound, a pre-existing */
    /* off-by-2 overflow independent of the */
    /* type) */
    short codetab[4096];
    long oldcode;
    long free_ent;
    long finchar;
} LZWData;

/* FlateStream */

#define flateWindow 32768  /* buffer size */
#define flateMask (flateWindow - 1)
#define flateReadBufSize 128  /* GetChar cache; avoids lock/unlock per byte */
#define flateMaxHuffman 15  /* max Huffman code length */
#define flateMaxCodeLenCodes 19 /* max # code length codes */
#define flateMaxLitCodes 288    /* max # literal codes */
#define flateMaxDistCodes 30    /* max # distance codes */

/* Huffman code table entry */
/*
 * All three fields narrowed from the original all-`long` layout : - code
 * (word): assigned either from a small loop index (safe) or from nextCode[]
 * in.
 */
typedef struct FlateCode
{
    word code; /* code word */
    word val; /* value represented by this code */
    Guchar len; /* code length in bits */
} FlateCode;

/* Huffman code table */
typedef struct FlateHuffmanTab
{
    long start[flateMaxHuffman + 2]; /* indexes of first code of each length */
    FlateCode *codes; /* codes, sorted by length and code word */
} FlateHuffmanTab;

/* Decoding info for length and distance code words */
typedef struct FlateDecode
{
    long bits; /* # extra bits */
    long first; /* first length/distance */
} FlateDecode;

typedef struct FlateData
{

    MemHandle bufhan; /* output data buffer */
    long index; /* current index into output buffer */
    long remain; /* number valid bytes in output buffer */
    long codeBuf; /* input buffer */
    long codeSize; /* number of bits in input buffer */
    FlateCode /* literal and distance codes */
    allCodes[flateMaxLitCodes + flateMaxDistCodes];
    FlateHuffmanTab litCodeTab; /* literal code table */
    FlateHuffmanTab distCodeTab; /* distance code table */
    GBool compressedBlock; /* set if reading a compressed block */
    long blockLen; /* remaining length of uncompressed block */
    GBool endOfBlock; /* set when end of block is reached */
    GBool eof; /* set when end of stream is reached */

    /* Keep the 32 KB window movable; lock it only while refilling the cache. */
    Guchar readBuf[flateReadBufSize];
    word readIndex;
    word readCount;

} FlateData;

/* ASCII85Stream */

typedef struct ASCII85Data
{
    long c[5];
    long b[4];
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
    long columns; /* 'Columns' parameter */
    long rows; /* 'Rows' parameter */
    GBool black; /* 'BlackIs1' parameter */
    GBool nextLine2D; /* true if next line uses 2D encoding */
    long inputBuf; /* input buffer */
    long inputBits; /* number of bits in input buffer */
    short *refLine; /* reference line changing elements */
    long b1; /* index into refLine */
    short *codingLine; /* coding line changing elements */
    long a0; /* index into codingLine */
    long outputBits; /* remaining ouput bits */
    long buf; /* character buffer */
} CCITTFaxData;

/* DCTStream */

/* DCT component info */
typedef struct DCTCompInfo
{
    long id; /* component ID */
    GBool inScan; /* is this component in the current scan? */
    long hSample, vSample; /* horiz/vert sampling resolutions */
    long quantTable; /* quantization table number */
    long dcHuffTable, acHuffTable; /* Huffman table numbers */
    long prevDC; /* DC coefficient accumulator */
} DCTCompInfo;

/* DCT Huffman decoding table */
typedef struct DCTHuffTable
{
    Guchar firstSym[17]; /* first symbol for this bit length */
    Gushort firstCode[17]; /* first code for this bit length */
    Gushort numCodes[17]; /* number of codes of this bit length */
    Guchar sym[256]; /* symbols */
} DCTHuffTable;

typedef struct DCTData
{
    long width, height; /* image size */
    long mcuWidth, mcuHeight; /* size of min coding unit, in data units */
    DCTCompInfo compInfo[4]; /* info for each component */
    short numComps; /* number of components in image */
    short colorXform; /* need YCbCr-to-RGB transform? */
    GBool sawAdobeMarker; /* TRUE if an Adobe APP14 marker was */
    /* actually found -- colorXform==0 is */
    /* ambiguous otherwise (never-set default */
    /* vs. */
    long restartInterval; /* restart interval, in MCUs */
    Guchar quantTables[4][64]; /* quantization tables */
    short numQuantTables; /* number of quantization tables */
    DCTHuffTable dcHuffTables[4]; /* DC Huffman tables */
    DCTHuffTable acHuffTables[4]; /* AC Huffman tables */
    short numDCHuffTables; /* number of DC Huffman tables */
    short numACHuffTables; /* number of AC Huffman tables */
    MemHandle rowBufHandle[4][32]; /* swapable buffers for one MCU row */
    /* valid only while corresponding handle is locked */
    Guchar *rowBuf[4][32];
    short lockedRow; /* output row currently locked, -1 if none */
    long comp, x, y, dy; /* current position within image/MCU */
    long restartCtr; /* MCUs left until restart */
    long restartMarker; /* next restart marker */
    long inputBuf; /* input buffer for variable length codes */
    long inputBits; /* number of valid bits in input buffer */
    MemHandle idctWorkspaceHandle;
} DCTData;

#endif  /* PDFGEODE_H */

