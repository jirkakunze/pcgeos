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
 * FILE:          page.c
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
 *      Port of Derek Noonburg's "Page" class from xpdf 0.8.
 *      Simple variable-length string type.
 ***********************************************************************/

#ifdef __GNUC__
#pragma implementation
#endif

#include "pdfGeode.h"
#include "page.h"
#include "obj.h"
#include "dict.h"
#include "array.h"
#include <Ansi/string.h>
#include <ec.h>

/* PageAttrs */

/***********************************************************************
 *      PageAttrsInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      PageAttrs *this    this
 *                  PageAttrs *attrs    attrs
 *                  Dict *dict    dictionary
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageAttrsInit(PageAttrs *this, PageAttrs *attrs, Dict *dict, XRef *xref)
{
    Obj obj1, obj2;

    if (attrs)
    {
#ifdef USE_FULL_PAGE_ATTRS
        this->x1 = attrs->x1;
        this->y1 = attrs->y1;
        this->x2 = attrs->x2;
        this->y2 = attrs->y2;
        this->cropX1 = attrs->cropX1;
        this->cropY1 = attrs->cropY1;
        this->cropX2 = attrs->cropX2;
        this->cropY2 = attrs->cropY2;
        this->rotate = attrs->rotate;
#endif
        ObjCopy(&this->resources, &attrs->resources);
    }
    else
    {
#ifdef USE_FULL_PAGE_ATTRS
        this->x1 = 0;
        this->y1 = 0;
        this->x2 = 612;
        this->y2 = 792;
        this->cropX1 = this->cropY1 = this->cropX2 = this->cropY2 = 0;
        this->rotate = 0;
#endif
        initNull(&this->resources);
    }

#ifdef USE_FULL_PAGE_ATTRS

    /* media box */
    DictLookup(dict, "MediaBox", &obj1, xref);
    if (isArray(&obj1) && ObjArrayGetLength(&obj1) == 4)
    {
        ObjArrayGet(&obj1, 0, &obj2, xref);
        if (isNum(&obj2))
        {
            this->x1 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 1, &obj2, xref);
        if (isNum(&obj2))
        {
            this->y1 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 2, &obj2, xref);
        if (isNum(&obj2))
        {
            this->x2 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 3, &obj2, xref);
        if (isNum(&obj2))
        {
            this->y2 = getNum(&obj2);
        }
        ObjFree(&obj2);
    }
    ObjFree(&obj1);

    /* crop box */
    DictLookup(dict, "CropBox", &obj1, xref);
    if (isArray(&obj1) && ObjArrayGetLength(&obj1) == 4)
    {
        ObjArrayGet(&obj1, 0, &obj2, xref);
        if (isNum(&obj2))
        {
            this->cropX1 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 1, &obj2, xref);
        if (isNum(&obj2))
        {
            this->cropY1 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 2, &obj2, xref);
        if (isNum(&obj2))
        {
            this->cropX2 = getNum(&obj2);
        }
        ObjFree(&obj2);
        ObjArrayGet(&obj1, 3, &obj2, xref);
        if (isNum(&obj2))
        {
            this->cropY2 = getNum(&obj2);
        }
        ObjFree(&obj2);

    }
    else
    {
        this->cropX1 = this->cropX2 = this->cropY1 = this->cropY2 = 0;
    }
    ObjFree(&obj1);

    /* rotate */
    DictLookup(dict, "Rotate", &obj1, xref);
    if (isInt(&obj1))
    {
        this->rotate = getInt(&obj1);
    }
    ObjFree(&obj1);
    while (this->rotate < 0)
    {
        this->rotate += 360;
    }
    while (this->rotate >= 360)
    {
        this->rotate -= 360;
    }
#endif

    /* resource dictionary */
    /*
     * XXX: made this into "nf" to prevent reading in resource, if a reference.
     */
    DictLookupNF(dict, "Resources", &obj1);
    if (isRef(&obj1) || isDict(&obj1))
    {
        ObjFree(&this->resources);
        ObjMove(&this->resources, &obj1);
    }
    ObjFree(&obj1);
}

/***********************************************************************
 *      PageAttrsFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageAttrsFree(PageAttrs *this)
{
    ObjFree(&this->resources);
}

#ifdef USE_FULL_PAGE_ATTRS

/* Accessors. */
/***********************************************************************
 *      PageAttrsGetX1
 ***********************************************************************
 * SYNOPSIS:        Get x1.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetX1(PageAttrs *this)
{
    return this->x1;
}

/***********************************************************************
 *      PageAttrsGetY1
 ***********************************************************************
 * SYNOPSIS:        Get y1.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetY1(PageAttrs *this)
{
    return this->y1;
}

/***********************************************************************
 *      PageAttrsGetX2
 ***********************************************************************
 * SYNOPSIS:        Get x2.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetX2(PageAttrs *this)
{
    return this->x2;
}

/***********************************************************************
 *      PageAttrsGetY2
 ***********************************************************************
 * SYNOPSIS:        Get y2.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetY2(PageAttrs *this)
{
    return this->y2;
}

/***********************************************************************
 *      PageAttrsIsCropped
 ***********************************************************************
 * SYNOPSIS:        Check cropped.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

GBool PageAttrsIsCropped(PageAttrs *this)
{
    return this->cropX2 > this->cropX1;
}

/***********************************************************************
 *      PageAttrsGetCropX1
 ***********************************************************************
 * SYNOPSIS:        Get crop x1.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetCropX1(PageAttrs *this)
{
    return this->cropX1;
}

/***********************************************************************
 *      PageAttrsGetCropY1
 ***********************************************************************
 * SYNOPSIS:        Get crop y1.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetCropY1(PageAttrs *this)
{
    return this->cropY1;
}

/***********************************************************************
 *      PageAttrsGetCropX2
 ***********************************************************************
 * SYNOPSIS:        Get crop x2.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetCropX2(PageAttrs *this)
{
    return this->cropX2;
}

/***********************************************************************
 *      PageAttrsGetCropY2
 ***********************************************************************
 * SYNOPSIS:        Get crop y2.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageAttrsGetCropY2(PageAttrs *this)
{
    return this->cropY2;
}

/***********************************************************************
 *      PageAttrsGetRotate
 ***********************************************************************
 * SYNOPSIS:        Get rotate.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

long PageAttrsGetRotate(PageAttrs *this)
{
    return this->rotate;
}
#endif

/***********************************************************************
 *      PageAttrsGetResourceDict
 ***********************************************************************
 * SYNOPSIS:        Get resource dictionary.
 * PARAMETERS:      PageAttrs *this    this
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

Dict *PageAttrsGetResourceDict(PageAttrs *this)
{

    return isDict(&this->resources) ?
        getDict(&this->resources) : (Dict *)NULL;
}

/***********************************************************************
 *      PageAttrsCopyResourceDict
 ***********************************************************************
 * SYNOPSIS:        Copy resource dictionary.
 * PARAMETERS:      PageAttrs *this    this
 *                  Obj *dest    destination
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageAttrsCopyResourceDict(PageAttrs *this, Obj *dest, XRef *xref)
{

    if (isRef(&this->resources))
    {
        ObjFetch(dest, &this->resources, xref);
    }
    else if (isDict(&this->resources))
    {
        ObjCopy(dest, &this->resources);
    }
    else
    {
        initNull(dest);
    }
}

/***********************************************************************
 *      PageInit
 ***********************************************************************
 * SYNOPSIS:        Initialize.
 * PARAMETERS:      Page *page    page
 *                  long num1    num1
 *                  Dict *pageDict    page dict
 *                  PageAttrs *attrs1    attrs1
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageInit(Page *page, long num1, Dict *pageDict, PageAttrs *attrs1)
{

    page->ok = gTrue;
    page->num = num1;

    memcpy(&page->attrs, attrs1, sizeof (PageAttrs));
    ObjCopy(&page->attrs.resources, &attrs1->resources);

    /* annotations */
    DictLookupNF(pageDict, "Annots", &page->annots);
    if (!(isRef(&page->annots) || isArray(&page->annots)
        || isNull(&page->annots)))
    {
        EC_WARNING(-1);

        ObjFree(&page->annots);
        goto err2;
    }

    /* contents */
    DictLookupNF(pageDict, "Contents", &page->contents);
    if (!(isRef(&page->contents) || isArray(&page->contents) ||
        isNull(&page->contents)))
    {
        EC_WARNING(-1);

        ObjFree(&page->contents);
        goto err1;
    }

    return;

    err2:
    initNull(&page->annots);
    err1:
    initNull(&page->contents);
    page->ok = gFalse;
}

/***********************************************************************
 *      PageFree
 ***********************************************************************
 * SYNOPSIS:        Release.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageFree(Page *page)
{

    PageAttrsFree(&page->attrs);
    ObjFree(&page->annots);
    ObjFree(&page->contents);
}

/***********************************************************************
 *      PageIsOk
 ***********************************************************************
 * SYNOPSIS:        Check ok.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

GBool PageIsOk(Page *page)
{
    return page->ok;
}

#ifdef USE_FULL_PAGE_ATTRS
/***********************************************************************
 *      PageGetX1
 ***********************************************************************
 * SYNOPSIS:        Get x1.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetX1(Page *page)
{
    return PageAttrsGetX1(&page->attrs);
}

/***********************************************************************
 *      PageGetY1
 ***********************************************************************
 * SYNOPSIS:        Get y1.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetY1(Page *page)
{
    return PageAttrsGetY1(&page->attrs);
}

/***********************************************************************
 *      PageIsCropped
 ***********************************************************************
 * SYNOPSIS:        Check cropped.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         success flag
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

GBool PageIsCropped(Page *page)
{
    return PageAttrsIsCropped(&page->attrs);
}

/***********************************************************************
 *      PageGetCropX1
 ***********************************************************************
 * SYNOPSIS:        Get crop x1.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetCropX1(Page *page)
{
    return PageAttrsGetCropX1(&page->attrs);
}

/***********************************************************************
 *      PageGetCropY1
 ***********************************************************************
 * SYNOPSIS:        Get crop y1.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetCropY1(Page *page)
{
    return PageAttrsGetCropY1(&page->attrs);
}

/***********************************************************************
 *      PageGetCropX2
 ***********************************************************************
 * SYNOPSIS:        Get crop x2.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetCropX2(Page *page)
{
    return PageAttrsGetCropX2(&page->attrs);
}

/***********************************************************************
 *      PageGetCropY2
 ***********************************************************************
 * SYNOPSIS:        Get crop y2.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetCropY2(Page *page)
{
    return PageAttrsGetCropY2(&page->attrs);
}

/***********************************************************************
 *      PageGetWidth
 ***********************************************************************
 * SYNOPSIS:        Get width.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetWidth(Page *page)
{
    return PageAttrsGetX2(&page->attrs) - PageAttrsGetX1(&page->attrs);
}

/***********************************************************************
 *      PageGetHeight
 ***********************************************************************
 * SYNOPSIS:        Get height.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

gdouble PageGetHeight(Page *page)
{
    return PageAttrsGetY2(&page->attrs) - PageAttrsGetY1(&page->attrs);
}

/***********************************************************************
 *      PageGetRotate
 ***********************************************************************
 * SYNOPSIS:        Get rotate.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result value
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

long PageGetRotate(Page *page)
{
    return PageAttrsGetRotate(&page->attrs);
}
#endif

/***********************************************************************
 *      PageGetResourceDict
 ***********************************************************************
 * SYNOPSIS:        Get resource dictionary.
 * PARAMETERS:      Page *page    page
 *
 * RETURNS:         result pointer
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

Dict *PageGetResourceDict(Page *page)
{
    return PageAttrsGetResourceDict(&page->attrs);
}

/***********************************************************************
 *      PageCopyResourceDict
 ***********************************************************************
 * SYNOPSIS:        Copy resource dictionary.
 * PARAMETERS:      Page *page    page
 *                  Obj *dest    destination
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageCopyResourceDict(Page *page, Obj *dest, XRef *xref)
{
    PageAttrsCopyResourceDict(&page->attrs, dest, xref);
}

/***********************************************************************
 *      PageGetAnnots
 ***********************************************************************
 * SYNOPSIS:        Get annots.
 * PARAMETERS:      Page *page    page
 *                  Obj *obj    object
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageGetAnnots(Page *page, Obj *obj, XRef *xref)
{
    ObjFetch(obj, &page->annots, xref);
}

/***********************************************************************
 *      PageGetContents
 ***********************************************************************
 * SYNOPSIS:        Get contents.
 * PARAMETERS:      Page *page    page
 *                  Obj *obj    object
 *                  XRef *xref    cross-reference table
 *
 * RETURNS:         void
 *
 * CONTEXT:
 *
 * STRATEGY:
 *
 * REVISION HISTORY:
 *  Name    Date        Description
 *  ----    ----        -----------
 *  JK      08/22/26    Style cleanup
 *
 ***********************************************************************/

void PageGetContents(Page *page, Obj *obj, XRef *xref)
{
    ObjFetch(obj, &page->contents, xref);
}

