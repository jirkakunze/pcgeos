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
    GEOS zeichnet nur die Typen 1-3 mit Funktionstyp 0/2/3 (seit 13b′,
    Codegröße); Netze/Patches (Typ 4-7) und PostScript-Funktionen
    (Typ 4: A5, A8, Seite 3 links) bleiben bei 'sh' leer, als
    Pattern-Füllung erscheint die Fläche einfarbig. Axial/radial mit
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
    Generator mk29.py (nur Scratchpad).

Alle Dateien außer 09 (verschlüsselt, by design) und 10 (kaputte xref,
by design) öffnen mit pypdf ohne Fehler. Rendering-Stichproben (05, 08,
11) wurden zusätzlich mit pdftoppm gegengeprüft.
