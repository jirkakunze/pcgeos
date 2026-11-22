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
 * FILE:          function.h
 *
 * DESCRIPTION:
 *      PDF functions (types 0, 2 and 3), evaluated in 16.16 fixed
 *      point (gdouble). See main/function.c.
 ***********************************************************************/

#ifndef FUNCTION_H
#define FUNCTION_H

#include "obj.h"

#define FUNC_MAX_IN  4
#define FUNC_MAX_OUT 8

/* opaque (the glue linker wants identical struct definitions everywhere) */
typedef void PdfFunction;

/*
 * Parse a function dictionary or stream (references must be resolved).
 * RETURNS: the function, NULL if malformed or unsupported.
 */
extern PdfFunction *FuncParse(Obj *obj, XRef *xref);

extern void FuncFree(PdfFunction *f);

/* number of inputs / outputs */
extern word FuncGetInputs(PdfFunction *f);
extern word FuncGetOutputs(PdfFunction *f);

/* Evaluate: in[FuncGetInputs], out[FuncGetOutputs]. */
extern void FuncEval(PdfFunction *f, const gdouble *in, gdouble *out);

#endif /* FUNCTION_H */
