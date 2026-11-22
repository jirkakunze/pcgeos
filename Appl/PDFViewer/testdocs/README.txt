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

Alle Dateien außer 09 (verschlüsselt, by design) und 10 (kaputte xref,
by design) öffnen mit pypdf ohne Fehler. Rendering-Stichproben (05, 08,
11) wurden zusätzlich mit pdftoppm gegengeprüft.
