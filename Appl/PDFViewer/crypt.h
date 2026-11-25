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
 * FILE:          crypt.h
 *
 * AUTHOR:        Jirka Kunze: 18.08.2026
 *
 * REVISION HISTORY:
 *      Date      Name      Description
 *      ----      ----      -----------
 *      18.08.26  JK        Relicensed under Apache 2.0, cleanup.
 *
 * DESCRIPTION:
 *
 ***********************************************************************/

#ifndef CRYPT_H
#define CRYPT_H

#include "dict.h"

/* Process rc4 key schedule. */
void RC4KeySchedule(Guchar *key, short keyLen, Guchar sBox[256]);
/* Handle rc4 next byte. */
Guchar RC4NextByte(Guchar sBox[256], Guchar *rc4i, Guchar *rc4j);
/* Handle rc4 apply. */
void RC4Apply(Guchar *key, short keyLen, Guchar *data, long len);

/* Result of XRefSetupEncryption. */
typedef enum
{
    CRYPT_OK,           /* password accepted, decryption set up */
    CRYPT_BAD_PASSWORD, /* supported encryption, wrong password */
    CRYPT_UNSUPPORTED   /* other security handler, AES, damaged */
} XRefCryptStatus;

/* Set up decryption with a user or owner password ("" = none). */
XRefCryptStatus XRefSetupEncryption(XRef *xref, const char *password);

/* Process cross-reference table derive object key. */
void XRefDeriveObjectKey(XRef *xref, long num, long gen,
    Guchar outKey[16], short *outKeyLen);

#endif  /* CRYPT_H */

