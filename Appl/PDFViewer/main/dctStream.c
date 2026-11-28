/***********************************************************************
 *
 * PROJECT:     PDF Viewer
 * FILE:        main/dctStream.c
 *
 * DESCRIPTION:
 *      DCTDecode filter (JPEG) on top of the IJG JPEG library (ijgjpeg).
 *
 *      Kept in its own file because jpeglib.h/jmorecfg.h define very
 *      generic macros (GLOBAL, LOCAL, FAR, boolean, ...).
 *
 *      Memory: Init only allocates a few bytes. The decoder (libjpeg
 *      object, input buffer, row buffer) is created by Reset, i.e. when
 *      the image is really read, and released as soon as the last row
 *      has been delivered -- the parser creates a filter chain for every
 *      stream object it meets, most are never read.
 *
 *      Errors: libjpeg ends every fatal error in error_exit, which
 *      (with an error handler context) jumps back to the last
 *      set_error_handler call (JpegErr/jpegerrManager.asm), much like longjmp.
 *      Every libjpeg call that can fail is therefore wrapped in a
 *      function that calls set_error_handler first. After the error
 *      return only si, di, bp, ds and sp are valid again, so the error
 *      branches only use the function parameter.
 *
 *      Callbacks (source manager, output_message) are called by the
 *      library through ProcCallFixedOrMovable_pascal: they have to be
 *      _pascal, and _export (= __loadds) so DS is our dgroup again.
 *
 *      Output is the same as with the old built-in decoder: interleaved
 *      8 bit samples, gray / RGB / CMYK. YCbCr is converted to RGB and
 *      YCCK to (Adobe style, inverted) CMYK by libjpeg; inverted CMYK is
 *      handled by the image's /Decode array as before.
 *
 ***********************************************************************/

#include <geos.h>
#include <heap.h>
#include <ec.h>
#include <Ansi/string.h>
#include <Ansi/stdio.h>
#include "obj.h"
#include "stream.h"
#include "gmem.h"
#include <jpeglib.h>

/* compressed bytes per StreamRead */
#define DCT_IN_BUF_SIZE 1024
/* rows decoded per jpeg_read_scanlines call (and per error handler) */
#define DCT_MAX_LINES_PER_FILL 16
#define DCT_FILL_BYTES 8192
/*
 * JERR_OUT_OF_MEMORY from the library's private jerror.h (position in
 * its message table; the table is fixed for libjpeg 6a).
 */
#define DCT_JERR_OUT_OF_MEMORY 53

/*
 * Size limits, checked right after the header (before libjpeg allocates
 * its buffers). libjpeg keeps its working buffers in fixed malloc memory:
 * for baseline JPEGs a few rows of the OUTPUT size (about output width *
 * components * 16 * 3 bytes), for progressive JPEGs additionally all DCT
 * coefficients of the full image (2 bytes per sample; above
 * max_memory_to_use = 64K they go to a backing store temp file, which is
 * slow). Images above the limits are skipped instead of failing with
 * "out of memory" in the middle of the page.
 *
 * Large images are decoded at 1/2, 1/4 or 1/8 size (libjpeg scaled IDCT,
 * see DCTStreamScaleFor): the stream then delivers the smaller image,
 * GfxDrawImage reads it with the same reduced width and height.
 * DCT_MAX_ROW_BYTES applies to the scaled row.
 * ATTENTION: The coefficient limit of progressive JPEGs does not change
 * with scaling (libjpeg keeps all coefficients at full size). Upgrade
 * path: more memory in PM.
 */
#define DCT_TARGET_DIMENSION 1500L          /* see GfxDrawImage sampling */
#define DCT_MAX_ROW_BYTES 12288L            /* output width * components */
#define DCT_MAX_PROGRESSIVE_COEF (4L * 1024L * 1024L)  /* coefficient bytes */

/* in JpegErr/jpegerrManager.asm (fixed resource) */
extern word set_error_handler(word *context);

typedef struct DCTDecoder
{
    struct jpeg_decompress_struct cinfo;   /* first: callbacks cast back */
    struct jpeg_error_mgr jerr;
    struct jpeg_source_mgr src;
    Stream *in;                 /* compressed data */
    GBool created;              /* jpeg_create_decompress done */
    GBool inputEnded;           /* compressed data ran out (fake EOI) */
    MemHandle bufHan;           /* decoded rows, locked while decoding */
    Guchar *buf;
    word rowBytes;
    word linesPerFill;
    word bufIdx;
    word bufCount;
    JSAMPROW rowPtr[DCT_MAX_LINES_PER_FILL];
    JOCTET inBuf[DCT_IN_BUF_SIZE];
} DCTDecoder;

typedef struct DCTData
{
    DCTDecoder *dec;            /* NULL while not reading */
    GBool eof;
} DCTData;

static void DCTStreamEnd(DCTData *d);
static GBool DCTStreamStart(DCTDecoder *dec);
static GBool DCTStreamSizeOk(struct jpeg_decompress_struct *cinfo);
static GBool DCTStreamFill(DCTDecoder *dec);
static void DCTStreamFailed(DCTDecoder *dec);

static void _pascal _export DCTSrcInit(j_decompress_ptr cinfo)
{
    (void)cinfo;
}

/********************************************************************
 *                      DCTSrcFill
 ********************************************************************
 * SYNOPSIS:      libjpeg source callback: read the next block of compressed
 *                data.
 *
 * PARAMETERS:    cinfo                 Ptr to the decompressor (first member of
 *                                      DCTDecoder).
 *
 * RETURNS:       boolean               Always TRUE.
 *
 * STRATEGY:      At the end of the data insert a fake EOI marker.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static boolean _pascal _export DCTSrcFill(j_decompress_ptr cinfo)
{
    DCTDecoder *dec = (DCTDecoder *)cinfo;
    word n;

    n = StreamRead(dec->in, dec->inBuf, DCT_IN_BUF_SIZE);
    if (n == 0)
    {
        /* End of data: fake EOI; DCTStreamFill stops after these rows */
        dec->inputEnded = gTrue;
        dec->inBuf[0] = 0xFF;
        dec->inBuf[1] = JPEG_EOI;
        n = 2;
    }
    dec->src.next_input_byte = dec->inBuf;
    dec->src.bytes_in_buffer = n;
    return TRUE;
}

/********************************************************************
 *                      DCTSrcSkip
 ********************************************************************
 * SYNOPSIS:      libjpeg source callback: skip compressed data.
 *
 * PARAMETERS:    cinfo                 Ptr to the decompressor.
 *                numBytes              Number of bytes to skip.
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
static void _pascal _export DCTSrcSkip(j_decompress_ptr cinfo, long numBytes)
{
    DCTDecoder *dec = (DCTDecoder *)cinfo;

    if (numBytes <= 0)
        return;

    while (numBytes > (long)dec->src.bytes_in_buffer)
    {
        numBytes -= (long)dec->src.bytes_in_buffer;
        DCTSrcFill(cinfo);
    }
    dec->src.next_input_byte += (size_t)numBytes;
    dec->src.bytes_in_buffer -= (size_t)numBytes;
}

static void _pascal _export DCTSrcTerm(j_decompress_ptr cinfo)
{
    (void)cinfo;
}

static void _pascal _export DCTOutputMessage(j_common_ptr cinfo)
{
    (void)cinfo;
}

/********************************************************************
 *                      DCTStreamInit
 ********************************************************************
 * SYNOPSIS:      Initialize a DCT (JPEG) filter stream.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
 *                *str1                 Ptr to the stream with the compressed
 *                                      data.
 *
 * RETURNS:       void
 *
 * STRATEGY:      The decoder is made in the first reset: the parser makes the
 *                filters of every stream object, also of streams that are never
 *                read.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void DCTStreamInit(Stream *str, Stream *str1)
{
    DCTData *d;

    StreamInit(str);
    str->str = str1;

    d = gmalloc(sizeof(DCTData));
    if (!d)
    {
        EOFStreamInit(str, str1);
        return;
    }
    d->dec = NULL;
    d->eof = gTrue;     /* nothing to read until the first reset */
    str->kind = strDCT;
    str->u.dct = d;
}

/********************************************************************
 *                      DCTStreamFree
 ********************************************************************
 * SYNOPSIS:      Free a DCT stream and its decoder.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
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
void DCTStreamFree(Stream *str)
{
    if (str->u.dct)
    {
        DCTStreamEnd(str->u.dct);
        gfree(str->u.dct);
        str->u.dct = NULL;
    }
}

/********************************************************************
 *                      DCTStreamReset
 ********************************************************************
 * SYNOPSIS:      Start decoding from the beginning.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
 *
 * RETURNS:       void
 *
 * STRATEGY:      Free an old decoder, make a new one and read the JPEG header.
 *                On error the stream stays at EOF.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
void DCTStreamReset(Stream *str)
{
    DCTData *d = str->u.dct;
    DCTDecoder *dec;

    DCTStreamEnd(d);
    StreamReset(str->str);

    dec = gmalloc(sizeof(DCTDecoder));
    if (!dec)
        return;

    memset(dec, 0, sizeof(DCTDecoder));
    dec->in = str->str;
    d->dec = dec;

    if (!DCTStreamStart(dec))
    {
        DCTStreamEnd(d);
        return;
    }
    d->eof = gFalse;
}

/********************************************************************
 *                      DCTStreamHaveData
 ********************************************************************
 * SYNOPSIS:      Make sure that decoded bytes are available.
 *
 * PARAMETERS:    *d                    Ptr to the DCT data.
 *
 * RETURNS:       GBool                 gFalse at the end of the image or on
 *                                      error.
 *
 * STRATEGY:      Decode the next rows when the buffer is empty. After the last
 *                row the decoder is freed at once.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static GBool DCTStreamHaveData(DCTData *d)
{
    if (d->eof)
        return gFalse;

    if (d->dec->bufIdx < d->dec->bufCount)
        return gTrue;

    if (!DCTStreamFill(d->dec))
    {
        /* last row delivered (or error): give the memory back now */
        DCTStreamEnd(d);
        return gFalse;
    }
    return gTrue;
}

/********************************************************************
 *                      DCTStreamGetChar
 ********************************************************************
 * SYNOPSIS:      Read the next decoded byte.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
 *
 * RETURNS:       short                 Byte, EOF at the end.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
short DCTStreamGetChar(Stream *str)
{
    DCTData *d = str->u.dct;

    if (!DCTStreamHaveData(d))
        return EOF;

    return d->dec->buf[d->dec->bufIdx++];
}

/********************************************************************
 *                      DCTStreamLookChar
 ********************************************************************
 * SYNOPSIS:      Return the next decoded byte but do not read it.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
 *
 * RETURNS:       short                 Byte, EOF at the end.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
short DCTStreamLookChar(Stream *str)
{
    DCTData *d = str->u.dct;

    if (!DCTStreamHaveData(d))
        return EOF;

    return d->dec->buf[d->dec->bufIdx];
}

/********************************************************************
 *                      DCTStreamRead
 ********************************************************************
 * SYNOPSIS:      Read a block of decoded bytes.
 *
 * PARAMETERS:    *str                  Ptr to the DCT stream.
 *                *buf                  Ptr to the destination.
 *                size                  Number of bytes.
 *
 * RETURNS:       word                  Number of bytes read, less at the end.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
word DCTStreamRead(Stream *str, Guchar *buf, word size)
{
    DCTData *d = str->u.dct;
    word done = 0, chunk;

    while (done < size && DCTStreamHaveData(d))
    {
        chunk = d->dec->bufCount - d->dec->bufIdx;
        if (chunk > size - done)
            chunk = size - done;

        memcpy(buf + done, d->dec->buf + d->dec->bufIdx, chunk);
        d->dec->bufIdx += chunk;
        done += chunk;
    }
    return done;
}

/********************************************************************
 *                      DCTStreamEnd
 ********************************************************************
 * SYNOPSIS:      Free the decoder; the stream is at EOF.
 *
 * PARAMETERS:    *d                    Ptr to the DCT data.
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
static void DCTStreamEnd(DCTData *d)
{
    DCTDecoder *dec = d->dec;

    d->eof = gTrue;
    if (!dec)
        return;

    if (dec->created)
    {
        /* safe after error_exit too: libjpeg checks cinfo->mem */
        jpeg_destroy_decompress(&dec->cinfo);
        dec->created = gFalse;
    }
    if (dec->bufHan)
        MemFree(dec->bufHan);

    gfree(dec);
    d->dec = NULL;
}

/********************************************************************
 *                      DCTStreamFailed
 ********************************************************************
 * SYNOPSIS:      Clean up after a libjpeg error exit.
 *
 * PARAMETERS:    *dec                  Ptr to the decoder.
 *
 * RETURNS:       void
 *
 * STRATEGY:      A libjpeg out-of-memory error sets the out-of-memory flag.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static void DCTStreamFailed(DCTDecoder *dec)
{
    jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, 0);
    if (dec->jerr.msg_code == DCT_JERR_OUT_OF_MEMORY)
        GMemSetError();

    EC_WARNING(-1);
}

/********************************************************************
 *                      DCTStreamSizeOk
 ********************************************************************
 * SYNOPSIS:      Check the image size and set the scaling.
 *
 * PARAMETERS:    *cinfo                Ptr to the decompressor after
 *                                      jpeg_read_header.
 *
 * RETURNS:       GBool                 gFalse if the image is too large for
 *                                      real mode.
 *
 * STRATEGY:      Large images are decoded at 1/2, 1/4 or 1/8 size. The row size
 *                and, for progressive JPEGs, the coefficient memory are
 *                limited.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static GBool DCTStreamSizeOk(struct jpeg_decompress_struct *cinfo)
{
    long w = (long)cinfo->image_width;
    long h = (long)cinfo->image_height;
    long nc = (long)cinfo->num_components;
    long d;

    if (w < 1 || h < 1 || nc < 1 || nc > 4)
    {
        return gFalse;
    }
    d = DCTStreamScaleFor(w, h);
    cinfo->scale_num = 1;
    cinfo->scale_denom = (unsigned int)d;

    if (((w + d - 1) / d) * nc > DCT_MAX_ROW_BYTES)
        return gFalse;

    /* Full-size coefficients: compare by division, no long overflow */
    if (cinfo->progressive_mode && w * nc * 2 > DCT_MAX_PROGRESSIVE_COEF / h)
        return gFalse;

    return gTrue;
}

/********************************************************************
 *                      DCTStreamScaleFor
 ********************************************************************
 * SYNOPSIS:      Return the scale divisor for an image size.
 *
 * PARAMETERS:    w                     Image width.
 *                h                     Image height.
 *
 * RETURNS:       word                  1, 2, 4 or 8: both sides at most
 *                                      DCT_TARGET_DIMENSION, if possible.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
word DCTStreamScaleFor(long w, long h)
{
    word d;

    for (d = 1; d < 8; d <<= 1)
    {
        if ((w + d - 1) / d <= DCT_TARGET_DIMENSION &&
            (h + d - 1) / d <= DCT_TARGET_DIMENSION)
            break;
    }
    return d;
}

/********************************************************************
 *                      DCTStreamStart
 ********************************************************************
 * SYNOPSIS:      Set up libjpeg and start decompressing.
 *
 * PARAMETERS:    *dec                  Ptr to the decoder.
 *
 * RETURNS:       GBool                 gFalse on error.
 *
 * STRATEGY:      Install the error handler (set_error_handler) before every
 *                libjpeg call. After an error exit only the parameter dec is
 *                valid.
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static GBool DCTStreamStart(DCTDecoder *dec)
{
    word ctx;
    long rowBytes;

    dec->cinfo.err = jpeg_std_error(&dec->jerr);
    /* No dialog for libjpeg warnings while a page is drawn */
    dec->jerr.output_message = (void *)DCTOutputMessage;

    if (set_error_handler(&ctx))
    {
        /* back from error_exit: only dec (parameter) is valid here */
        DCTStreamFailed(dec);
        return gFalse;
    }
    if (!ctx)
    {
        GMemSetError();
        return gFalse;
    }
    jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, ctx);

    jpeg_create_decompress(&dec->cinfo);
    dec->created = gTrue;

    dec->cinfo.src = &dec->src;
    dec->src.init_source = (void *)DCTSrcInit;
    dec->src.fill_input_buffer = (void *)DCTSrcFill;
    dec->src.skip_input_data = (void *)DCTSrcSkip;
    dec->src.resync_to_restart = (void *)jpeg_resync_to_restart;
    dec->src.term_source = (void *)DCTSrcTerm;
    dec->src.bytes_in_buffer = 0;
    dec->src.next_input_byte = NULL;

    jpeg_read_header(&dec->cinfo, TRUE);

    if (!DCTStreamSizeOk(&dec->cinfo))
    {
        jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, 0);
        EC_WARNING(-1);
        return gFalse;
    }
    /* Default output: gray to gray, YCbCr/RGB to RGB, CMYK/YCCK to CMYK */
    dec->cinfo.dct_method = JDCT_IFAST;
    jpeg_start_decompress(&dec->cinfo);

    jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, 0);

    rowBytes = (long)dec->cinfo.output_width *
        (long)dec->cinfo.output_components;
    if (rowBytes <= 0 || rowBytes > 0xFFF0L)
    {
        EC_WARNING(-1);
        return gFalse;
    }
    dec->rowBytes = (word)rowBytes;
    dec->linesPerFill = DCT_FILL_BYTES / dec->rowBytes;
    if (dec->linesPerFill < 1)
        dec->linesPerFill = 1;
    else if (dec->linesPerFill > DCT_MAX_LINES_PER_FILL)
        dec->linesPerFill = DCT_MAX_LINES_PER_FILL;

    /* Row buffer in a movable block, locked while decoding */
    dec->bufHan = MemAlloc((word)dec->rowBytes * dec->linesPerFill,
        HF_SWAPABLE, HAF_NO_ERR);
    if (!dec->bufHan)
    {
        GMemSetError();
        return gFalse;
    }
    dec->buf = MemLock(dec->bufHan);
    dec->bufIdx = dec->bufCount = 0;
    return gTrue;
}

/********************************************************************
 *                      DCTStreamFill
 ********************************************************************
 * SYNOPSIS:      Decode the next rows into the row buffer.
 *
 * PARAMETERS:    *dec                  Ptr to the decoder.
 *
 * RETURNS:       GBool                 gFalse at the end of the image or on
 *                                      error.
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      11/22/26  JK        Initial Revision
 *******************************************************************/
static GBool DCTStreamFill(DCTDecoder *dec)
{
    word ctx, i, lines;

    if (dec->cinfo.output_scanline >= dec->cinfo.output_height)
        return gFalse;

    /* Truncated baseline JPEG: stop after the delivered rows */
    if (dec->inputEnded && !dec->cinfo.progressive_mode)
        return gFalse;

    if (set_error_handler(&ctx))
    {
        /* back from error_exit: only dec (parameter) is valid here */
        DCTStreamFailed(dec);
        dec->created = gFalse;
        return gFalse;
    }
    if (!ctx)
    {
        GMemSetError();
        return gFalse;
    }
    jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, ctx);

    for (i = 0; i < dec->linesPerFill; ++i)
        dec->rowPtr[i] = dec->buf + i * dec->rowBytes;

    lines = jpeg_read_scanlines(&dec->cinfo, dec->rowPtr, dec->linesPerFill);

    jpeg_set_error_handler_context((j_common_ptr)&dec->cinfo, 0);

    if (lines == 0)
        return gFalse;

    dec->bufIdx = 0;
    dec->bufCount = lines * dec->rowBytes;
    return gTrue;
}
