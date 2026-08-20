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

//------------------------------------------------------------------------

#define xrefSearchSize 1024	// read this many bytes at end of file
				//   to look for 'startxref'

/* Defined later in this file; used earlier (XRefReadTrailer et al).
 * See its own comment where defined. */
static long XRefParseVarDigits(const char *p);

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

/* 
 * Forward decls
 */
long XRefReadTrailer (XRef *xref, Stream *fs);
static void XRefSetStart (XRef *xref, Stream *fs);

/*
 * PDF 1.5+ cross-reference STREAMS (as opposed to the classic
 * plain-text table XRefReadXRef/XRefReadTrailer above handle).
 */
static GBool XRefSectionIsStream(XRef *xref, Stream *fs, long pos);
static long XRefStreamReadField(Obj *streamObj, long width, long defaultVal);
static long XRefReadTrailerFromStream(XRef *xref, Stream *fs, long pos);
GBool XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection);
GBool XRefReadXRefStream(XRef *xref, Stream *fs, long *pos, GBool isFirstSection);
static void XRefFetchFromObjStm(XRef *xref, long objStmNum, long idxInStm, Obj *obj);



void
XRefInitNull(XRef *xref) {

  xref->size = 0;
  xref->entries = NULL;
  xref->vmFile = 0;
  initNull(&xref->trailerDict);
  xref->encrypted = gFalse;
}


Boolean
XRefInit(XRef *xref, FileHandle fileHan) {
    XRefEntry entryTemplate;
    XRefEntry *batchBuf;
    word batchMax, batchThis;
    long remaining;
    long entryIdx;
    char *p;

/* XRef::XRef(FileStream *str) {
*/

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
   * Own scratch VM file for `entries`, separate from
   * PdfDocInternal's gstringFile (decoded pages): that one doesn't
   * exist yet at this point in PdfOpen's sequence (created later,
   * only once the xref table -- this one -- is already up and
   * running). A second small temp file per open document is a
   * trivial cost either way.
   */
  p = xref->vmFileName;
  FileConstructFullPath(&p, sizeof(xref->vmFileName), SP_WASTE_BASKET, "", TRUE);
  xref->vmFile = VMOpen(xref->vmFileName,
			VMAF_FORCE_READ_WRITE | VMAF_USE_BLOCK_LEVEL_SYNCHRONIZATION,
			VMO_TEMP_FILE,
			0);
  if (!xref->vmFile) {
    goto err;
  }

  // read the trailer
//  file = str->getFile();

  XRefSetStart(xref, &fs);
//  xref->start = 0;		/* str->getStart(); */
  pos = XRefReadTrailer(xref, &fs);


  // if there was a problem with the trailer,
  // try to reconstruct the xref table
  if (pos == 0) 
    {
    goto err;
//    if (!(ok = constructXRef(str))) {
//      xref = oldXref;
//      return;
//    }

  // trailer is ok - read the xref table
  } else {
    /*
     * One HugeArray element per XRefEntry, pre-filled to "free/
     * unused" via a template -- same construction Catalog uses for
     * cat->pages (HugeArrayCreate at 0, then HugeArrayAppend with a
     * template element), not a single flat MemAlloc.
     *
     * IMPORTANT: appending all xref->size elements in a single
     * HugeArrayAppend(..., xref->size, &entryTemplate) call was
     * confirmed (via targeted diagnostics, see project history) to
     * only correctly initialize the FIRST appended element -- every
     * element after that came back with garbage instead of the
     * template's values. This is NOT a bug in HugeArrayAppend itself:
     * per a later kernel source review (project history), initData
     * for numElem>1 is not a single template to replicate -- the
     * kernel copies numElem*elementSize bytes starting at initData
     * directly into the newly appended elements, i.e. it expects a
     * buffer already holding numElem complete, consecutive elements.
     * A single &entryTemplate object only provides the first
     * element's worth of real data; every element after that reads
     * past the object as an out-of-bounds read of whatever memory
     * happens to follow it. Appending one element at a time, in a
     * loop, sidesteps this correctly: with numElem=1 a single
     * template object legitimately *is* "a buffer holding 1 complete
     * element", no replication semantics needed. (A second, separate
     * kernel bug exists in InitHAChain's own byte-count computation
     * for numElem*elementSize > 65535, unrelated to this and not
     * triggered here since numElem is always 1.)
     */
    entryTemplate.offset = -1;
    entryTemplate.gen = 0;
    entryTemplate.used = gFalse;
    entryTemplate.type = xrefEntryFree;
    xref->entries = HugeArrayCreate(xref->vmFile, sizeof(XRefEntry), 0);

    /*
     * Batched append instead of one HugeArrayAppend call per element
     * (correct but slow -- see roadmap 2.7) or a single call for the
     * whole table (silently under-initializes past a real, separate
     * kernel bug in InitHAChain once elementSize*numElem exceeds
     * ~65535 bytes: the high word of that product gets dropped).
     * batchBuf holds up to HUGEARRAY_BATCH_MAX_BYTES worth of real,
     * filled-in XRefEntry copies -- not a single template. This is
     * deliberately robust to an unresolved discrepancy: hugearr.h's
     * own docs promise initData gets "copied into each new element"
     * (template replication), but kernel source review (project
     * history) shows InitHAChain actually doing a straight linear
     * copy of numElem*elementSize bytes starting at initData instead
     * -- a real bug against its own documented contract, not just
     * unclear wording. Since every element in batchBuf is already an
     * identical copy of entryTemplate, either behavior produces the
     * same correct result here: true replication would just copy
     * batchBuf[0] repeatedly (same value throughout); linear copying
     * copies batchBuf[0..batchMax-1] directly (already all equal).
     * Filling batchBuf is plain in-process memory work, so building
     * even a few thousand copies is cheap; the expensive part this
     * replaces is the *count* of separate HugeArrayAppend kernel
     * calls, which this cuts from xref->size down to
     * xref->size/batchMax.
     */
    batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
    if (batchMax < 1) {
	batchMax = 1;
    }
    batchBuf = gmalloc( (long) batchMax * sizeof(XRefEntry) );
    for (entryIdx = 0; entryIdx < batchMax; ++entryIdx) {
	batchBuf[entryIdx] = entryTemplate;
    }

    remaining = xref->size;
    while (remaining > 0) {
	batchThis = (remaining > (long) batchMax) ? batchMax : (word) remaining;
	HugeArrayAppend(xref->vmFile, xref->entries, batchThis, batchBuf);
	remaining -= batchThis;
    }
    gfree(batchBuf);

    /*
     * Each section in the /Prev chain can independently be either a
     * classic table or a cross-reference stream (e.g. an older
     * incremental update from before a file was ever saved with
     * PDF 1.5+ tools, underneath newer stream-based sections) -- so
     * the check happens fresh every iteration, not just once up
     * front.
     *
     * isFirstSection (see project optimization analysis P8/V1):
     * xref->entries was just batch-initialized above with every
     * entry starting free/unclaimed, so the FIRST section read here
     * is guaranteed to find every entry it touches still in that
     * initial state -- no need for the careful per-entry "is this
     * already claimed by a higher-priority section" check that
     * later /Prev sections genuinely need (older sections in an
     * incrementally-updated document can and do collide with
     * entries a newer section already claimed). Only the first
     * section gets to skip that check and batch its writes.
     */
    {
      GBool isFirstSection = gTrue;
      while (XRefSectionIsStream(xref, &fs, pos)
	       ? XRefReadXRefStream(xref, &fs, &pos, isFirstSection)
	       : XRefReadXRef(xref, &fs, &pos, isFirstSection))
	  isFirstSection = gFalse;
    }

    // if there was a problem with the xref table,
    // try to reconstruct it
// skip it for now.
    if (!xref->ok) {
      if (xref->entries) HugeArrayDestroy(xref->vmFile, xref->entries);
      xref->size = 0;
      xref->entries = NULL;
//      if (!(ok = constructXRef(str))) {
//	xref = oldXref;
//	return;
//      }
      goto err;
    }
  }

  // set up new xref table
//  xref = this;

  // check for encryption
  xref->encrypted = gFalse;
  if (XRefCheckEncrypted(xref)) {
    /*
     * /Encrypt present doesn't mean "give up" anymore: try deriving
     * the file key assuming an empty user password (the overwhelming
     * majority of "encrypted" PDFs in practice -- /Encrypt used
     * purely to restrict printing/copying, not to keep content
     * secret from a viewer). XRefSetupEncryption returns gFalse for
     * everything this module doesn't handle (a real non-empty
     * password, AES, a non-Standard handler, ...), in which case the
     * behavior is unchanged from before this existed: xref->ok stays
     * gFalse, and PdfOpen's XRefCheckEncrypted(...) ? PDF_ERR_ENCRYPTED
     * check still fires. xref->trailerDict is unaffected either way,
     * same as always.
     */
    if (!XRefSetupEncryption(xref)) {
      xref->ok = gFalse;
      goto err;
    }
  }

  StreamFree(&fs);
  return TRUE;

  err:
  StreamFree(&fs);
  return FALSE;

}	/* End of XRefInit.	*/

void XRefFree(XRef *xref) {

  if (xref->entries) 
      HugeArrayDestroy(xref->vmFile, xref->entries);
  if (xref->vmFile) {
      VMClose(xref->vmFile, FALSE);
      FileDelete(xref->vmFileName);
  }

  ObjFree(&xref->trailerDict);
}

/*
 * Find garbage bytes at start of file
 */
#define headerSearchSize 512

static void XRefSetStart (XRef *xref, Stream *fs) {
  /*
   * Streaming character-by-character match against "%PDF-" instead
   * of reading a 512-byte buffer and then searching it (see project
   * optimization analysis S1) -- matchLen tracks how many leading
   * characters of the pattern currently match; since every
   * character of "%PDF-" is distinct, a mismatch can only ever
   * restart the match at length 0 or 1 (whether the current
   * character happens to equal the pattern's first character), no
   * KMP-style failure table needed for a pattern this simple.
   */
  static const char pattern[] = "%PDF-";
  short matchLen = 0;
  short i;
  long c;

  StreamSetPos(fs, 0);

  for (i = 0; i < headerSearchSize; ++i) {
    c = FStreamGetChar(fs);
    if (c == EOF) {
      break;
    }
    if ((char) c == pattern[matchLen]) {
      ++matchLen;
      if (matchLen == 5) {
	xref->start = i - 4;
	return;
      }
    } else {
      matchLen = ((char) c == pattern[0]) ? 1 : 0;
    }
  }
  EC_WARNING(-1);
//    error(-1, "May not be a PDF file (continuing anyway)");
  xref->start = 0;
}

/***********************************************************************
 *		XRefSectionIsStream
 ***********************************************************************
 * SYNOPSIS:	    Does the xref section at file position `pos` look
 *		    like a classic plain-text table (starts with the
 *		    literal keyword "xref") or a PDF 1.5+ cross-
 *		    reference STREAM (starts with "N G obj" instead)?
 * PARAMETERS:	    XRef *xref, Stream *fs, long pos
 * RETURNS:	    gTrue if it's a stream section, gFalse if classic
 * SIDE EFFECTS:    moves fs's position (caller must not rely on it
 *		    afterward)
 *
 * STRATEGY:	    Only checks for the one thing classic sections are
 *		    guaranteed to start with; doesn't try to positively
 *		    validate "N G obj" shape, that's XRefReadXRefStream's
 *		    job to fail cleanly on if this guess was wrong.
 *
 ***********************************************************************/
static GBool
XRefSectionIsStream(XRef *xref, Stream *fs, long pos)
{
    long c;
    char s[4];
    short i;

    StreamSetPos(fs, xref->start + pos);
    while ((c = StreamGetChar(fs)) != EOF && isspace(c)) ;
    s[0] = (char) c;
    for (i = 1; i < 4; ++i) {
	s[i] = (char) StreamGetChar(fs);
    }
    return !(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f');
}

/***********************************************************************
 *		XRefStreamReadField
 ***********************************************************************
 * SYNOPSIS:	    Read one big-endian, `width`-byte field from an
 *		    already-ObjStreamReset() stream object -- the
 *		    per-entry fields in a cross-reference stream's
 *		    decoded data. width==0 means the field is entirely
 *		    absent from the encoding (per spec); return
 *		    defaultVal without reading anything in that case.
 *
 ***********************************************************************/
static long
XRefStreamReadField(Obj *streamObj, long width, long defaultVal)
{
    long val;
    long i;
    int c;

    if (width <= 0) {
	return defaultVal;
    }
    val = 0;
    for (i = 0; i < width; ++i) {
	c = ObjStreamGetChar(streamObj);
	if (c == EOF) {
	    c = 0;
	}
	val = (val << 8) | (c & 0xff);
    }
    return val;
}

/***********************************************************************
 *		XRefReadTrailerFromStream
 ***********************************************************************
 * SYNOPSIS:	    XRefReadTrailer's job (find /Size and /Root, fill in
 *		    xref->trailerDict) for the case where the section
 *		    at `pos` is a cross-reference STREAM rather than a
 *		    classic table -- streams carry the trailer
 *		    information directly in their own dict, there's no
 *		    separate "trailer" keyword/dict to hunt for.
 * PARAMETERS:	    XRef *xref, Stream *fs, long pos
 * RETURNS:	    `pos` unchanged on success (same convention
 *		    XRefReadTrailer uses: the position the entry-filling
 *		    pass should (re-)start from), 0 on failure
 *
 ***********************************************************************/
static long
XRefReadTrailerFromStream(XRef *xref, Stream *fs, long pos)
{
    Parser parser;
    Lexer *lexer;
    Stream *fs2;
    Obj numObj, genObj, objKwObj, streamObj, fieldObj;
    Dict *dict;

    (void) fs;
    initNull(&numObj);
    fs2 = gmalloc( sizeof(Stream) );
    lexer = gmalloc( sizeof(Lexer) );
    FStreamInit(fs2, xref->fHan, xref->start + pos, -1, &numObj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&numObj);

    ParserGetObj(&parser, &numObj);
    ParserGetObj(&parser, &genObj);
    ParserGetObj(&parser, &objKwObj);
    if (!(isInt(&numObj) && isInt(&genObj) && isCmdSame(&objKwObj, "obj"))) {
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

    if (!isStream(&streamObj)) {
	ObjFree(&streamObj);
	return 0;
    }
    dict = ObjStreamGetDict(&streamObj);

    DictLookupNF(dict, "Size", &fieldObj);
    if (isInt(&fieldObj)) {
	xref->size = getInt(&fieldObj);
    } else {
	ObjFree(&fieldObj);
	ObjFree(&streamObj);
	return 0;
    }
    ObjFree(&fieldObj);

    DictLookupNF(dict, "Root", &fieldObj);
    if (isRef(&fieldObj)) {
	xref->rootNum = getRefNum(&fieldObj);
	xref->rootGen = getRefGen(&fieldObj);
    } else {
	ObjFree(&fieldObj);
	ObjFree(&streamObj);
	return 0;
    }
    ObjFree(&fieldObj);

    /* The dict's KEYS (Info/Encrypt/Root/Size/etc.) become the
     * trailer dict -- a cross-reference stream carries the same
     * trailer information a classic trailer dict would, just as
     * fields on its own stream dict instead of a separate "trailer"
     * block. But xref->trailerDict must end up objDict-typed, NOT a
     * copy of the stream object itself: every consumer everywhere
     * else (XRefCheckEncrypted, XRefGetCatalog, XRefGetDocInfo, ...)
     * calls ObjDictLookup() directly on it, which reads obj->u.dict
     * unconditionally -- for an objStream-typed Obj that union slot
     * actually holds a Stream*, not a Dict*, so every such lookup
     * would silently misinterpret that pointer as a Dict* and read
     * garbage. initDictData() wraps the stream's already-parsed Dict*
     * directly (no re-parsing) but, unlike ObjCopy's objDict case,
     * does NOT bump its refcount itself -- DictIncRef here balances
     * that so the dict survives streamObj's own free below. */
    dict = ObjStreamGetDict(&streamObj);
    ObjFree(&xref->trailerDict);
    initDictData(&xref->trailerDict, dict);
    DictIncRef(dict);

    ObjFree(&streamObj);
    return pos;
}

/***********************************************************************
 *		XRefReadXRefStream
 ***********************************************************************
 * SYNOPSIS:	    Read one PDF 1.5+ cross-reference STREAM section and
 *		    fill in xref->entries[] from it -- the stream
 *		    counterpart to XRefReadXRef (classic table). Chases
 *		    /Prev the same way, and the same
 *		    "first section to speak for an object wins" rule
 *		    applies (checked via each entry's `type`, which
 *		    starts out xrefEntryFree/0 for every object and is
 *		    never touched twice).
 * PARAMETERS:	    XRef *xref, Stream *fs (unused directly -- kept for
 *		    signature symmetry with XRefReadXRef, since both are
 *		    called from the same driving loop), long *pos (in:
 *		    this section's position; out: /Prev's position, if
 *		    any)
 * RETURNS:	    gTrue if there's a /Prev section to read next,
 *		    gFalse otherwise (on error, xref->ok is cleared --
 *		    same contract XRefReadXRef uses to distinguish
 *		    "done" from "failed" for its gFalse return)
 *
 ***********************************************************************/
GBool
XRefReadXRefStream(XRef *xref, Stream *fs, long *pos, GBool isFirstSection)
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
    /*
     * Same batch-write fast path as XRefReadXRef's own isFirstSection
     * handling -- see its comment (project optimization analysis
     * P8/V1).
     */
    word batchMax, batchCount;
    dword batchStart;
    XRefEntry *batchBuf = NULL;

    if (isFirstSection) {
      batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
      if (batchMax < 1) {
        batchMax = 1;
      }
      batchBuf = gmalloc( (long) batchMax * sizeof(XRefEntry) );
      batchCount = 0;
    }

    (void) fs;
    initNull(&numObj);
    fs2 = gmalloc( sizeof(Stream) );
    lexer = gmalloc( sizeof(Lexer) );
    FStreamInit(fs2, xref->fHan, xref->start + *pos, -1, &numObj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&numObj);

    ParserGetObj(&parser, &numObj);
    ParserGetObj(&parser, &genObj);
    ParserGetObj(&parser, &objKwObj);
    if (!(isInt(&numObj) && isInt(&genObj) && isCmdSame(&objKwObj, "obj"))) {
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

    if (!isStream(&streamObj)) {
	ObjFree(&streamObj);
	goto err;
    }
    dict = ObjStreamGetDict(&streamObj);

    DictLookup(dict, "W", &wObj, xref);
    if (!isArray(&wObj) || ObjArrayGetLength(&wObj) < 3) {
	ObjFree(&wObj);
	ObjFree(&streamObj);
	goto err;
    }
    for (i = 0; i < 3; ++i) {
	long wVal;

	ObjArrayGet(&wObj, i, &elemObj, xref);
	wVal = isInt(&elemObj) ? getInt(&elemObj) : 0;
	ObjFree(&elemObj);
	/* XRefStreamReadField combines up to `width` bytes into a
	 * long (max 4 bytes without overflowing it); w[] itself is
	 * now a byte, so an oversized /W entry has to be rejected
	 * here, before the cast, rather than silently truncated. */
	if (wVal < 0 || wVal > 4) {
	    ObjFree(&wObj);
	    ObjFree(&streamObj);
	    goto err;
	}
	w[i] = (byte) wVal;
    }
    ObjFree(&wObj);

    /* /Index: pairs of (startObjNum, count); default is a single pair
     * covering the whole [0, Size) range */
    DictLookup(dict, "Index", &indexObj, xref);
    if (isArray(&indexObj)) {
	numSections = ObjArrayGetLength(&indexObj) >> 1;
    } else {
	numSections = 0;
    }

    ObjStreamReset(&streamObj);

    for (sectionIdx = 0; sectionIdx < (numSections > 0 ? numSections : 1); ++sectionIdx) {
	if (numSections > 0) {
	    ObjArrayGet(&indexObj, sectionIdx * 2, &elemObj, xref);
	    objNum = isInt(&elemObj) ? getInt(&elemObj) : 0;
	    ObjFree(&elemObj);
	    ObjArrayGet(&indexObj, sectionIdx * 2 + 1, &elemObj, xref);
	    count = isInt(&elemObj) ? getInt(&elemObj) : 0;
	    ObjFree(&elemObj);
	} else {
	    objNum = 0;
	    count = xref->size;
	}

	for (i = 0; i < count; ++i, ++objNum) {
	    type = XRefStreamReadField(&streamObj, w[0], 1);
	    f2 = XRefStreamReadField(&streamObj, w[1], 0);
	    f3 = XRefStreamReadField(&streamObj, w[2], 0);

	    if (objNum < 0 || objNum >= xref->size) {
		continue;	/* malformed /Index range -- ignore */
	    }

	    if (isFirstSection) {
		XRefEntry newEntry;

		switch (type) {
		case 1:
		    newEntry.type = xrefEntryClassic;
		    newEntry.used = gTrue;
		    newEntry.offset = f2;
		    newEntry.gen = f3;
		    break;
		case 2:
		    newEntry.type = xrefEntryCompressed;
		    newEntry.used = gTrue;
		    newEntry.offset = f2;	/* containing ObjStm's object number */
		    newEntry.gen = f3;		/* index within that ObjStm */
		    break;
		default:
		    /* type 0 (free) or anything unrecognized -- same
		     * "leave free" value the array was already
		     * batch-initialized with further up this file, so
		     * writing it again here is a harmless no-op, not a
		     * behavior change */
		    newEntry.type = xrefEntryFree;
		    newEntry.used = gFalse;
		    newEntry.offset = -1;
		    newEntry.gen = 0;
		    break;
		}

		/*
		 * Same contiguity check as XRefReadXRef's own
		 * isFirstSection path (see its comment) -- /Index can
		 * (and often does) list non-contiguous sections.
		 */
		if (batchCount > 0 && (dword) objNum != batchStart + batchCount) {
		    HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
		    batchCount = 0;
		}
		if (batchCount == 0) {
		    batchStart = objNum;
		}
		batchBuf[batchCount] = newEntry;
		++batchCount;
		if (batchCount == batchMax) {
		    HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
		    batchCount = 0;
		}
	    } else {
		HugeArrayLock(xref->vmFile, xref->entries, objNum,
			      (void**) &pEntry, &entrySize);
		if (pEntry->type != xrefEntryFree) {
		    HugeArrayUnlock(pEntry);
		    continue;	/* a higher-priority section already
				 * spoke for this object number */
		}

		switch (type) {
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
		    pEntry->offset = f2;	/* containing ObjStm's object number */
		    pEntry->gen = f3;	/* index within that ObjStm */
		    HugeArrayDirty(pEntry);
		    break;
		default:
		    /* type 0 (free) or anything unrecognized -- leave as
		     * xrefEntryFree, nothing to fetch either way */
		    break;
		}
		HugeArrayUnlock(pEntry);
	    }
	}
    }

    /* flush any final partial batch left over after the last section */
    if (batchBuf && batchCount > 0) {
	HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
	batchCount = 0;
    }
    if (batchBuf) {
	gfree(batchBuf);
	batchBuf = NULL;
    }

    ObjFree(&indexObj);

    /* /Prev chases the same way XRefReadXRef's does */
    DictLookupNF(dict, "Prev", &prevObj);
    if (isInt(&prevObj)) {
	*pos = getInt(&prevObj);
	more = gTrue;
    } else {
	more = gFalse;
    }
    ObjFree(&prevObj);

    ObjFree(&streamObj);
    return more;

 err:
    /*
     * NULL-guarded, matching XRefReadXRef's own err2 cleanup -- all
     * four goto err sites above happen before the batching loop
     * runs, so batchBuf here is either NULL (!isFirstSection) or
     * freshly allocated with batchCount==0 (isFirstSection), never
     * something with a pending flush to lose.
     */
    if (batchBuf) {
      if (batchCount > 0) {
        HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
      }
      gfree(batchBuf);
    }
    xref->ok = gFalse;
    EC_WARNING(-1);
    return gFalse;
}

long
XRefReadTrailer (XRef *xref, Stream *fs) {

/* int XRef::readTrailer(FileStream *str) {
*/

  Parser parser;
  Obj obj;
  /*
   * 40 bytes covers this function's two remaining small, bounded
   * reads (a 4-byte "xref" keyword check, and a 35-byte lookahead
   * for either "trailer" or a subsection header's two numbers) --
   * see project optimization analysis S2. The "startxref" backward
   * search that used to need the FULL xrefSearchSize (1024+1)
   * buffer is handled separately below, via a streaming
   * find-the-last-match scan instead of buffering the whole search
   * window at once.
   */
  char buf[40];
  long n, pos, pos1;
  char *p;
  long c;
  long i;
  Obj *ptrailerDict = &xref->trailerDict;
  Stream *fs2;
//  Stream *str;
  Lexer *lexer;

  /*
   * Find the LAST occurrence of "startxref" within the last
   * xrefSearchSize bytes of the file -- streaming forward through
   * the window while remembering the most recent match's end
   * position, rather than buffering the whole window and scanning
   * backward from the end. A match found later in a single forward
   * pass is always closer to the file's end than an earlier one,
   * so "keep the last match seen" gives exactly the same result as
   * a backward scan, without needing xrefSearchSize bytes of stack.
   *
   * fs supports real StreamSetPos seeking here (this is the raw
   * file stream, not a Flate-decompressed one -- see
   * FStreamSetPos), so once the best match is known, the same
   * relative-from-end positioning is simply repeated and read
   * forward again to land on it a second time, rather than trying
   * to compute an absolute file offset by hand.
   */
  {
    static const char startxrefPattern[] = "startxref";
    short matchLen = 0;
    long lastMatchEnd = -1;

    StreamSetPos(fs, -xrefSearchSize);
    for (n = 0; n < xrefSearchSize; ++n) {
      if ((c = StreamGetChar(fs)) == EOF) {
	break;
      }
      if ((char) c == startxrefPattern[matchLen]) {
	++matchLen;
	if (matchLen == 9) {
	  lastMatchEnd = n + 1;
	  matchLen = 0;	/* keep scanning for a LATER match, not the first */
	}
      } else {
	matchLen = ((char) c == startxrefPattern[0]) ? 1 : 0;
      }
    }
    if (lastMatchEnd < 0) {
      return 0;
    }
    StreamSetPos(fs, -xrefSearchSize);
    for (i = 0; i < lastMatchEnd; ++i) {
      StreamGetChar(fs);	/* discard -- just repositioning */
    }
  }
  while (isspace(c = StreamLookChar(fs))) {
    StreamGetChar(fs);
  }
  pos = 0;
  while ((c = StreamLookChar(fs)) >= '0' && c <= '9') {
    pos = pos * 10 + (c - '0');
    StreamGetChar(fs);
  }

  if (XRefSectionIsStream(xref, fs, pos)) {
    return XRefReadTrailerFromStream(xref, fs, pos);
  }

  StreamSetPos(fs, xref->start + pos);
  for (i = 0; i < 4; ++i)
    buf[i] = StreamGetChar(fs);
  if (strncmp(buf, "xref", 4))
    return 0;
  pos1 = pos + 4;
  while (1) {
    StreamSetPos(fs, xref->start + pos1);
    for (i = 0; i < 35; ++i) {
      if ((c = StreamGetChar(fs)) == EOF)
	return 0;
      buf[i] = c;
    }
    if (!strncmp(buf, "trailer", 7))
      break;
    p = buf;
    while (isspace(*p)) ++p;
    while ('0' <= *p && *p <= '9') ++p;
    while (isspace(*p)) ++p;
    n = XRefParseVarDigits(p);
    while ('0' <= *p && *p <= '9') ++p;
    while (isspace(*p)) ++p;
    if (p == buf)
      return 0;
    pos1 += (p - buf) + n * 20;
  }
  pos1 += 7;


  // read trailer dict

  initNull(&obj);

// new Parser(new Lexer(new FileStream(file, start + pos1, -1, &obj)));

  fs2 = gmalloc( sizeof(Stream) );
//  str = gmalloc( sizeof (Stream) );
  lexer = gmalloc( sizeof (Lexer) );

  FStreamInit(fs2, xref->fHan, xref->start + pos1, -1, &obj);
//  StreamInitFS(str, fs2);
  LexerInitFromStream(lexer, fs2, xref);
  ParserInit(&parser, lexer);
  ObjFree(&obj);

  ParserGetObj(&parser, ptrailerDict);
  if (isDict(ptrailerDict)) {
    ObjDictLookupNF(ptrailerDict, "Size", &obj);
    if (isInt(&obj))
      xref->size = getInt(&obj);
    else
      pos = 0;
    ObjFree(&obj);
    ObjDictLookupNF(ptrailerDict, "Root", &obj);
    if (isRef(&obj)) {
      xref->rootNum = getRefNum(&obj);
      xref->rootGen = getRefGen(&obj);
    } else {
      pos = 0;
    }
    ObjFree(&obj);
  } else {
    pos = 0;
  }
  ParserFree(&parser);

  // return first xref position
  return pos;

}	/* End of ReadTrailer.	*/

/***********************************************************************
 *		XRefParseDigits
 ***********************************************************************
 * SYNOPSIS:	    Parse exactly `count` ASCII decimal digits starting
 *		    at `p`, straight into a long -- no intermediate
 *		    null-terminated copy, no atol(). Used for the fixed-
 *		    width fields (offset, generation) of a classic xref
 *		    entry line, which is read into a buffer as a whole
 *		    anyway (its trailing 'n'/'f' byte needs checking), so
 *		    unlike XRefReadXRef's `first`/`n` header numbers or
 *		    XRefFetchFromObjStm's pairOffset (both accumulated
 *		    directly during their own read loop instead, no
 *		    buffer involved at all), there's already a buffer
 *		    here to parse out of directly.
 * PARAMETERS:	    const char *p, int count (both already known to be
 *		    ASCII digits -- caller's responsibility, matches
 *		    every call site here reading a validated fixed-
 *		    format field)
 ***********************************************************************/
static long
XRefParseDigits(const char *p, int count)
{
    long value = 0;

    while (count--) {
	value = value * 10 + (*p++ - '0');
    }
    return value;
}

/***********************************************************************
 *		XRefParseVarDigits
 ***********************************************************************
 * SYNOPSIS:	    Parse a run of ASCII decimal digits starting at
 *		    `p`, straight into a long -- no intermediate
 *		    null-terminated copy, no atol(). Unlike
 *		    XRefParseDigits (fixed field width, known in
 *		    advance), this is for the remaining atol() call
 *		    sites that parse a variable-length digit run
 *		    embedded in a larger already-read buffer (the
 *		    startxref byte offset, and a page-count-ish field
 *		    in the trailer-search fallback loop) -- stops at
 *		    the first non-digit character, same as atol()
 *		    would for a purely-numeric prefix, which is all
 *		    these call sites ever feed it (already validated
 *		    by an isdigit() check or an isspace()-skip loop
 *		    immediately before the call).
 * PARAMETERS:	    const char *p -- need not be digits already; stops
 *		    cleanly (returns 0) if *p isn't a digit at all
 ***********************************************************************/
static long
XRefParseVarDigits(const char *p)
{
    long value = 0;

    while (*p >= '0' && *p <= '9')
	    value = value * 10 + (*p++ - '0');

    return value;
}

GBool
XRefReadXRef(XRef *xref, Stream *fs, long *pos, GBool isFirstSection)
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
     * Batch-write support for the isFirstSection fast path (see
     * project optimization analysis P8/V1) -- batchBuf accumulates
     * real, filled-in XRefEntry copies (not a single template,
     * matching the same HugeArrayAppend-vs-documentation discrepancy
     * noted where xref->entries was first batch-initialized further
     * up this file) for a contiguous run of entries, flushed with one
     * HugeArrayReplace call instead of one HugeArrayLock/Unlock cycle
     * per entry.
     */
    word batchMax, batchCount;
    dword batchStart;
    XRefEntry *batchBuf = NULL;
    XRefEntry *pBatch;

    if (isFirstSection) {
        batchMax = HUGEARRAY_BATCH_MAX_BYTES / sizeof(XRefEntry);
        if (batchMax < 1) {
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
    if (!(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f')) {
        goto err2;
    }

    /* read xref */

    while (1) {
        while ((c = StreamLookChar(fs)) != EOF && isspace(c)) {
            StreamGetChar(fs);
        }
        if (c == 't') {
            break;
        }
        first = 0;
        for (i = 0; (c = StreamGetChar(fs)) != EOF && isdigit(c) && i < 20; ++i) {
            first = first * 10 + (c - '0');
        }
        if (i == 0) {
            goto err2;
        }
        while ((c = StreamLookChar(fs)) != EOF && isspace(c)) {
            StreamGetChar(fs);
        }
        n = 0;
        for (i = 0; (c = StreamGetChar(fs)) != EOF && isdigit(c) && i < 20; ++i) {
            n = n * 10 + (c - '0');
        }
        if (i == 0) {
            goto err2;
        }
        while ((c = StreamLookChar(fs)) != EOF && isspace(c)) {
            StreamGetChar(fs);
        }
        for (i = first; i < first + n; ++i) {
            for (j = 0; j < 20; ++j) {
                if ((c = StreamGetChar(fs)) == EOF) {
                    goto err2;
                }
                s[j] = (char)c;
            }
            if (i < 0 || i >= xref->size) {
                continue; /* malformed table -- ignore, matches
                           * the stream-based path's same guard.
                           * If isFirstSection, the contiguity
                           * check below correctly flushes any
                           * pending batch before the resulting
                           * gap. */
            }

            if (isFirstSection) {
                /*
                 * Subsections can be (and often are) non-contiguous with
                 * each other -- e.g. "0 5" then "100 3" -- and a malformed
                 * table can also produce a gap via the out-of-range skip
                 * just above. Either way, a batch can only ever be flushed
                 * as one truly consecutive run, so check contiguity against
                 * whatever's already accumulated before adding this entry;
                 * flush first if it doesn't line up.
                 */
                if (batchCount > 0 && (dword)i != batchStart + batchCount) {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
                    batchCount = 0;
                }
                if (batchCount == 0) {
                    batchStart = i;
                }
                pBatch = &batchBuf[batchCount];
                pBatch->offset = XRefParseDigits(s, 10);
                pBatch->gen = XRefParseDigits(&s[11], 5);
                pBatch->type = xrefEntryClassic;
                if (s[17] == 'n') {
                    pBatch->used = gTrue;
                } else if (s[17] == 'f') {
                    pBatch->used = gFalse;
                } else {
                    /* malformed -- flush whatever was already valid, matching
                     * the original per-entry path's own "commit what's already
                     * dirtied, then bail" behavior, rather than silently
                     * discarding it */
                    if (batchCount > 0) {
                        HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
                    }
                    gfree(batchBuf);
                    batchBuf = NULL;
                    goto err2;
                }
                ++batchCount;
                if (batchCount == batchMax) {
                    HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
                    batchCount = 0;
                }
            } else {
                HugeArrayLock(xref->vmFile, xref->entries, i, (void **)&pEntry, &entrySize);
                if (pEntry->offset < 0) {
                    pEntry->offset = XRefParseDigits(s, 10);
                    pEntry->gen = XRefParseDigits(&s[11], 5);
                    pEntry->type = xrefEntryClassic;
                    if (s[17] == 'n') {
                        pEntry->used = gTrue;
                    } else if (s[17] == 'f') {
                        pEntry->used = gFalse;
                    } else {
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
    if (batchBuf && batchCount > 0) {
        HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
        batchCount = 0;
    }
    if (batchBuf) {
        gfree(batchBuf);
        batchBuf = NULL;
    }

    /* read prev pointer from trailer dictionary */
    initNull(&obj);

    fs2 = gmalloc(sizeof(Stream));
    lexer = gmalloc(sizeof(Lexer));

    FStreamInit(fs2, xref->fHan, FStreamGetPos(fs), -1, &obj);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    ObjFree(&obj);

    ParserGetObj(&parser, &obj);
    if (!isCmdSame(&obj, "trailer")) {
        goto err1;
    }
    ObjFree(&obj);
    ParserGetObj(&parser, &obj);
    if (!isDict(&obj)) {
        goto err1;
    }
    DictLookupNF(getDict(&obj), "Prev", &obj2);
    if (isInt(&obj2)) {
        *pos = getInt(&obj2);
        more = gTrue;
    } else {
        more = gFalse;
    }
    ObjFree(&obj);
    ObjFree(&obj2);

    ParserFree(&parser);
    return more;

err1:
    ObjFree(&obj);
err2:
    /*
     * Catches every goto err2 path that didn't already flush+free
     * batchBuf itself (e.g. a malformed first/n subsection header,
     * reached before the per-entry loop's own handling runs) --
     * NULL-guarded so it's a no-op wherever a specific error path
     * already did this and set batchBuf back to NULL.
     */
    if (batchBuf) {
        if (batchCount > 0) {
            HugeArrayReplace(xref->vmFile, xref->entries, batchCount, batchStart, batchBuf);
        }
        gfree(batchBuf);
    }
    xref->ok = gFalse;
    EC_WARNING(-1);
    return gFalse;

}


GBool XRefCheckEncrypted(XRef *xref) {
  Obj obj;
  GBool encrypted;

  ObjDictLookup(&xref->trailerDict, "Encrypt", &obj, xref);
  encrypted = !isNull(&obj);

  ObjFree(&obj);
  return encrypted;
}

/***********************************************************************
 *		XRefFetchFromObjStm
 ***********************************************************************
 * SYNOPSIS:	    Fetch object index `idxInStm` out of Object Stream
 *		    `objStmNum` -- the type-2 (compressed) counterpart
 *		    to XRefFetch's classic file-seek path below.
 * PARAMETERS:	    XRef *xref, long objStmNum (containing ObjStm's own
 *		    object number), long idxInStm (this object's
 *		    0-based index within it), Obj *obj (out)
 * SIDE EFFECTS:    none beyond the usual object-graph traffic
 *
 * STRATEGY:	    Per spec (7.5.7): an Object Stream's decoded content
 *		    starts with N "objNum offset" pairs as plain-text
 *		    ASCII integers, followed by /First and then the N
 *		    objects' actual values back to back at those
 *		    offsets. ObjStms can't contain other ObjStms per
 *		    spec, so the XRefFetch call below can't recurse
 *		    back into this function; their contents are always
 *		    generation 0, so (unlike the classic path) there's
 *		    no "num gen obj" header inside to validate against,
 *		    just the bare value.
 *
 *		    Getting from "just read the header" to "now parse
 *		    the target object" took three attempts to get right:
 *
 *		    1. StreamSetPos to the target byte offset, then a
 *		       second LexerInitFromStream: LexerInitFromStream
 *		       unconditionally calls ObjStreamReset, which jumps
 *		       straight back to position 0 -- the seek never
 *		       survived.
 *		    2. Reuse the SAME Parser across a StreamSetPos
 *		       instead: Parser keeps its own one-token lookahead
 *		       (buf1/buf2, filled eagerly by ParserInit), so it
 *		       kept handing back an already-buffered header
 *		       token instead of reading fresh from the
 *		       repositioned stream.
 *		    3. Wrap the stream in a SubStream before the second
 *		       LexerInitFromStream (SubStreamReset deliberately
 *		       doesn't propagate a reset to the wrapped stream --
 *		       see main/stream.goc) so the reset is a no-op on
 *		       position, with a genuinely fresh Parser so there's
 *		       no stale lookahead either. This fixed the reset
 *		       problem, but StreamSetPos turned out to be the
 *		       real remaining bug: it only has a real
 *		       implementation for strFile. For any other kind
 *		       (strFlate very much included -- Object Streams are
 *		       almost always Flate-compressed) it just recurses
 *		       into the *wrapped* stream (`StreamSetPos(str->str,
 *		       pos1)`), i.e. seeks the raw, still-compressed file
 *		       bytes to a byte offset that was computed against
 *		       the DECOMPRESSED content -- a meaningless
 *		       operation. It doesn't fail loudly; it just hands
 *		       back whatever garbage sits at that unrelated file
 *		       position.
 *
 *		    The only operation that actually advances a Flate
 *		    stream correctly is reading it, one character at a
 *		    time, letting the decompressor do its job -- there is
 *		    no shortcut. So: read the header manually (not via
 *		    Parser -- its lookahead buffering makes it impossible
 *		    to know precisely how many raw bytes it has actually
 *		    consumed), keep an exact running byte count via
 *		    ObjStreamGetCharCounted, and once the target's offset
 *		    is known, keep reading forward (discarding the
 *		    output) on that SAME stream until the count reaches
 *		    First + targetOffset. Only then hand the
 *		    now-correctly-positioned stream to a fresh Lexer/
 *		    Parser -- wrapped in a SubStream so its
 *		    ObjStreamReset doesn't undo the positioning, per
 *		    point 3 above.
 *
 *		    The Lexer wraps the ObjStm Obj's own Stream* directly
 *		    rather than building a fresh FStreamInit at a file
 *		    position like every other Lexer/Parser construction in
 *		    this file -- decoded stream content has no file offset
 *		    of its own to re-open. LexerInitFromStream doesn't take
 *		    its own reference (initStream() just wraps the pointer,
 *		    no refcount bump), so StreamIncRef is called explicitly
 *		    first: without it, ParserFree's eventual ObjFree on the
 *		    Lexer's copy would drop the *only* reference the real
 *		    owner (objStmObj, freed further down) still needs,
 *		    freeing the stream out from under it. The SubStream
 *		    wrapper needs no such increment itself: it's a fresh,
 *		    single-owner heap Stream that only the second Lexer
 *		    ever references, and SubStreamFree doesn't free the
 *		    stream it wraps.
 *
 ***********************************************************************/
/***********************************************************************
 *		ObjStreamGetCharCounted
 ***********************************************************************
 * SYNOPSIS:	    ObjStreamGetChar, but also increments a running
 *		    byte-position counter -- used instead of raw
 *		    ObjStreamGetChar everywhere below, so *every*
 *		    character read is counted exactly once no matter
 *		    where the call appears (a loop body, or a loop
 *		    *condition* that reads one character ahead to decide
 *		    whether to keep going). Manually sprinkling
 *		    `++consumed` only inside loop bodies undercounts: the
 *		    character that makes a `while (... = GetChar() ...)`
 *		    condition go false has still been read off the stream
 *		    even though the loop body never runs for it.
 *
 ***********************************************************************/
static int
ObjStreamGetCharCounted(Obj *objP, long *consumed)
{
    int c;

    c = ObjStreamGetChar(objP);
    if (c != EOF) {
	++(*consumed);
    }
    return c;
}

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
     * 1-entry cache of the most recently read ObjStm's header (see
     * project optimization analysis V2): the N "objNum offset" pairs
     * at the start of every Object Stream are identical across every
     * object fetched from that same stream, but were previously
     * re-scanned character-by-character from position 0 on EVERY
     * single call -- wasteful when a page tree (or anything else)
     * pulls several objects out of the same ObjStm in a row, which
     * is the common case ObjStm exists for in the first place.
     *
     * Deliberately narrow in scope: only the header-scanning loop
     * below is skipped on a cache hit. The forward-only stream
     * positioning after it (finding First+targetOffset, handing off
     * to a fresh Lexer/Parser) is left completely untouched -- see
     * this function's own STRATEGY comment above for why that part
     * took three attempts to get right the first time; not worth
     * the risk of touching it again for this.
     */
    static XRef *cachedXref = NULL;
    static long cachedObjStmNum = -1;
    static long cachedN = 0;
    static long cachedFirst = 0;
    static long *cachedPairOffsets = NULL;

    XRefFetch(xref, objStmNum, 0, &objStmObj);
    if (!isStream(&objStmObj)) {
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

    if (idxInStm < 0 || idxInStm >= n) {
	ObjFree(&objStmObj);
	initNull(obj);
	return;
    }

    ObjStreamReset(&objStmObj);
    consumed = 0;
    targetOffset = -1;

    cacheHit = (xref == cachedXref && objStmNum == cachedObjStmNum && n == cachedN
		&& first == cachedFirst && cachedPairOffsets != NULL);

    if (cacheHit) {
	/*
	 * Same ObjStm as last time, with a matching element count --
	 * already know every pair's offset from the cache, so skip
	 * straight to knowing targetOffset without scanning any
	 * header text. ObjStreamReset above still ran (needed either
	 * way: objStmObj is a fresh fetch each call, see XRefFetch at
	 * the top), so `consumed` correctly starts at 0 for the
	 * forward-read step below, exactly as the cache-miss path
	 * leaves it.
	 */
	targetOffset = cachedPairOffsets[idxInStm];
    } else {
	/*
	 * Cache miss (different ObjStm, or first call ever): scan the
	 * header for real, same as always -- but also record every
	 * pair's offset into a freshly (re)allocated cache array, not
	 * just the one this call actually needs, so a later call for
	 * a *different* index into this same ObjStm can be served
	 * from cache too.
	 */
	if (cachedPairOffsets) {
	    gfree(cachedPairOffsets);
	}
	cachedPairOffsets = (n > 0) ? (long *) gmalloc(n * sizeof(long)) : NULL;
	cachedXref = xref;
	cachedObjStmNum = objStmNum;
	cachedN = n;
	cachedFirst = first;

	for (i = 0; i < n; ++i) {
	    /* objNum of this pair -- not needed, we already know which
	     * object we want by its position (idxInStm), not by matching
	     * numbers, so just skip past it */
	    c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    while (c != EOF && isspace(c)) {
		c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    }
	    while (c != EOF && isdigit(c)) {
		c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    }

	    c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    while (c != EOF && isspace(c)) {
		c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    }
	    pairOffset = 0;
	    for (j = 0; c != EOF && isdigit(c) && j < 31; ++j) {
		pairOffset = pairOffset * 10 + (c - '0');
		c = ObjStreamGetCharCounted(&objStmObj, &consumed);
	    }

	    if (cachedPairOffsets) {
		cachedPairOffsets[i] = pairOffset;
	    }
	    if (i == idxInStm) {
		targetOffset = pairOffset;
	    }
	}
    }

    if (targetOffset < 0) {
	ObjFree(&objStmObj);
	initNull(obj);
	return;
    }

    /* `consumed` is an exact byte count of everything read so far
     * (see ObjStreamGetCharCounted above). /First is the
     * spec-authoritative start of object data; keep reading forward
     * -- the only operation that correctly advances a Flate stream,
     * see STRATEGY above -- to First + targetOffset. If a malformed
     * file already overshot that point, don't try to go backward
     * (this stream can only read forward); the subsequent parse will
     * simply fail cleanly on garbage instead. */
    while (consumed < first + targetOffset) {
	if (ObjStreamGetChar(&objStmObj) == EOF) {
	    break;
	}
	++consumed;
    }

    /* Now correctly positioned. Wrap in a SubStream and hand to a
     * fresh Lexer/Parser -- see STRATEGY above for why a bare
     * LexerInitFromStream directly on the stream, or reusing the
     * header-reading Parser, both fail here. */
    str = getStream(&objStmObj);

    subStr = gmalloc( sizeof(Stream) );
    initNull(&dummyDict);
    SubStreamInit(subStr, str, &dummyDict);
    ObjFree(&dummyDict);

    lexer = gmalloc( sizeof(Lexer) );
    LexerInitFromStream(lexer, subStr, xref);
    ParserInit(&parser, lexer);
    ParserGetObj(&parser, obj);
    ParserFree(&parser);

    ObjFree(&objStmObj);
}

void XRefFetch(XRef *xref, long num, long gen, Obj *obj) {
  XRefEntry *e;
  Parser parser;
  Obj obj1, obj2, obj3;
  word entrySize;
  short entryType;
  long entryOffset, entryGen;

  Stream *fs2;
//  Stream *str;
  Lexer *lexer;


  // check for bogus ref - this can happen in corrupted PDF files
  if (num < 0 || num >= xref->size) {
    initNull(obj);
    return ;
  }

/* lock down just this one entry -- XRefFetchFromObjStm below recurses
 * back into XRefFetch (to fetch the containing Object Stream), which
 * locks its own entry independently, so nothing here needs to still
 * be held across that call */

  HugeArrayLock(xref->vmFile, xref->entries, num, (void**) &e, &entrySize);
  entryType = e->type;
  entryOffset = e->offset;
  entryGen = e->gen;
  HugeArrayUnlock(e);

  if (entryType == xrefEntryCompressed) {
    if (gen != 0) {
      /* compressed objects are always generation 0 per spec; a
       * reference asking for anything else doesn't match */
      initNull(obj);
      return;
    }
    XRefFetchFromObjStm(xref, entryOffset, entryGen, obj);
    return;
  }

  if (entryType == xrefEntryClassic && entryGen == gen && entryOffset >= 0) {
    initNull(&obj1);

    fs2 = gmalloc( sizeof(Stream) );
    lexer = gmalloc( sizeof (Lexer) );

    FStreamInit(fs2, xref->fHan, xref->start + entryOffset, -1, &obj1);
    LexerInitFromStream(lexer, fs2, xref);
    ParserInit(&parser, lexer);
    /* the only Parser in the whole codebase that corresponds to one
     * specific indirect object -- safe to enable decryption here,
     * using exactly this object's own num/gen (Algorithm 1 needs
     * both). See crypt.goh/ParserInit's comment for why every other
     * Parser must stay noCrypt. */
    parser.cryptNum = num;
    parser.cryptGen = gen;
    parser.noCrypt = !xref->encrypted;
    ObjFree(&obj1);

    ParserGetObj(&parser, &obj1);
    ParserGetObj(&parser, &obj2);
    ParserGetObj(&parser, &obj3);
    if (isInt(&obj1) && getInt(&obj1) == num &&
	isInt(&obj2) && getInt(&obj2) == gen &&
	isCmdSame(&obj3, "obj")) {
      ParserGetObj(&parser, obj);
    } else {
      initNull(obj);
    }
    ObjFree(&obj1);
    ObjFree(&obj2);
    ObjFree(&obj3);
    ParserFree(&parser);
  } else {
    initNull(obj);
  }
}

void XRefGetDocInfo(XRef *xref, Obj *obj) {

  ObjDictLookup(&xref->trailerDict, "Info", obj, xref);
}

  // Is xref table valid?
  GBool XRefIsOk(XRef *xref) { return xref->ok; }

  // Get catalog object.
  void XRefGetCatalog(XRef *xref, Obj *obj) { 

	  XRefFetch(xref, xref->rootNum, xref->rootGen, obj); 
  }

 GBool XRefOkToPrint(XRef *xref) {
  (void) xref;
  return gTrue;
}

 GBool XRefOkToCopy(XRef *xref) {
  (void) xref;
  return gTrue;
}
