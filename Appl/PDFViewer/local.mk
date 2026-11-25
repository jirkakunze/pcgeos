#
#	Local makefile for: PDFViewer
#

GEODE = pdfvu
.PATH.ui	: UI Document $(INSTALL_DIR)/UI $(INSTALL_DIR)/Document
.PATH.uih	: UI Document $(INSTALL_DIR)/UI $(INSTALL_DIR)/Document
UICFLAGS	+= -IUI -I$(INSTALL_DIR)/UI 

ASMFLAGS	+= -Wall

# The view code depends on this to orient the view properly.
#
CCOMFLAGS   += -DUSE_FULL_PAGE_ATTRS

# Enable the ability to copy a page to the clipboard.
#
GOCFLAGS    += -DCOPY_PAGE
CCOMFLAGS   += -DCOPY_PAGE


#include <$(SYSMAKEFILE)>
