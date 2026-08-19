gfx.obj \
gfx.eobj: 
gfx.obj \
gfx.eobj: Ansi/string.h geos.h Ansi/stdio.h pdfEngine.h graphics.h \
                fontID.h font.h color.h heap.h char.h localize.h \
                Ansi/ctype.h timedate.h file.h sllang.h ec.h lmem.h \
                hugearr.h resource.h gfx.h gtypes.h pdfGeode.h parser.h \
                lexer.h obj.h dict.h gfxFont.h gstring.h object.h geode.h \
                gfxState.h array.h stream.h gmem.h gstr.h
stream.obj \
stream.eobj: ccitt.goh
stream.obj \
stream.eobj: Ansi/string.h geos.h Ansi/stdio.h heap.h lmem.h obj.h \
                pdfGeode.h file.h gtypes.h array.h stream.h crypt.h \
                dict.h gmem.h Ansi/ctype.h ec.h hugearr.h
gfxFont.obj \
gfxFont.eobj: gfxFont2.goh
gfxFont.obj \
gfxFont.eobj: Ansi/stdlib.h geos.h Ansi/string.h char.h Ansi/ctype.h \
                ec.h obj.h pdfGeode.h file.h gtypes.h array.h dict.h \
                stream.h lexer.h parser.h xref.h gfxFont.h gstring.h \
                graphics.h fontID.h font.h color.h object.h geode.h \
                lmem.h gstr.h gmem.h Ansi/stdio.h heap.h
crypt.obj \
crypt.eobj: 
crypt.obj \
crypt.eobj: Ansi/string.h geos.h obj.h pdfGeode.h file.h gtypes.h \
                array.h dict.h gstr.h gmem.h Ansi/stdio.h crypt.h
pdfvu.obj \
pdfvu.eobj: stdapp.goh object.goh ui.goh Objects/metaC.goh \
                Objects/inputC.goh Objects/clipbrd.goh \
                Objects/uiInputC.goh iacp.goh Objects/winC.goh \
                Objects/gProcC.goh alb.goh Objects/processC.goh \
                Objects/visC.goh Objects/vCompC.goh Objects/vCntC.goh \
                Objects/gAppC.goh Objects/genC.goh Objects/gInterC.goh \
                Objects/gPrimC.goh Objects/gDispC.goh Objects/gTrigC.goh \
                Objects/gViewC.goh Objects/gTextC.goh Objects/vTextC.goh \
                Objects/gCtrlC.goh gcnlist.goh spool.goh \
                Objects/gFSelC.goh Objects/gGlyphC.goh \
                Objects/gDocCtrl.goh Objects/gDocGrpC.goh \
                Objects/gDocC.goh Objects/gContC.goh Objects/gDCtrlC.goh \
                Objects/gEditCC.goh Objects/gBoolGC.goh \
                Objects/gItemGC.goh Objects/gDListC.goh \
                Objects/gItemC.goh Objects/gBoolC.goh \
                Objects/gGadgetC.goh Objects/gToolCC.goh \
                Objects/gValueC.goh Objects/gToolGC.goh \
                Objects/helpCC.goh Objects/gViewCC.goh \
                Objects/gPageCC.goh art/PDFVuSC.goh art/PDFVuTC.goh
pdfvu.obj \
pdfvu.eobj: geos.h heap.h geode.h resource.h ec.h object.h lmem.h \
                graphics.h fontID.h font.h color.h gstring.h timer.h vm.h \
                dbase.h localize.h Ansi/ctype.h timedate.h file.h \
                sllang.h system.h geoworks.h chunkarr.h Objects/helpCC.h \
                disk.h drive.h input.h char.h hwr.h win.h uDialog.h \
                Objects/gInterC.h Objects/Text/tCommon.h stylesh.h \
                driver.h thread.h print.h Internal/spoolInt.h serialDr.h \
                parallDr.h hugearr.h fileEnum.h pdfEngine.h initfile.h \
                Ansi/string.h
catalog.obj \
catalog.eobj: pdfGeode.h geos.h file.h gtypes.h catalog.h obj.h page.h \
                xref.h dict.h array.h gmem.h Ansi/stdio.h heap.h ec.h
xref.obj \
xref.eobj: Ansi/string.h geos.h Ansi/stdlib.h Ansi/stdio.h obj.h \
                pdfGeode.h file.h gtypes.h array.h stream.h parser.h \
                dict.h xref.h lexer.h gmem.h crypt.h vm.h lmem.h \
                hugearr.h Ansi/ctype.h ec.h
pdfEngine.obj \
pdfEngine.eobj: pdfGeode.h geos.h file.h gtypes.h pdfEngine.h graphics.h \
                fontID.h font.h color.h heap.h char.h catalog.h xref.h \
                obj.h page.h gfx.h gmem.h Ansi/stdio.h geode.h lmem.h \
                hugearr.h vm.h ec.h gstring.h
gfxState.obj \
gfxState.eobj: geos.h math.h graphics.h fontID.h font.h color.h \
                Ansi/string.h gmem.h Ansi/stdio.h gtypes.h obj.h \
                pdfGeode.h file.h array.h dict.h stream.h gfxState.h \
                gstr.h ec.h
page.obj \
page.eobj: pdfGeode.h geos.h file.h gtypes.h page.h obj.h dict.h \
                array.h Ansi/string.h ec.h
parser.obj \
parser.eobj: obj.h pdfGeode.h geos.h file.h gtypes.h array.h dict.h \
                parser.h lexer.h stream.h gmem.h Ansi/stdio.h crypt.h \
                ec.h
lexer.obj \
lexer.eobj: pdfGeode.h geos.h file.h gtypes.h char.h Ansi/ctype.h ec.h \
                Ansi/string.h Ansi/stdio.h obj.h array.h stream.h lexer.h \
                gmem.h gstr.h
obj.obj \
obj.eobj: Ansi/string.h geos.h obj.h pdfGeode.h file.h gtypes.h \
                array.h dict.h xref.h stream.h gmem.h Ansi/stdio.h gstr.h
dict.obj \
dict.eobj: Ansi/string.h geos.h obj.h pdfGeode.h file.h gtypes.h \
                xref.h dict.h gmem.h Ansi/stdio.h
array.obj \
array.eobj: pdfGeode.h geos.h file.h gtypes.h array.h gmem.h \
                Ansi/stdio.h obj.h
gstr.obj \
gstr.eobj: Ansi/string.h geos.h pdfGeode.h file.h gtypes.h gstr.h \
                gmem.h Ansi/stdio.h
gmem.obj \
gmem.eobj: Ansi/stdlib.h geos.h Ansi/string.h ec.h gmem.h \
                Ansi/stdio.h gtypes.h

pdfvuEC.geo pdfvu.geo : geos.ldf ui.ldf ansic.ldf spool.ldf 