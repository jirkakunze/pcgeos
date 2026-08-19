/***********************************************************************
 *
 * crypt.h
 *
 * Copyright 2026 Jirka Kunze/FreeGEOS Project
 *
 * This file is derived from the original Xpdf source code and has been
 * modified for use in the PC/GEOS PDF Viewer.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***********************************************************************/

#ifndef _CRYPT_H
#define _CRYPT_H


#include "dict.h"

/***********************************************************************
 *		MD5Compute
 ***********************************************************************
 * SYNOPSIS:	    One-shot MD5 (RFC 1321) over a single, fully-in-
 *		    memory buffer. No incremental/streaming API --
 *		    every input this module ever hashes is small and
 *		    of a known, fixed size up front (password padding,
 *		    /O, /P, /ID[0], and the 50-round key-strengthening
 *		    loop's own 16-byte output), so there's no need for
 *		    the extra complexity of a chunked interface.
 * PARAMETERS:	    Guchar *data, word len (input), Guchar digest[16] (out)
 ***********************************************************************/
void MD5Compute(Guchar *data, word len, Guchar digest[16]);

/***********************************************************************
 *		RC4KeySchedule / RC4NextByte / RC4Apply
 ***********************************************************************
 * SYNOPSIS:	    Shared RC4 primitives -- the same key-scheduling and
 *		    per-byte keystream generation used both here (one-
 *		    shot, for Algorithm 6's password verification) and
 *		    by the streaming RC4Stream in main/stream.goc (fed
 *		    one byte at a time via StreamGetChar). Kept as
 *		    plain functions operating on caller-owned state
 *		    (sBox[256] + two index bytes) rather than a shared
 *		    struct, so both call sites can use whatever storage
 *		    shape suits them (RC4Data's fields directly for the
 *		    stream, a local array here).
 ***********************************************************************/
void RC4KeySchedule(Guchar *key, short keyLen, Guchar sBox[256]);
Guchar RC4NextByte(Guchar sBox[256], Guchar *rc4i, Guchar *rc4j);
void RC4Apply(Guchar *key, short keyLen, Guchar *data, long len);

/***********************************************************************
 *		XRefSetupEncryption
 ***********************************************************************
 * SYNOPSIS:	    Called from XRefInit once /Encrypt is seen: reads
 *		    the Standard Security Handler's parameters (/V, /R,
 *		    /O, /U, /P, /Length) and the trailer's /ID[0],
 *		    derives the file encryption key assuming an empty
 *		    user password (Algorithm 2), and verifies that
 *		    assumption against /U (Algorithm 6) before trusting
 *		    it. On success, fills in xref->encrypted/fileKey/
 *		    fileKeyLen/encryptRevision and returns gTrue.
 * RETURNS:	    gFalse if /Encrypt is present but isn't something
 *		    this module handles (unsupported /V, non-Standard
 *		    /Filter, AES-only /V 4-5, or -- most commonly -- a
 *		    real non-empty user password actually required).
 *		    The caller's existing PDF_ERR_ENCRYPTED handling
 *		    covers all of those identically; this module
 *		    doesn't try to distinguish them further.
 ***********************************************************************/
GBool XRefSetupEncryption(XRef *xref);

/***********************************************************************
 *		XRefDeriveObjectKey
 ***********************************************************************
 * SYNOPSIS:	    PDF Algorithm 1: file key + object number/generation
 *		    -> this object's own RC4 key. Called once per stream
 *		    object, from ParserMakeStream, right before wrapping
 *		    its raw bytes in an RC4-decrypting stream.
 * PARAMETERS:	    XRef *xref (must have xref->encrypted set), long num,
 *		    long gen, Guchar outKey[16] (out), short *outKeyLen (out)
 ***********************************************************************/
void XRefDeriveObjectKey(XRef *xref, long num, long gen,
			  Guchar outKey[16], short *outKeyLen);

#endif /* _CRYPT_H */
