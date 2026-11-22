#
# THIS FILE HAS BEEN GENERATED AUTOMATICALLY.
#
# If you edit it, you will lose your changes, should it be regenerated.
#
GEODE           = pdfviewer
MAIN            = catalog.c xref.c pdfEngine.c gfxState.c page.c parser.c\
                  lexer.c obj.c dctStream.c dict.c array.c gfx.goc\
                  stream.goc gfxFont.goc crypt.goc
UI              = pdfvu.goc
UTIL            = gstr.c gmem.c
JPEGERR         = jpegerrManager.asm
UI_TO_RDFS      =
OBJS            = catalog.obj xref.obj pdfEngine.obj gfxState.obj page.obj\
                  parser.obj lexer.obj obj.obj dctStream.obj dict.obj\
                  array.obj gfx.obj stream.obj gfxFont.obj crypt.obj\
                  pdfvu.obj gstr.obj gmem.obj
COMMON          = pdfEngine.h gtypes.h gstr.h array.h ccitt.goh catalog.h\
                  gfx.h pdfGeode.h page.h crypt.h dict.h xref.h gfxState.h\
                  gmem.h stream.h obj.h gfxFont.h lexer.h gfxFont2.goh\
                  parser.h
MODULES         = JpegErr
CMODULES        = main ui util
SRCS            = $(MAIN) $(UI) $(UTIL) $(JPEGERR) $(COMMON)
LOBJS           =

SYSMAKEFILE     = geode.mk

#include <geos.mk>
#include <gpath.mk>

#if exists(local.mk)
#include "local.mk"
#else
#include <$(SYSMAKEFILE)>
#endif

#if exists($(DEPFILE))
#include "$(DEPFILE)"
#endif
