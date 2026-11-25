PDF Viewer Testdateien
======================

Elf gezielte Testdateien, jede auf einen konkreten Problembereich aus
der Projekt-README bzw. aus der Engine-Arbeit (Phasen 0-4) zugeschnitten.
Erzeugt mit reportlab/pypdf bzw. (bei 08, 10, 11) von Hand als rohes PDF,
um volle Kontrolle über Struktur/Colorspace/xref zu haben.

01_basic_text.pdf
    Baseline-Rauchtest: ein Absatz Fließtext, Standardfont. Wenn das
    nicht sauber rendert, ist alles andere zweitrangig.

02_font_mapping.pdf
    Alle 14 Standard-PDF-Fonts (Helvetica/Times/Courier je 4 Stile,
    Symbol, ZapfDingbats) auf einer Seite. Direkter Regressionstest
    für das grobe Namens-Substring-Mapping (Arial/Helvetica,
    CourierNew/Courier, sonst Default) -- erwartungsgemäß wird der
    Viewer hier aktuell alles außer den ersten beiden Substitutionen
    auf denselben Default-Font abbilden.

03_bezier_curves.pdf
    S-Kurven, gefüllte Blob-Form, offene Wellenlinie, konzentrische
    Bögen. Testet direkt den in der README dokumentierten
    Bézier-Rendering-Bug (bisher nur mit Kernel-Workaround umschifft,
    nicht behoben).

04_rotation.pdf
    Vier Seiten mit /Rotate 0/90/180/270. Großer "TOP"-Marker plus
    asymmetrischer roter Eckmarker pro Seite, damit falsche/fehlende
    Rotation auf einen Blick auffällt. Testet
    PdfComputePageGeometry's Rotationslogik (main/pdfEngine.goc).

05_cropbox.pdf
    MediaBox 8.5x11", CropBox mit 1" Rand nach innen (roter Rahmen an
    der MediaBox-Kante, blauer an der CropBox-Kante). Ein korrekter
    Viewer zeigt/clippt nur bis zum blauen Rahmen. Testet den
    PageIsCropped-Zweig in PdfComputePageGeometry -- das ist genau die
    Stelle, die vor der Engine-Umstellung hart auf 8.5x11 lag.

06_images.pdf
    Seite 1: JPEG (DCTDecode-Filter) und PNG mit Alphakanal (Maske).
    Seite 2: 1-Bit-Schwarzweißbild (ImageMask-Pfad) -- die README
    nennt invertierte Image-Masks als bekannten Bug auf manchen
    Dateien.

07_multipage_50.pdf
    50 Seiten mit fortlaufender, gut sichtbarer Seitenzahl. Testet die
    30-Seiten-Cache-Verdrängung (PDF_MAX_CACHED_PAGES in
    main/pdfEngine.goc): von Seite 1 bis 50 und zurück scrollen, Seite
    1 muss sauber neu dekodiert werden, nicht leer oder falsch
    bleiben.

08_colorspace_indexed.pdf
    4x4-Pixel-Bild mit Indexed-Colorspace (3-Farben-Palette,
    DeviceRGB-Basis), vier Farbquadranten (rot/grün/blau/gelb). Die
    README nennt fehlende Named/Separation-Colorspaces als bekannte
    Lücke -- Indexed ist die nächstliegende, häufig vorkommende
    Variante davon.

09_encrypted.pdf
    Mit leerem Nutzer-Passwort RC4-128-verschlüsselt. Muss ohne
    Passwortabfrage öffnen und normal dargestellt werden (die meisten
    verschlüsselten PDFs schränken nur Drucken/Kopieren ein).

10_malformed_xref.pdf
    Xref-Tabelle mit durchgängig um 5 Byte verschobenen Offsets --
    jeder Objekt-Lookup landet mitten im Objekt statt an dessen
    Anfang. Testet PDF_ERR_BAD_XREF und den Cleanup-Pfad von
    PdfClose() nach fehlgeschlagenem PdfOpen() (siehe Phase 1). Zum
    Vergleich: pypdf selbst repariert das beim Einlesen automatisch
    ("Ignoring wrong pointing object..." beim Parsen mit pypdf) --
    unser Engine-Code ist laut README deutlich weniger tolerant, das
    ist hier gewünscht zu prüfen.

11_inline_image.pdf
    4x4-RGB-Bild als Inline-Image (BI/ID/EI im Content-Stream statt
    XObject), ASCIIHexDecode-gefiltert. Testet den inlineImage-Zweig
    in main/gfx.goc's Bild-Decoder (Progress-Dialog wird für Inline-
    Images bewusst übersprungen, siehe Phase 4).

17_form_xobject.pdf
    Form-XObjects (Do auf /Subtype /Form). Oben: rotes Quadrat (Fm1),
    darin über eine verschachtelte Form (Fm2) ein grünes Quadrat. Unten:
    blaues Quadrat aus einer Form (Fm3), die sich selbst aufruft --
    muss nach PDF_MAX_FORM_DEPTH abbrechen, darf nicht hängen oder
    abstürzen. Der Text "End of page reached." muss erscheinen.

18_broken_page.pdf
    Zwei Seiten, die zweite verweist auf ein nicht existierendes Objekt.
    Seite 1 muss normal erscheinen; Seite 2 muss genau einmal die
    Meldung "could not display this page" zeigen (nicht bei jedem
    Neuzeichnen erneut) und darf nicht abstürzen. Drucken von Seite 1-2
    muss mit derselben Meldung abbrechen.

19_predictors.pdf
    PDF 1.5 ohne klassische xref-Tabelle: Xref-Stream mit /Predictor 12,
    Catalog/Pages/Page/Font liegen in einem Objekt-Stream -- öffnet nur,
    wenn der Predictor auch für Xref-Streams greift. Vier 64x64-Bilder:
    A PNG-Predictor mit allen fünf Filtertypen (zeilenweise wechselnd),
    B TIFF-Predictor RGB 8 Bit, C 16-Bit-RGB mit PNG-Predictor (Paeth),
    D 4-Bit-Graustufen mit TIFF-Predictor. A, B und C müssen gleich
    aussehen (Farbverlauf schwarz/rot/grün/weiß), D zeigt 16 Graustufen.

20_jpeg_variants.pdf
    Sieben DCTDecode-Bilder (JPEG über die IJG-Library): A Baseline
    YCbCr 4:2:0, B progressiv, C Graustufen, D CMYK (Adobe, invertiert,
    /Decode [1 0 1 0 1 0 1 0]), E 4:4:4 mit Restart-Markern. A, B, D und
    E müssen gleich aussehen (Farbverlauf, weißer Rahmen, schwarze
    Diagonale), C in Grau. F ist nach 40 % der Scan-Daten abgeschnitten:
    nur der obere Teil erscheint, darunter bleibt die Seite frei (wie in
    poppler). G hat einen kaputten Header: es wird nichts gezeichnet,
    kein schwarzes Quadrat. Beide dürfen weder abstürzen noch einen
    Dialog der JPEG-Library zeigen; der Rest der Seite muss erscheinen.

21_password_rc4_128.pdf, 22_password_rc4_40.pdf
    01_basic_text.pdf mit qpdf verschlüsselt, RC4 128 Bit (R3) bzw.
    40 Bit (R2). Benutzerpasswort "geos", Besitzerpasswort "owner".
    Beim Öffnen erscheint der Passwortdialog (Eingabe verdeckt); beide
    Passwörter öffnen die Datei. Ein falsches Passwort zeigt den Dialog
    erneut mit dem Hinweis "The password is not correct"; Cancel
    bricht ohne weitere Meldung ab.

23_password_aes.pdf
    Wie 21, aber AES-128 (R4). Wird nicht unterstützt: kein
    Passwortdialog, sondern die Meldung, dass die Verschlüsselungsart
    nicht unterstützt wird.

24_cid_fonts.pdf
    Type0-Schriften mit /Encoding /Identity-H (wie von Word, LibreOffice
    und Browsern erzeugt), dargestellt mit GEOS-Ersatzschriften.
    Seite 1: CIDFontType2 (TrueType) mit /ToUnicode (bfchar, bfrange,
    bfrange mit Array) in Sans, Serif (URW Roman), Mono (URW Mono) und
    Bold: Umlaute, ß, €, Gedankenstrich, deutsche Anführungszeichen,
    Ligaturen "fi"/"fl" (Profi-Fluss). Breiten aus /W (Array- und
    Bereichsform). Darunter TJ mit Kerning (Lücke nach "Gr"), Tc 2
    (gesperrt) und Tw 20, das bei 2-Byte-Codes keine Wirkung haben darf.
    Seite 2: Wahl der Ersatzschrift - allgemeine Namensregeln
    ("VintageSerif" -> Serif, "OfficeCode" -> Mono, beide mit /Flags 32),
    unbekannte Namen über /Flags (34 Serif, 33 Mono), Namenstabelle
    (TimesNewRomanPSMT mit /Flags 32 -> Serif).
    Seite 3: ohne /ToUnicode (auch mit /CIDToGIDMap-Stream): PDF Viewer
    zeichnet bewusst keinen Text (die Codes sind Glyphnummern ohne
    eigene Bedeutung), nur die Beschriftungen; kein Absturz.
    Seite 4: CIDFontType0 mit eingebettetem CFF: oben ohne /ToUnicode
    (kein Text), unten mit /ToUnicode (Serif).
    Seite 5: nicht unterstützte CMaps (Identity-V, UniJIS-UCS2-H): der
    Text wird übersprungen, die Beschriftungen und die untere Zeile
    erscheinen.
    Die TrueType-Schriften sind Liberation-Schriften, nur mit den
    benutzten Glyphen gefüllt (~250 KB).

25_shadings.pdf
    Verläufe (Shadings) aller Typen, zum Vergleich mit poppler/Acrobat.
    Seit I1 zeichnet der RM-Viewer KEINE Verläufe mehr (kommen im PM
    zurück): 'sh' füllt die aktuelle Clip-Fläche und eine Pattern-
    Füllung die Fläche einfarbig mit einer Ersatzfarbe (Mitte von C0/C1
    einer Typ-2-Funktion, auch Teilfunktion einer Typ-3-Funktion, sonst
    /Background, sonst keine Füllung bzw. bisherige Farbe). Die Seite
    darf weder abstürzen noch langsam sein. Bis I1 (13b′) galt: nur
    Typen 1-3 mit Funktionstyp 0/2/3; Netze/Patches (Typ 4-7) und
    PostScript-Funktionen (Typ 4: A5, A8, Seite 3 links) leer, als
    Pattern-Füllung einfarbig. Axial/radial mit
    höchstens 64 Bändern/Ringen, Typ 1 mit höchstens 16 x 16 Zellen:
    Seite 1: axial (Typ 2) mit allen Funktionstypen: A1 rot->blau im
    Einheitsquadrat (per cm skaliert), A2 Achse 0,3..0,7 mit Extend,
    A3 Stitching (gelb-grün-dunkelblau), A4 Sampled (Regenbogen), A5
    PostScript-Funktion, A6 DeviceCMYK diagonal, A7 in Seitenkoordinaten
    grau mit /BBox (rotes Rechteck) und Clip, A8 PostScript ifelse
    (harte Kante bei 25 %).
    Seite 2: radial (Typ 3): R1 weiß -> blau, R2 versetzter Brennpunkt,
    R3 Ring mit Extend (innen grün, außen gelb), R4 Kegel rot -> blau,
    R5 schrumpfender Radius mit Extend (außen dunkelblau, innen weiß),
    R6 Stitching, darunter R1 als breite Ellipse.
    Seite 3: funktionsbasiert (Typ 1): PostScript f(x,y), Sampled 4x4,
    Kreis mit /Domain und /Matrix (rechts rot, links blau, Rand weiß).
    Seite 4: Dreiecksnetze (Typ 4 mit Flags 0/1/2 und mit Funktion,
    Typ 5 Gitter 4 x 3).
    Seite 5: Patches (Typ 6: zwei Coons-Patches mit gemeinsamer Kante,
    Typ 7: Tensor-Patch mit verschobenen Innenpunkten, P6F Coons-Patch
    mit Funktion).
    Seiten 4 und 5 bleiben in GEOS leer (nur Beschriftungen).
    Seite 6: Shading-Pattern als Füllung (Rechteck, Kreispfad, mit
    /Background, in einem Form-XObject); Text und Linie mit Pattern
    erscheinen in einer mittleren Farbe des Verlaufs (keine echte
    Verlaufsfüllung für Text/Linien), danach wieder schwarzer Text.

27_transparency.pdf
    Weiche Masken, Transparenzgruppen und konstante Deckkraft, Referenz
    pdftoppm. ca/CA wird in GEOS als Rastermaske gezeichnet (GEOS-
    Zeichenmasken SDM_0..SDM_100): der Hintergrund scheint als
    Punktraster durch, z. B. A1/A2 Gelb unter dem Blau, G1 Rot unter
    dem Blau; A1 und A2 gleich, egal ob gs vor oder nach rg steht. A3
    Grau, Rosa und eine gerasterte rote Linie. ExtGState-/SMask wird
    ignoriert (M1 rote Fläche ohne Verblassen, M2 schwarzer Schatten
    als volles Rechteck unter der Karte), M3 /SMask /None grün. I1 Bild
    mit Alpha-/SMask wird echt maskiert. A4 und der Text am Ende müssen
    wieder voll deckend sein.

28_hybrid_xref.pdf
    Hybrid-Datei (PDF 1.5): klassische xref-Tabelle, deren Trailer mit
    /XRefStm auf einen Querverweis-Stream zeigt; die Ressourcen von
    Seite 1 und das Seitenobjekt von Seite 2 stehen in einem Objekt-
    Stream, der nur darüber erreichbar ist. Dazu ein inkrementelles
    Update (klassische Tabelle mit /Prev), das Seite 1 ersetzt und
    Seite 3 anhängt. Erwartet: 3 Seiten, Überschriften in Times fett,
    Fließtext in Helvetica, auf Seite 1 kein "(ALT)". Generator mk28.py
    (nur Scratchpad).

28b_objstm_rc4.pdf
    Inhalt von 28, mit qpdf in Objekt-Streams und Querverweis-Stream
    umgeschrieben und RC4-128-verschlüsselt (leeres Benutzerpasswort,
    Besitzerpasswort "geheim"). Öffnet ohne Passwortabfrage, Ergebnis
    wie 28.

28c_no_print_copy.pdf
    Inhalt von 28, RC4-128 mit leerem Benutzerpasswort und /P ohne
    Drucken und Kopieren (P = -2072). Öffnet ohne Passwortabfrage;
    Drucken (Menü, Werkzeugleiste) und Bearbeiten > Kopieren sind
    deaktiviert.

28d_no_print_copy_pw.pdf
    Wie 28c, aber mit Benutzerpasswort "user" (Besitzerpasswort
    "geheim"). Mit "user": Drucken und Kopieren deaktiviert. Mit
    "geheim": keine Einschränkung.

28e_doc_info.pdf
    Dokumentinformationen (/Info), Datei > Document Info... zeigt:
      Title:        Größe über 5 € – „Test“ ?   (UTF-16BE; ☺ gibt es
                    im GEOS-Zeichensatz nicht -> ?)
      Author:       Jürgen Müller • Straße – € 100   (PDFDocEncoding
                    0x80/0x85/0xA0 und Latin-1)
      Subject:      Sprachkennung übersprungen   (UTF-16-Sprach-
                    Escape wird übersprungen; pdfinfo zeigt hier ein
                    falsches Zeichen davor)
      Keywords:     PDF, Info, Umlaute üöä, Zeilen umbruch   (Oktal-
                    Escapes, Zeilenumbruch als Leerzeichen)
      Application:  mk28e.py
      PDF Producer: Handgeschrieben, kein qpdf
      Created:      23.11.2026 21:45 (Format nach Ländereinstellung)
      Modified:     01.01.2026 00:00 (nur Jahr angegeben)
      Pages:        1
    Generator mk28e.py (nur Scratchpad).

28f_doc_info_rc4.pdf
    28e RC4-128-verschlüsselt (leeres Benutzerpasswort): gleiche Anzeige,
    die Info-Strings werden entschlüsselt.

29_graphics_state.pdf
    Grafikzustand, Bilder und Farben (Phase C), Referenz pdftoppm.
    Seite 1 (C1): ExtGState-Linienparameter. Links mit Operatoren
    (w, J, j, M, d, Tf), rechts dasselbe über gs mit /LW, /LC, /LJ,
    /ML, /D, /Font - beide Spalten müssen gleich aussehen (Strich-
    muster [2 4] 3 bei kleiner Auflösung leicht abweichend); Ecken
    folgen der Eckform, "ML 1" ist abgeschrägt, kein Stummel am
    Pfadanfang.
    Seite 2 (C1b): Pfade zeichnen - geschlossenes Dreieck und Rechteck
    (re) mit Gehrung/rund/abgeschrägt an allen Ecken, zwei offene
    Teilpfade in einem Pfad ohne Verbindungslinie, Kurve + Gerade (dort
    keine Eckform), gefülltes Dreieck mit gestrichelter Kontur (B).
    Seite 3 (C2, K2/K3): Farbräume. Im Viewer zeigen die Zeilen
    DeviceRGB, ICCBased N 3 und CalRGB dasselbe reine Rot/Grün/Blau/
    Grau 50 %, CalGray/ICCBased N 1 Grau 50 %, ICCBased N 4 wie
    DeviceCMYK, Separation Orange in Tint 1/0.5/0 (0 = weiß), DeviceN
    mit zwei Farben als Grau-Näherung, das 2x2-Bild rot/grün/blau/weiß.
    /DefaultRGB (CalRGB) in den Ressourcen ändert im Viewer nichts
    (CalRGB/ICCBased werden wie DeviceRGB gezeichnet); pdftoppm wendet
    es an und zeigt Zeilen 1, 3 und das Bild leicht rosa getönt, die
    ICC-Platzhalterprofile meldet es als fehlerhaft (nutzt /Alternate).
    Seite 4 (C3): Bildtransparenz. Alle Bilder liegen auf gelb/türkisen
    Streifen, durchsichtige Stellen zeigen die Streifen (nicht weiß):
    Color-Key RGB (roter Kreis), Color-Key Indexed 2 Bit (Ringe),
    /Mask-Stream (blauer Kreis), /SMask-Rampe (links durchsichtig,
    rechts deckend; im Viewer gerastert), Color-Key 16 Bit (blauer
    Kreis), Color-Key-Bild mit ca 0.5 (Kreis gerastert), ImageMask als
    Schachbrett-Kreis in Magenta, dasselbe mit ca 0.5. Referenz hier
    mutool: pdftoppm wertet den 16-Bit-Color-Key nicht aus (grünes
    Quadrat).
    Generator mk29.py (nur Scratchpad).

30_text_render.pdf
    Textdarstellungsmodi Tr 0-7 (D1), Referenz pdftoppm. "GEOS" in
    Helvetica-Bold 40 pt, Füllfarbe rot, Linienfarbe blau: Tr 0 rot
    gefüllt, Tr 1 blaue Kontur, Tr 2 beides, Tr 3 unsichtbar (nichts
    zu sehen, der Text bleibt für die Suche). Tr 4-7 zusätzlich als
    Clip: die anschließend gezeichneten grün/orangen Streifen
    erscheinen nur innerhalb der Buchstaben (Tr 7 ohne eigene Farbe).
    Zwei Tj in einem BT mit Tr 7: Streifen in "AB" und "CD" (Vereinigung
    bis ET). Danach (nach Q) Text und blauer Balken ohne Clip.
    Generator mk30.py (nur Scratchpad).

31_fonts.pdf
    Schriften (D4), Referenz pdftoppm. Ersatzschriften: Form und
    Laufweite weichen ab, alle Zeichen müssen lesbar und richtig sein.
    Seite 1: echtes pdfTeX-PDF (plain TeX, Computer Modern Type1-
    Subsets mit /Flags 4, /CharSet, ohne /Encoding und /ToUnicode -
    das Encoding steht nur in der Schriftdatei). Normal, Ligaturen
    fi/fl/ff/ffi/ffl, Satzzeichen, Umlaute (OT1: Akzent als eigenes
    Zeichen neben dem Buchstaben, nicht darüber), fett, kursiv,
    Schreibmaschine, serifenlos, Mathe (Hoch-/Tiefstellung, griechische
    Buchstaben, kleiner-gleich, Summe; Zeichen ohne GEOS-Entsprechung
    fehlen). Vor D4 war diese Seite weitgehend leer.
    Seite 2: Base-14 ohne /Widths (Helvetica, Times, Times-BoldItalic,
    Courier, Symbol), WinAnsi- und MacRoman-Sonderzeichen,
    /Differences mit Standardnamen (Äöß), /Differences mit
    Fantasienamen /g1../g5 plus /ToUnicode ("Hallo" - pdftoppm zeichnet
    nach Glyphnamen und zeigt hier nichts), /FirstChar 30 /LastChar 400
    (Bereich wird begrenzt; Text gesperrt, kein Absturz). /ProcSet in
    den Ressourcen wird ignoriert.
    Generatoren mk31a.tex (pdftex) und mk31b.py, zusammengefügt mit
    qpdf --empty --pages (nur Scratchpad).

32_links.pdf
    Interne Links (F4). Seite 1: acht Zeilen, 1-7 mit blauem Rahmen
    (Link-Rechteck). Klick in einen Rahmen springt zum Ziel; das Ziel
    steht dann am oberen Fensterrand (rote Linie mit der Nummer):
    1 explizit [Seite 3 /XYZ 0 400], 2 [Seite 2 /Fit] (Seitenanfang),
    3 benannt /Kap2 über /Dests, 4 String (kap3) über den Name-Tree
    (/Names /Dests, Wert als << /D ... >>), 5 Aktion /GoTo mit /FitH,
    6 /GoTo (zz) im zweiten Kind des Name-Trees -> Seite 4 (/Rotate 90),
    7 /URI (extern) und 8 (kein Rahmen): nichts passiert. Seite 4 hat
    einen Link zurück zu Seite 1. Zoom-Angaben der Ziele werden
    ignoriert. Referenz: pdfinfo -dests (mutool löst Name-Tree-Ziele
    hier nicht auf). Generator mk32.py (nur Scratchpad).

33_print.pdf
    Drucken (G2). Sieben Seiten mit blauem Rahmen am Rand der (Crop-)
    Box, roten Diagonalen und grünem Pfeil "OBEN". Gedruckt muss jeweils
    der ganze Rahmen sichtbar sein, im richtigen Seitenverhältnis und
    zentriert im Druckbereich: 1 A4 hoch, 2 A4 quer (auf Hochformat-
    Papier um 90 Grad gedreht, größer als ungedreht), 3 A4 mit /Rotate
    90 (wirkt quer, wird wieder hochkant gedruckt), 4 A6 (vergrößert),
    5 A2 (verkleinert), 6 Streifen 842x200 (gedreht), 7 CropBox: nur
    der weiße Bereich mit Rahmen, kein Grau. Der Druckdialog schlägt die
    angezeigte Seite als Bereich vor. Generator mk33.py (nur Scratchpad).

34_limits.pdf
    Grenzen (H1). Seite 1: drei Q ohne q am Anfang, dann 200
    verschachtelte q ... Q (Grenze im Viewer 32; tiefere q und ihre Q
    werden ignoriert) - eine Treppe kleiner Quadrate nach rechts oben,
    danach grüner Balken und "Ende OK" schwarz, links, normal groß.
    Seite 2: Form A lässt drei q offen (verschoben, rot), Form B hat Q
    ohne q; trotzdem stehen die folgenden Zeilen schwarz und normal
    platziert, der blaue Balken links auf Höhe 300, unten grüner Balken
    und "Ende OK". pdftoppm meldet "Restoring state when no valid
    states to pop" (erwartet). Generator mk34.py (nur Scratchpad).

35_large_jpeg.pdf
    Große JPEGs (H2), Referenz pdftoppm. Jedes Bild: vier Farbflächen
    (blau, gelb / magenta, türkis), weißes Raster 4x4, roter Rahmen,
    schwarze Diagonale von links oben nach rechts unten, grüner Kreis
    links oben (Lage und Seitenverhältnis prüfbar). 1 800x600 RGB und
    6 800x500 progressiv: unverkleinert (Gegenprobe). 2 4200x1600 RGB,
    3 8400x240 Grau, 4 3200x500 CMYK: vorher abgelehnt (leer), jetzt
    verkleinert dekodiert (1/4, 1/8, 1/4). 5 2000x1500 progressiv:
    bleibt im Viewer leer (Koeffizienten über 4 MB, Grenze unverändert).
    Qualität 10, daher Blockartefakte. Generator mk35.py (nur Scratchpad).

36_filters.pdf
    Stream-Filter (Typ-Optimierung long -> short), Referenz pdftoppm und
    mutool. Seite 1: dasselbe 96x96-RGB-Bild (Farbflächen, roter Rahmen,
    weiße Diagonale, grüner Kreis, schwarzer Balken unten) acht Mal: roh,
    ASCIIHex, ASCII85 (mit z), LZW EarlyChange 1 und 0, RunLength,
    ASCII85 + LZW + TIFF-Prädiktor, LZW + PNG-Prädiktor. Alle acht müssen
    gleich aussehen. Seite 2: CCITT G4 (K -1) als 1-Bit-Graubild
    (BlackIs1 true: schwarze Linien auf weiß) und als ImageMask (rot auf
    gelb); der Inhalt der Seite ist ASCII85 + LZW kodiert (Text unten
    muss lesbar sein). Seite 3: Geschwindigkeit, 600x600 LZW (RGB) und
    RunLength (grau). Seite 4 (Zeilenpfad H3a): Indexed mit 64 Farben,
    Grau 8 Bit, RGB mit /Decode [1 0 ...] (invertiert, langsamer Pfad als
    Gegenprobe: gelb/blau/gr�n/rot, Kreis magenta), zwei 3100x120-Bilder
    (Flate RGB, LZW Grau) mit 10 Farbstreifen und wei�er Mittellinie, die
    nur jede 3. Spalte zeigen. Generator mk36.py (nur Scratchpad).

Alle Dateien außer 09 (verschlüsselt, by design) und 10 (kaputte xref,
by design) öffnen mit pypdf ohne Fehler. Rendering-Stichproben (05, 08,
11) wurden zusätzlich mit pdftoppm gegengeprüft.
