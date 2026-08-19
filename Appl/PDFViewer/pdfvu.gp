########################################################################
#
#                       Copyright FreeGEOS-Project
#               Portions Copyright (c) GlobalPC 1999
# 
#       Licensed under the Apache License, Version 2.0 (the "License");
#       you may not use this file except in compliance with the License.
#       You may obtain a copy of the License at
# 
#           http://www.apache.org/licenses/LICENSE-2.0
# 
#  PROJECT:       FreeGEOS
#  MODULE:        PDF Viewer
#  FILE:          pdfvu.gp
# 
#  AUTHOR:        Jirka Kunze: 18.08.2026
# 
#  REVISION HISTORY:
#       Date      Name      Description
#       ----      ----      -----------
#       3/31/99   mevissen  Initial version (GlobalPC).
#       18.08.26  JK        Relicensed under Apache 2.0, cleanup.
# 
#  DESCRIPTION:
#       PDF code ported from xpdf (source from //www.foolabs.com/xpdf)
########################################################################

name pdfvu.app

longname "PDF Viewer"

type	appl, process, single

class	PDFProcessClass

appobj	PDFApp

tokenchars "PDFV"
tokenid 0


heapspace 70k
stack 3000

library	geos
library ui
library ansic
library spool


resource DisplayUI   object shared read-only
resource AppResource ui-object
resource Interface   ui-object
resource DocumentUI  object

resource ENCODINGS lmem read-only shared
resource OPTABLE   lmem read-only shared
resource FAXCODES  lmem read-only shared

resource AppSCIconResource lmem read-only shared
resource AppTCIconResource lmem read-only shared


export PDFDocumentClass
export PDFDocumentControlClass
export PDFImageInteractionClass
export PDFPageControlClass