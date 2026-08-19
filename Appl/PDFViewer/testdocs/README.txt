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
    Mit leerem Nutzer-Passwort RC4-128-verschlüsselt (bewusst der
    schwache, zeitgemäße Algorithmus für einen xpdf-0.80-Port, nicht
    modernes AES). XRefCheckEncrypted() sollte die Verschlüsselung
    erkennen; XRefIsEncrypted() ist aktuell hart auf gFalse gestubbt.
    Erwartetes Ergebnis: sauberer Fehlschlag/Ablehnung, kein Absturz
    und keine Darstellung von Datenmüll.

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

Alle Dateien außer 09 (verschlüsselt, by design) und 10 (kaputte xref,
by design) öffnen mit pypdf ohne Fehler. Rendering-Stichproben (05, 08,
11) wurden zusätzlich mit pdftoppm gegengeprüft.
