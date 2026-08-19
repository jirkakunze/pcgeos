/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  Copyright (c) GlobalPC 1999.  All rights reserved.
  GLOBALPC CONFIDENTIAL

PROJECT:
MODULE:		PDF Viewer
FILE:		pdfGeode.goh

AUTHOR:		John Mevissen, Apr 02, 1999

ROUTINES:
  Name			Description
  ----			-----------

REVISION HISTORY:
  Name		Date		Description
  ----		----		-----------
  mevissen	4/02/99   	Initial version.

DESCRIPTION:

  All the structs.

  $Id$

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef _PDFGEODE_H
#define _PDFGEODE_H

#include <geos.h>
#include <file.h>
#include "gtypes.h"

/*
 * There's round-off error in setting the current point after drawing
 * very small (e.g. 1-pt) fonts.  We scale the font up, and the transfer
 * matrix down.
 */

#define FONT_SCALING_FACTOR 32

/*
 * Safe upper bound on bytes-per-HugeArrayAppend-call for batched
 * fixed-size-element appends (main/xref.goc's xref->entries,
 * main/catalog.goc's cat->pages).
 *
 * Deliberately modest, not pushed up toward the ~65535-byte ceiling
 * where a real kernel bug in InitHAChain silently drops the high
 * word of elementSize*numElem (see project roadmap 2.7). An earlier
 * version of this constant (60000) sat close enough to a full 64KB
 * real-mode segment that the temporary gmalloc'd batch buffer itself
 * became a problem -- allocating something that close to the segment
 * ceiling repeatedly (once per XRefInit/CatalogInit call) made the
 * viewer noticeably *slower* than before, almost certainly from
 * memory-manager compaction/pressure at that size, not from anything
 * about HugeArrayAppend itself. 2000 bytes still cuts the number of
 * HugeArrayAppend calls by roughly 150x for a ~12-byte XRefEntry (or
 * more for smaller elements), which is the overwhelming majority of
 * the achievable win -- going further up towards the 64KB ceiling
 * buys very little additional call-count reduction for a lot more
 * memory pressure.
 */
#define HUGEARRAY_BATCH_MAX_BYTES 2000

typedef struct GooString
{
  long length;
  char *s;
} GooString;

typedef struct Ref
{
  long num; // object number
  long gen; // generation number
} Ref;

//------------------------------------------------------------------------
// object types
//------------------------------------------------------------------------

typedef enum ObjType
{
  // simple objects
  objBool,   // boolean
  objInt,    // integer
  objReal,   // real
  objString, // string
  objName,   // name
  objNull,   // null

  // complex objects
  objArray,  // array
  objDict,   // dictionary
  objStream, // stream
  objRef,    // indirect reference

  // special objects
  objCmd,   // command name
  objError, // error return from Lexer
  objEOF,   // end of file return from Lexer
  objNone   // uninitialized object
} ObjType;

#define numObjTypes 14 // total number of object types

//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

typedef struct Obj
{

  ObjType type; // object type
  union
  {                        // value for each type:
    GBool booln;           //   boolean
    long intg;             //   integer
    gdouble real;          //   real
    GooString string;      //   string
    char *name;            //   name
    struct Array *array;   //   array
    struct Dict *dict;     //   dictionary
    struct Stream *stream; //   stream
    Ref ref;               //   indirect reference
    char *cmd;             //   command
  } u;
} Obj;

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

typedef struct Array
{

  Obj *elems;  // array of elements
  long size;   // size of <elems> array
  long length; // number of elements in array
  long ref;    // reference count

} Array;

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

typedef struct Catalog
{
  Obj pagesRoot; // top-level /Pages ref or direct dict
  long numPages; // number of pages
  Obj dests;     // named destination dictionary
  Obj nameTree;  // name tree
  GBool ok;      // true if catalog is valid

} Catalog;

//------------------------------------------------------------------------
// Dict
//------------------------------------------------------------------------

typedef struct DictEntry
{
  char *key;
  Obj val;
} DictEntry;

typedef struct Dict
{
  DictEntry *entries; // array of entries
  long size;          // size of <entries> array
  long length;        // number of entries in dictionary
  long ref;           // reference count

} Dict;

//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------

typedef struct PageAttrs
{

#ifdef USE_FULL_PAGE_ATTRS
  gdouble x1, y1, x2, y2;
  gdouble cropX1, cropY1, cropX2, cropY2;
  long rotate;
#endif

  Obj resources;
} PageAttrs;

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

typedef struct Page
{
  long num;        // page number
  PageAttrs attrs; // page attributes
  Obj annots;      // annotations array
  Obj contents;    // page contents
  GBool ok;        // true if page is valid
} Page;

//------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------

typedef struct Parser
{
  Obj buf1, buf2; // next two tokens
  long inlineImg; // set when inline image data is encountered
  struct Lexer *lexer;

  /*
   * Set by whoever constructs this Parser for one specific indirect
   * object fetch (XRefFetch's classic path), so ParserMakeStream can
   * wrap a freshly-built stream in RC4 decryption using that
   * object's own per-object key (Algorithm 1) -- see main/crypt.goc.
   * noCrypt (the safe default set by ParserInit) covers every other
   * Parser in the codebase: cross-reference streams are never
   * encrypted even in an encrypted document (chicken-and-egg -- they
   * have to be readable before the key can be derived at all), and
   * objects parsed *out of* an already-decrypted Object Stream aren't
   * separately re-encrypted per spec.
   */
  GBool noCrypt;
  long cryptNum, cryptGen;

  //  FStream fs;
} Parser;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

typedef struct XRefEntry
{
  long offset;
  long gen;
  GBool used;
  /*
   * 0 = free/unused, 1 = classic (offset is a byte position in the
   * file, gen is the real generation number -- the only kind that
   * existed before PDF 1.5), 2 = compressed inside an Object Stream
   * (PDF 1.5+ cross-reference streams). For type 2, offset/gen are
   * reinterpreted rather than given new fields: offset holds the
   * containing ObjStm's object number, gen holds this object's index
   * within it. Generation is implicitly 0 for every compressed
   * object per spec, so there's no real generation number to lose.
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

  long start; // offset in file (to allow for garbage
              //   at beginning of file)
  VMBlockHandle entries; 
  long size;             // size of <entries> array
  long rootNum, rootGen; // catalog dict
  GBool ok;              // true if xref table is valid
  Obj trailerDict;       // trailer dictionary

  /*
   * Standard Security Handler, empty user password only (see
   * main/crypt.goc). `encrypted` is set once XRefSetupEncryption has
   * verified the empty user password actually opens the file (PDF
   * spec Algorithm 6) -- a real, non-empty password requirement still
   * ends in PDF_ERR_ENCRYPTED, same as before this existed. `fileKey`
   * is the file-level encryption key from Algorithm 2, reused by
   * XRefDeriveObjectKey (Algorithm 1) for each object's own RC4 key.
   */
  GBool encrypted;
  Guchar fileKey[16];
  short fileKeyLen;
  short encryptRevision;
  long permissions; // Standard Security Handler /P flags

  /*
   * One-entry Object Stream header cache.  This used to be static
   * state inside XRefFetchFromObjStm(), which made it shared across
   * documents and left a raw XRef pointer plus allocated offsets
   * alive past XRefFree().  Keeping it here gives the cache exactly
   * the lifetime of its owning document.
   */
  long objStmCacheNum;
  long objStmCacheN;
  long objStmCacheFirst;
  long *objStmPairOffsets;
  VMBlockHandle objStmDecoded; // HugeArray<byte>, complete decoded ObjStm
  long objStmDecodedLength;

  struct XRefFetchContext *fetchContexts;

} XRef;

//------------------------------------------------------------------------
// Lexer
//------------------------------------------------------------------------

// #define tokBufSize 128		// size of token buffer

typedef struct Lexer
{
  Array *streams;  // array of input streams
  long strPtr;     // index of current stream
  Obj curStr;      // current stream
  GBool freeArray; // should lexer free the streams array?

  XRef *xref;
} Lexer;

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

/*
 * Geos needs to be told when a path is starting, and when to set a clip path
 */

typedef enum PathType
{
  PATH_NONE,
  PATH_NORMAL,
  PATH_CLIP,
  PATH_CLIP_EO,
} PathType;

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

typedef struct GfxColor
{
  short r, g, b;
} GfxColor;

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

typedef enum GfxColorMode
{
  colorGray,
  colorCMYK,
  colorRGB
} GfxColorMode;

typedef struct GfxColorSpace
{

  //  Function *sepFunc;		// separation tint transform function
  GfxColorMode mode;   // color mode
  GBool indexed;       // set for indexed colorspaces
  short numComps;      // number of components in colors
  short indexHigh;     // max pixel for indexed colorspace
  Guchar (*lookup)[4]; // lookup table (only for indexed
                       //   colorspaces)
  GBool ok; // is color space valid?

  /*
   * Set when `mode` is really standing in for a DeviceN or
   * Separation colorspace whose real tint-transform Function isn't
   * evaluated (see GfxColorSpaceInit). Tells GfxColorSpaceGetColor to
   * average numComps raw tint values and invert them (tint 1.0 = full
   * colorant = dark, the opposite sense of DeviceGray's 1.0 = white)
   * instead of treating component 0 as a literal gray level.
   *
   * This is now only the FALLBACK for cases we can't evaluate for
   * real (DeviceN with more than one colorant, or a tint-transform
   * Function that isn't Type 2). See hasFunction below for the
   * common case, which is evaluated properly.
   */
  GBool tintApprox;

  /*
   * Set for Separation (or single-colorant DeviceN) whose
   * tint-transform Function is Type 2 (Exponential Interpolation) --
   * by far the most common case in practice, and the only Function
   * type this port evaluates. numComps stays 1 here (that's what's
   * actually in the content stream, one raw tint byte per pixel);
   * altMode/altNumComps describe the REAL alternate colorspace the
   * function's output lands in, kept separate from mode/numComps
   * specifically so pixel reading (driven by numComps) doesn't
   * regress to the old bug of expecting as many bytes/pixel as the
   * alternate space has components.
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
  //  double ctm[6];		// coord transform matrix
  //  double px1, py1, px2, py2;	// page corners (user coords)
  //  double pageWidth, pageHeight;	// page size (pixels)

  /*
   * Embedded by value (not pointer, unlike upstream xpdf's
   * new/delete-managed GfxColorSpace*) so GfxStateCopy's plain
   * memcpy() on q/Q already does the right thing -- no separate
   * alloc/free lifecycle to get wrong. Only wrinkle: an Indexed
   * colorspace's `lookup` table pointer would be shallow-copied
   * (shared, not duplicated) across a q/Q pair; opSetFillColorSpace/
   * opSetStrokeColorSpace below deliberately don't support Indexed
   * as a fill/stroke colorspace (rare in practice for plain fills)
   * specifically to avoid that lifecycle question entirely.
   */
  GfxColorSpace fillColorSpace;   // fill color space
  GfxColorSpace strokeColorSpace; // stroke color space
  GfxColor fillColor;             // current fill color -- needed by
                      // GfxDrawImageMask to paint a stencil
                      // mask in the current color per spec,
                      // instead of the fixed black/white it
                      // used to draw unconditionally (see
                      // project roadmap 2.1.4). Not tracked for
                      // stroke: nothing in this port needs a
                      // stroke-color readback the way image
                      // masks need a fill-color readback.
  //  GfxColor strokeColor;		// stroke color

  /*
   * Constant opacity from an ExtGState's /ca (fill) and /CA (stroke)
   * -- see opSetExtGState and GfxBlendAlpha in main/gfx.goc. No blend
   * modes, no soft masks/transparency groups: just alpha blended
   * against white at the point a color is actually set, same
   * against-white approximation as /SMask (we can't read back
   * whatever's already drawn on the page any more here than there).
   * 1.0 (fully opaque, PDF's own default) needs no blending at all,
   * so this is a no-op for the overwhelming majority of content that
   * never touches the "gs" operator.
   */
  gdouble fillAlpha;
  gdouble strokeAlpha;

  //  double lineWidth;		// line width
  //  double *lineDash;		// line dash
  //  int lineDashLength;
  //  double lineDashStart;
  //  int flatness;			// curve flatness
  //  int lineJoin;			// line join style
  //  int lineCap;			// line cap style
  //  double miterLimit;		// line miter limit

  struct GfxFont *font; // font
  gdouble fontSize;     // font size
  gdouble textMat[6];   // text matrix
  sdword charSpace;     // character spacing
  gdouble wordSpace;    // word spacing
  sdword horizScaling;  // horizontal scaling
  gdouble leading;      // text leading
  sdword rise;          // text rise
  short render;         // text rendering mode

  //  GfxPath *path;		// array of path elements
  //  double curX, curY;		// current point (user coords)
  gdouble lineX, lineY;    // start of current text line (text coords)
  WWFixedAsDWord curTextX; // current drawing position on x-axis

  struct GfxState *saved; // next GfxState on stack

  Handle gstring;    /* gstring we're writing to */
  PathType pathType; /* whether there's a current path or clipping */

  /*
   * Raw (pre-rounding) bounding box of the path currently being
   * built via opMoveTo/opLineTo/opCurveTo, tracked in parallel to
   * the already-rounded path GEOS itself is building via
   * GrMoveTo/GrDrawLineTo/GrDrawCurveTo. opMoveTo resets this
   * whenever it starts a fresh path (pathType wasn't already
   * PATH_NORMAL); opLineTo/opCurveTo extend it.
   *
   * Needed because GrDrawLineTo/GrDrawCurveTo/GrDrawRect only take
   * integer (sword) coordinates -- confirmed against the real
   * graphics.h, no fixed-point siblings exist for these, unlike
   * GrMoveToWWFixed. A path whose true (unrounded) extent is under
   * ~1 unit in some dimension (e.g. a hairline table border drawn
   * as a thin filled rectangle rather than a stroke) can have both
   * of that dimension's edges round to the *same* integer, filling
   * zero area -- invisible despite being a perfectly valid,
   * intentional shape in the PDF. opFill/opEOFill compare this raw
   * box against the rounded one it's about to fill, and substitute
   * a deliberately nudged (>=1 unit) rectangle fill instead when a
   * collapse like that is about to happen (see project roadmap
   * 2.6). Left untouched (and unused) for anything that isn't a
   * plain fill -- strokes already get a correct minimum width from
   * GrSetLineWidth's own WWFixed parameter, and clip paths don't
   * have this "the whole point vanishes" failure mode the way a
   * fill does.
   */
  gdouble pathRawMinX, pathRawMinY, pathRawMaxX, pathRawMaxY;
  GBool pathRawBoundsValid; /* gFalse until the first point of the
                             * current path has been seen */

  /*
   * PDF fill closes each open subpath implicitly.  GEOS GrFillPath does
   * not supply that missing closing edge for the path representation used
   * by this port, so remember the rounded start of the current subpath.
   * This is intentionally tiny state for the real-mode build.
   */
  sword pathSubpathStartX, pathSubpathStartY;
  GBool pathSubpathStartValid;

  //  GfxState(GfxState *state);
} GfxState;

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

typedef struct GfxFontDict
{
  struct GfxFont **fonts; // list of fonts
  long numFonts;          // number of fonts
  /*
   * 1-entry "last lookup" cache (see project optimization analysis
   * P10): GfxFontDictLookup runs a linear, string-comparing scan
   * over every font in the page's /Resources on EVERY /Tf operator
   * -- toggling between the same small handful of fonts (regular/
   * bold/italic mixed inline, very common) means the immediately
   * preceding tag is often looked up again shortly after. Scoped to
   * this struct specifically (font resources), not a change to the
   * generic Dict lookup used everywhere else in the project.
   *
   * No separate cached-tag field needed: lastFont->tag (a GooString
   * owned by the GfxFont itself, stable for the font's whole
   * lifetime) is exactly what GfxFontMatches already compares
   * against, so re-checking against it directly is both correct and
   * doesn't need to track a second piece of state that could get
   * out of sync with it.
   */
  struct GfxFont *lastFont; // NULL until the first successful
                            // lookup
} GfxFontDict;

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

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
  GooString tag;          // PDF font tag
  Ref id;                 // reference (used as unique ID)
  GooString name;         // font name
  long flags;             // font descriptor flags
  GfxFontType type;       // type of font
  GooString embFontName;  // name of embedded font
  Ref embFontID;          // ref to embedded font file stream
  GooString extFontFile;  // external font file name
                          //  gdouble fontMat[6];		// font matrix
  GBool is16;             // set if font uses 16-bit chars
  GBool hasCharSet;       /* true if font descriptor has 'charset' entry */
  sdword fontWidthFactor; /* Used to tweak font width based on char widths */

  unsigned short widths[256]; /* char widths (x1000) */
  unsigned char charMap[256]; // char encoding map
  /*
   * charNames[256] removed (see project optimization analysis H1):
   * used to persist every glyph name discovered during encoding so
   * a later pass (GfxFontMakeWidths) could search for each one's
   * standard-font fallback width. That search now happens
   * immediately as each name is discovered (GfxFontAddChar,
   * CopyNamedEncoding, and GfxFontInit's own base-table setup, all
   * in main/gfxFont.goc), so no name ever needs to outlive the Obj
   * it came from -- removing both the persistent storage (256
   * pointers, 1KB on this platform) and the small, previously
   * accepted leak from R3 (copyString'd /Differences names that
   * were never freed, since telling them apart from the
   * non-owned static-table pointers sharing this same array would
   * have needed real per-slot ownership tracking).
   */

  //  union {
  //    GfxFontEncoding *encoding;	// 8-bit font encoding
  //    struct {
  //      GfxFontCharSet16 charSet;	// 16-bit character set
  //      GfxFontEncoding16 *enc;	// 16-bit encoding (CMap)
  //    } enc16;
  //  };
  //  union {
  //    double widths[256];		// width of each char for 8-bit font
  //    GfxFontWidths16 widths16;	// char widths for 16-bit font
  //  }u;
} GfxFont;

//------------------------------------------------------------------------
// GfxFontEncoding
//------------------------------------------------------------------------

#define gfxFontEncHashSize 419

typedef struct GfxFontEncoding
{
  char **encoding; // code --> name mapping
  GBool freeEnc;   // should we free the encoding array?
  short            // name --> code hash table
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
  //  OutputDev *out;		// output device
  GfxResources *res; // resource stack

  GfxState *state;   // current graphics state
  GBool fontChanged; // set if font or text matrix has changed
                     //  GfxClipType clip;		// do a clip?
  int ignoreUndef;   // current BX/EX nesting level

  Parser *parser;              // parser for page content stream(s)
  XRef *xref;                  // current xref pointer
  VMFileHandle vmFile;         // scratch VM file
  word nImages;                // # of images drawn so far on this page
  GBool resourceLimitExceeded; /* OPT-15: page image guard fired */


  VMBlockHandle pageResourceList;
  word pageResourceCount;
  dword pageResourceBytes;

  void (*progressCallback)(dword kind, dword current, dword total, void *userData);
  void *progressUserData;
  GBool *cancelFlag;
} Gfx;

//------------------------------------------------------------------------
// Built-in font table.
//------------------------------------------------------------------------

typedef struct BuiltinFont
{
  char *name;
  Gushort *widths;
  struct GfxFontEncoding *encoding;
} BuiltinFont;

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

typedef struct GfxImageColorMap
{

  GfxColorSpace *colorSpace; // the image colorspace
  short bits;                // bits per component
  short numComps;            // number of components in a pixel
  GBool indexed;             // set for indexed color space
  GfxColorMode mode;         // color mode
  short (*lookup)[4];        // lookup table
  gdouble decodeLow[4];      // minimum values for each component
  gdouble decodeRange[4];    // max - min value for each component
  GBool ok;
  /*
   * Set when `lookup` is a pure identity mapping (see project
   * optimization analysis H4): bits==8, the default/no-op decode
   * array (decodeLow==0, decodeRange==1 for every component), not
   * indexed, and neither the hasFunction nor tintApprox special
   * cases apply (those have fundamentally different per-pixel
   * logic, not a simple per-component table lookup at all).
   * Computed once in GfxImageColorMapInit; lets
   * GfxImageColorMapGetColor skip the lookup[][] table entirely for
   * the common case (a plain 8-bit DeviceRGB/DeviceGray image with
   * no custom /Decode array) and use the raw sample byte directly.
   */
  GBool isIdentity;

} GfxImageColorMap;

//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

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
  strObjStmCache, // VM-backed decoded Object Stream cache
  strEOF,
  strWeird, // internal-use stream types
  strSubStream,
  strRC4 
} StreamKind;

typedef struct Stream
{
  long ref; // reference count

  StreamKind kind;

  struct Stream *str; /* pointer to generic stream feeding this stream */

  //----- image stuff
  short predictor; // predictor
  short nComps;    // components per pixel
  short nBits;     // bits per component
  short pixBytes;  // bytes per pixel
  long width;      // pixels per line
  long nVals;      // components per line
  long rowBytes;   // bytes per line
  Guchar *rawLine; // raw line buffer
  Guchar *pixLine; // pixel line buffer
  long pixIdx;     // current index in line buffer

  long buf; // asciiHex data
  Obj dict; // SubStream data
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

//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------

/*
 * Small standalone stream over a byte HugeArray in the XRef scratch VM.
 * Used by the Object Stream decode cache (OPT-16): decoded ObjStm bytes
 * stay in VM, while only a tiny read buffer occupies conventional heap.
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
   * 256 -> 512 (see project optimization analysis V5): fewer,
   * larger FileRead() calls for the same total bytes read. Purely
   * a size change -- every use of this field goes through
   * sizeof(this->buf) or plain pointer arithmetic within it (main/
   * stream.goc's FStreamFillBuf etc.), nothing hardcodes 256
   * elsewhere. Heap-allocated as part of Stream, not on the stack,
   * so the extra 256 bytes per open file stream is a modest,
   * bounded cost, not a Real-Mode stack concern.
   */
  char buf[512];
  char *bufPtr;
  char *bufEnd;
  long bufPos;
  long savePos;
  Obj dict;
} FStream;

/*------------------------------------------------------------------------
 * RC4Stream -- decrypts an encrypted PDF's raw stream bytes on the fly
 *-----------------------------------------------------------------------*/

typedef struct RC4Data
{
  Guchar sBox[256];  // RC4 permutation state
  Guchar keyBuf[16]; // per-object key (Algorithm 1), 5-16 bytes
  short keyLen;
  Guchar rc4i, rc4j; // RC4 PRGA indices
  GBool peeked;      // one-byte lookahead, for *LookChar
  long peekedChar;
} RC4Data;

//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------

typedef struct LZWData
{
  //  Stream *str;			// stream
  long predictor; // parameters
  long columns;
  long colors;
  long bits;
  long early;
  long inputBuf;     // input buffer
  long inputBits;    // number of bits in input buffer
  long inCodeBits;   // size of input code
  char buf[1024];    // buffer
  char *bufPtr;      // next char to read
  char *bufEnd;      // end of buffer
  Guchar htab[4096]; // only ever holds single decoded bytes
                     // (0..255); was long[4094] -- wrong type
                     // (16-bit range never needed) AND wrong
                     // size (free_ent can reach 4095, one past
                     // the old declared bound, a pre-existing
                     // off-by-2 overflow independent of the
                     // type)
  short codetab[4096];
  long oldcode;
  long free_ent;
  long finchar;
} LZWData;

//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

#define flateWindow 32768 // buffer size
#define flateMask (flateWindow - 1)
#define flateReadBufSize 128    // GetChar cache; avoids lock/unlock per byte
#define flateMaxHuffman 15      // max Huffman code length
#define flateMaxCodeLenCodes 19 /* max # code length codes */
#define flateMaxLitCodes 288    /* max # literal codes */
#define flateMaxDistCodes 30    /* max # distance codes */

// Huffman code table entry
/*
 * All three fields narrowed from the original all-`long` layout
 * (see project optimization analysis H3, and the code/nextCode
 * validation added alongside it):
 *
 * - code (word): assigned either from a small loop index (safe) or
 *   from nextCode[] in FlateStreamCompHuffmanCodes (main/
 *   stream.goc). nextCode[] itself is now validated as it's built
 *   (Kraft's inequality, checked incrementally) rather than left
 *   wide to tolerate malformed input -- any code-length
 *   distribution that would push it past word's range is rejected
 *   before construction completes, so a validated table's code
 *   values never exceed 2^15 (32768). See that function's own
 *   comment for the full reasoning and the numeric verification
 *   behind it.
 * - val (word): always a loop index bounded by flateMaxLitCodes
 *   (288) at most, comfortably within word's 65535 ceiling
 *   regardless of malformed input.
 * - len (Guchar): always either a small literal constant, a 3-bit
 *   code word (0-7), or a decoded code-length value 0-15
 *   (flateMaxHuffman) -- 16/17/18 are "repeat previous" markers
 *   handled separately and never stored here directly.
 *
 * With no `long` field left, alignment no longer dictates field
 * order (word's 2-byte alignment covers everything here), so this
 * is 2+2+1 = 5 bytes, padded to 6 -- versus the original 12-byte
 * all-`long` layout, or the 8-byte intermediate version before
 * code's own validation was added.
 */
typedef struct FlateCode
{
  word code;  // code word
  word val;   // value represented by this code
  Guchar len; // code length in bits
} FlateCode;

// Huffman code table
typedef struct FlateHuffmanTab
{
  long start[flateMaxHuffman + 2]; // indexes of first code of each length
  FlateCode *codes;                // codes, sorted by length and code word
} FlateHuffmanTab;

// Decoding info for length and distance code words
typedef struct FlateDecode
{
  long bits;  // # extra bits
  long first; // first length/distance
} FlateDecode;

typedef struct FlateData
{

  MemHandle bufhan; // output data buffer
  long index;       // current index into output buffer
  long remain;      // number valid bytes in output buffer
  long codeBuf;     // input buffer
  long codeSize;    // number of bits in input buffer
  FlateCode         // literal and distance codes
      allCodes[flateMaxLitCodes + flateMaxDistCodes];
  FlateHuffmanTab litCodeTab;  // literal code table
  FlateHuffmanTab distCodeTab; // distance code table
  GBool compressedBlock;       // set if reading a compressed block
  long blockLen;               // remaining length of uncompressed block
  GBool endOfBlock;            // set when end of block is reached
  GBool eof;                   // set when end of stream is reached

  /* Small near-memory read cache.  The 32 KB DEFLATE window stays
   * movable: it is locked only while this cache is refilled, rather
   * than once for every StreamGetChar() call. */
  Guchar readBuf[flateReadBufSize];
  word readIndex;
  word readCount;

} FlateData;

//------------------------------------------------------------------------
// ASCII85Stream
//------------------------------------------------------------------------

typedef struct ASCII85Data
{
  long c[5];
  long b[4];
  short index, n;
} ASCII85Data;

//------------------------------------------------------------------------
// RunLengthStream
//------------------------------------------------------------------------

typedef struct RunLengthData
{
  char buf[128]; // buffer
  char *bufPtr;  // next char to read
  char *bufEnd;  // end of buffer
} RunLengthData;

//------------------------------------------------------------------------
// CCITTFaxStream
//------------------------------------------------------------------------

typedef struct CCITTFaxData
{
  long encoding;     // 'K' parameter
  GBool byteAlign;   // 'EncodedByteAlign' parameter
  long columns;      // 'Columns' parameter
  long rows;         // 'Rows' parameter
  GBool black;       // 'BlackIs1' parameter
  GBool nextLine2D;  // true if next line uses 2D encoding
  long inputBuf;     // input buffer
  long inputBits;    // number of bits in input buffer
  short *refLine;    // reference line changing elements
  long b1;           // index into refLine
  short *codingLine; // coding line changing elements
  long a0;           // index into codingLine
  long outputBits;   // remaining ouput bits
  long buf;          // character buffer
} CCITTFaxData;

//------------------------------------------------------------------------
// DCTStream
//------------------------------------------------------------------------

// DCT component info
typedef struct DCTCompInfo
{
  long id;                       // component ID
  GBool inScan;                  // is this component in the current scan?
  long hSample, vSample;         // horiz/vert sampling resolutions
  long quantTable;               // quantization table number
  long dcHuffTable, acHuffTable; // Huffman table numbers
  long prevDC;                   // DC coefficient accumulator
} DCTCompInfo;

// DCT Huffman decoding table
typedef struct DCTHuffTable
{
  Guchar firstSym[17];   // first symbol for this bit length
  Gushort firstCode[17]; // first code for this bit length
  Gushort numCodes[17];  // number of codes of this bit length
  Guchar sym[256];       // symbols
} DCTHuffTable;

typedef struct DCTData
{
  long width, height;       // image size
  long mcuWidth, mcuHeight; // size of min coding unit, in data units
  DCTCompInfo compInfo[4];  // info for each component
  short numComps;           // number of components in image
  short colorXform;         // need YCbCr-to-RGB transform?
  GBool sawAdobeMarker;     // TRUE if an Adobe APP14 marker was
                        // actually found -- colorXform==0 is
                        // ambiguous otherwise (never-set default
                        // vs. explicit "no transform")
  long restartInterval;          // restart interval, in MCUs
  Guchar quantTables[4][64];     // quantization tables
  short numQuantTables;          // number of quantization tables
  DCTHuffTable dcHuffTables[4];  // DC Huffman tables
  DCTHuffTable acHuffTables[4];  // AC Huffman tables
  short numDCHuffTables;         // number of DC Huffman tables
  short numACHuffTables;         // number of AC Huffman tables
  MemHandle rowBufHandle[4][32]; // swapable buffers for one MCU row
  Guchar *rowBuf[4][32];         // valid only while corresponding handle is locked
  short lockedRow;               // output row currently locked, -1 if none
  long comp, x, y, dy;           // current position within image/MCU
  long restartCtr;               // MCUs left until restart
  long restartMarker;            // next restart marker
  long inputBuf;                 // input buffer for variable length codes
  long inputBits;                // number of valid bits in input buffer
  MemHandle idctWorkspaceHandle; // swapable 64-long IDCT workspace (OPT-22)
} DCTData;

#endif  /* _PDFGEODE_H */
