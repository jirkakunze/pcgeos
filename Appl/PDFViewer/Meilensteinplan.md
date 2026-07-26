# Meilensteinplan zur Modernisierung des PC/GEOS PDF Viewers

## Zielsetzung

Der bestehende PDF Viewer basiert auf xpdf 0.8 und soll schrittweise modernisiert werden. Dabei stehen folgende Ziele im Mittelpunkt:

- potenzielle Fehler finden und beheben
- Code aufräumen und besser strukturieren
- Performance verbessern
- Speicherbedarf und Binärgröße berücksichtigen
- technische Basis und PDF-Kompatibilität verbessern
- Vorgaben und Konventionen des PC/GEOS-Projekts einhalten
- Risiken durch kleine, überprüfbare Entwicklungsschritte begrenzen

Der Viewer ist keine dünne PC/GEOS-Hülle um xpdf, sondern eine tiefgreifende Portierung. C++-Klassen wurden in C-Strukturen und manuell verwaltete Lebenszyklen überführt. Die Ausgabe erfolgt über GEOS-GStrings, und Fließkommaoperationen werden teilweise mit `WWFixed` umgesetzt.

Daher ist eine vollständige Aktualisierung auf eine moderne xpdf- oder Poppler-Version nicht als erster Schritt zu empfehlen. Sinnvoller ist eine schrittweise Stabilisierung mit anschließendem gezielten Rückportieren neuer PDF-Funktionen.

---

## Erste Erkenntnisse aus dem Code

Besonders relevante Bereiche:

- `main/stream.goc`
  - enthält Filter, Bilddecoder, Flate, LZW, JPEG und CCITT
  - voraussichtlich größter Performance- und Fehlerschwerpunkt
- `ui/pdfvu.goc`
  - vermischt Dokumentverwaltung, Darstellung, VM-Dateien, Seiten-Cache, Benutzeroberfläche und Druck
- `main/gfx.goc`
  - verarbeitet PDF-Operatoren und erzeugt GEOS-GStrings
- `gfxFont2.goh`
  - enthält große statische Font- und Zuordnungstabellen

Weitere Auffälligkeiten:

- zahlreiche nichtssagende `EC_WARNING(-1)`
- mögliche Speicherlecks durch manuell portierte C++-Objektlebenszyklen
- nur klassische XRef-Tabellen unterstützt
- keine XRef-Streams
- keine Object Streams
- beschädigte XRef-Tabellen werden nicht rekonstruiert
- verschlüsselte Dateien werden grundsätzlich abgelehnt
- vorhandene Unterstützung unter anderem für:
  - Flate
  - LZW
  - ASCIIHex
  - ASCII85
  - RunLength
  - CCITT Fax
  - DCT/JPEG
- keine Unterstützung für modernere Filter wie JPX/JPEG 2000 oder JBIG2
- Farbräume und PDF-Funktionen teilweise unvollständig
- bekannte Ressourcenprobleme, beispielsweise bei VM-Bitmaps
- pauschaler Heap-Eintrag von `70k`
- viele feingranulare Allokationen sowie häufige `MemLock`- und `MemUnlock`-Aufrufe
- irreführender Name `USE_NATIVE_FLOAT_TYPE`, obwohl intern ein 16.16-Festkommatyp verwendet wird

---

# Meilensteine

## M0 – Referenzstand und Testkorpus herstellen

### Ziel

Bevor der Code verändert wird, muss reproduzierbar feststehen, was heute funktioniert.

### Arbeitspakete

1. Aktuellen Viewer unverändert mit dem vorgesehenen PC/GEOS-Compiler bauen.
2. Alle Compiler- und Linkerwarnungen erfassen.
3. Ein repräsentatives PDF-Testkorpus zusammenstellen:
   - einfache PDF-1.2- und PDF-1.3-Dateien
   - mehrseitige Dokumente
   - komprimierte Inhaltsströme
   - JPEG- und Schwarzweißbilder
   - eingebettete und nicht eingebettete Fonts
   - gedrehte und beschnittene Seiten
   - fehlerhafte oder abgeschnittene PDFs
   - größere Dateien
   - später XRef-Streams und Object Streams
4. Für jedes Dokument erfassen:
   - Datei öffnet oder öffnet nicht
   - Seitenzahl korrekt
   - sichtbare Darstellungsfehler
   - Ladezeit
   - Speicherverbrauch
   - Verhalten beim wiederholten Öffnen und Schließen
5. Mehrfaches Öffnen und Schließen unter SWAT mit `hgwalk` prüfen.
6. Relevante Seiten als Referenz-Screenshots sichern.

### Abnahmekriterien

- mindestens 20 bis 30 repräsentative Testdateien
- reproduzierbare Build-Anleitung
- vollständige Liste aller Compilerwarnungen
- Ausgangswerte für Ladezeit und Speicherverbrauch
- dokumentierte bekannte Darstellungsfehler

### Ergebnis

Eine belastbare Ausgangsbasis für alle weiteren Änderungen.

---

## M1 – PC/GEOS-Konformität und diagnostische Basis

### Ziel

Der Code soll den heutigen Projektkonventionen entsprechen und Fehler eindeutig melden.

### Arbeitspakete

1. Alle `EC_WARNING(-1)` durch benannte Warncodes ersetzen, beispielsweise:
   - `PDF_WARNING_INVALID_XREF`
   - `PDF_WARNING_INVALID_OBJECT`
   - `PDF_WARNING_UNSUPPORTED_FILTER`
   - `PDF_WARNING_OUT_OF_MEMORY`
   - `PDF_WARNING_BAD_PAGE_TREE`
2. Warncodes zentral in einer Headerdatei definieren.
3. Veraltete Copyright- und Vertraulichkeitsvermerke prüfen.
4. Dateinamen, Include-Reihenfolge und Funktionsdeklarationen vereinheitlichen.
5. Nicht mehr benötigten, auskommentierten Original-xpdf-Code entfernen oder separat sichern.
6. C++-Reste in Kommentaren und Funktionskörpern bereinigen.
7. PC/GEOS-Typen konsequent verwenden:
   - `Boolean`
   - `byte`
   - `word`
   - `dword`
   - `MemHandle`
   - `VMBlockHandle`
8. Öffentliche und interne Funktionen klar trennen.
9. `USE_NATIVE_FLOAT_TYPE` verständlich umbenennen, zum Beispiel in `USE_FIXED_POINT_GDOUBLE`.
10. Globale Konstanten statt verstreuter magischer Zahlen verwenden:
    - XRef-Suchfenster
    - Cache-Grenzen
    - Bildpuffergrößen
    - maximale Rekursionstiefe

### Abnahmekriterien

- keine anonymen `EC_WARNING(-1)` mehr
- neue Fehler sind in SWAT eindeutig zuzuordnen
- keine unnötigen auskommentierten Implementierungsblöcke
- keine neuen Compilerwarnungen
- Verhalten gegenüber M0 unverändert

### Risiko

Niedrig.

---

## M2 – Speicher- und Ressourcenstabilität

### Ziel

Leaks, doppelte Freigaben, ungültige Handles und problematische Fehlerpfade beseitigen.

### Besonders zu prüfen

- `ObjFree`
- `StreamFree`
- `ParserFree`
- `LexerFree`
- `GfxFree`
- `GfxStateFree`
- `CatalogFree`
- `PageFree`
- `GfxFontFree`
- VM-Bitmaps
- GStrings
- `MemLock`- und `MemUnlock`-Paare
- Fehlerpfade mit frühem `return` oder `goto`

### Arbeitspakete

1. Für jede größere Struktur Besitzregeln dokumentieren:
   - Wer erzeugt sie?
   - Wer besitzt sie?
   - Wer gibt sie frei?
   - Werden enthaltene Objekte rekursiv freigegeben?
2. Initialisierungsfunktionen so gestalten, dass jederzeit eine sichere Freigabe möglich ist.
3. `XxxInitNull()`- und `XxxClear()`-Funktionen vereinheitlichen.
4. Fehlerpfade auf unvollständig initialisierte Strukturen prüfen.
5. Freigabefunktionen soweit sinnvoll idempotent machen:
   - Handles anschließend auf `0`
   - Zeiger anschließend auf `NULL`
   - Objekttyp anschließend auf `objNull`
6. VM-Bitmap-Lebenszyklen untersuchen.
7. Seitencache beim Dokumentwechsel und Schließen vollständig freigeben.
8. Wiederholtes Öffnen und Schließen systematisch testen.
9. Tiefe Objektstrukturen auf Stackverbrauch prüfen.
10. `heapspace 70k` vermessen und anschließend begründet einstellen.
11. Prüfen, ob bei größeren temporären Allokationen `GeodeRequestSpace` und `GeodeReturnSpace` verwendet werden sollten.

### Abnahmekriterien

- nach mindestens zehn Öffnen-/Schließen-Zyklen kein stetig steigender Speicherverbrauch
- keine verwaisten VM-Blöcke oder GStrings
- fehlerhafte PDFs können ohne Ressourcenverlust abgebrochen werden
- Dokumentwechsel funktioniert auch nach einem vorherigen Ladefehler
- Heapspace ist gemessen und dokumentiert

### Risiko

Mittel, aber zwingend notwendig.

---

## M3 – Parser und Objektmodell härten

### Ziel

Fehlerhafte oder ungewöhnliche PDFs dürfen nicht zu Abstürzen, Endlosschleifen oder Speicherüberschreibungen führen.

### Betroffene Module

- `main/lexer.goc`
- `main/parser.goc`
- `main/obj.goc`
- `main/array.goc`
- `main/dict.goc`
- `main/xref.goc`

### Arbeitspakete

1. Größen- und Offsetberechnungen auf Überläufe prüfen.
2. Negative Längen und Objektanzahlen ablehnen.
3. Dictionary- und Array-Grenzen konsequent kontrollieren.
4. Rekursion begrenzen:
   - indirekte Objektreferenzen
   - Seitenbaum
   - Form-XObjects
   - verschachtelte Ressourcen
5. Zyklen in indirekten Referenzen erkennen.
6. Verhalten bei unerwartetem Dateiende vereinheitlichen.
7. Direktzugriffe des Parsers auf interne Stream-Daten beseitigen.
8. Fehlerstatus nicht nur über Warnungen, sondern über definierte Rückgabewerte transportieren.
9. Rekonstruktion einer XRef-Tabelle bei beschädigtem Trailer vorbereiten.
10. Objektgenerationen und freie XRef-Einträge korrekt behandeln.
11. Größen vor `MemAlloc` validieren.
12. Unbekannte Objekte kontrolliert überspringen, sofern eine sichere Fortsetzung möglich ist.

### Abnahmekriterien

- kein Absturz mit dem Fehlerkorpus
- kein endloses Parsen bei zyklischen Referenzen
- defekte Dateien erzeugen eine verständliche Fehlermeldung
- gültige PDFs aus M0 werden weiterhin identisch dargestellt

### Risiko

Mittel.

---

## M4 – Architektur entkoppeln und Code aufräumen

### Ziel

Parser, PDF-Modell, Renderer und PC/GEOS-Oberfläche voneinander trennen.

### Mögliche Zielstruktur

```text
document/
    pdfDocument.goc
    pdfPageCache.goc
    pdfErrors.goc

parser/
    pdfLexer.goc
    pdfParser.goc
    pdfObject.goc
    pdfXRef.goc
    pdfStream.goc

render/
    pdfGfx.goc
    pdfGfxState.goc
    pdfImage.goc
    pdfFont.goc
    pdfColor.goc

ui/
    pdfApplication.goc
    pdfDocumentUI.goc
    pdfPageControl.goc
    pdfView.goc
```

Die tatsächliche Aufteilung muss nicht sofort so fein erfolgen. Zunächst sind klare Schnittstellen wichtiger.

### Arbeitspakete

1. Dokumentöffnung und -schließung aus `ui/pdfvu.goc` herauslösen.
2. Seiten-Cache in ein eigenes Modul verschieben.
3. Bilddekodierung besser kapseln.
4. PDF-Fehlercodes zentralisieren.
5. Dateizugriffe hinter einer kleinen Stream-Schnittstelle verbergen.
6. Direkte Kenntnis von UI-Objekten aus Parser und Renderer entfernen.
7. Große Funktionen in nachvollziehbare Teilfunktionen zerlegen.
8. Gemeinsam verwendete Hilfsfunktionen zentralisieren.
9. Header-Abhängigkeiten reduzieren.
10. Öffentliche Strukturen möglichst nur über definierte Funktionen verändern.

### Abnahmekriterien

- Dokumentöffnung kann ohne UI-spezifischen Code nachvollzogen werden
- Rendering und Parsing sind getrennt testbar
- Änderungen im Parser erfordern keine Änderungen im UI-Modul
- keine funktionalen Änderungen gegenüber M3

### Risiko

Mittel.

---

## M5 – Risikoarme Performanceoptimierungen

### Ziel

Spürbare Verbesserungen ohne Änderung der PDF-Funktionalität.

### Wahrscheinliche Ansatzpunkte

#### `MemLock` und `MemUnlock`

Locks sollten insbesondere in Decodern möglichst aus inneren Schleifen herausgezogen werden.

#### Dateizugriffe

Viele kleine `StreamGetChar()`-Aufrufe sind wahrscheinlich teuer. Ein gepufferter Eingabestream kann Parser und Decoder deutlich beschleunigen.

#### Allokationen

Temporäre Objekte und Puffer werden häufig erzeugt und wieder freigegeben. Wiederverwendbare Puffer können Fragmentierung und Verwaltungsaufwand reduzieren.

#### Operator-Lookup

Der Grafikoperator kann über effizientere Tabellen oder Dispatch-Mechanismen gesucht werden.

#### Seitencache

Die Cache-Grenze sollte sich am tatsächlichen Speicherverbrauch orientieren und nicht nur an einer festen Seitenanzahl.

### Arbeitspakete

1. Profilierung getrennt für:
   - Dokument öffnen
   - XRef lesen
   - Seiteninhalt parsen
   - Bilder dekodieren
   - GString erzeugen
   - Seite darstellen
2. Gepufferten Dateistream einführen.
3. Locks aus inneren Schleifen herausziehen.
4. Byteweise Pufferzugriffe durch Blockzugriffe ersetzen.
5. Häufige kleine Allokationen reduzieren.
6. Konstante Tabellen in geeignete read-only- oder shared-Ressourcen verschieben.
7. Nicht verwendete Tabellen und Fontdaten identifizieren.
8. Seiten-GStrings nur bei Bedarf erzeugen.
9. Cache mit LRU-Verhalten versehen.
10. Abbruchprüfungen bei teuren Bildoperationen beibehalten.
11. Wirkung jeder Optimierung einzeln messen.

### Abnahmekriterien

Realistische erste Ziele:

- Dokumentöffnung mindestens 10 bis 20 Prozent schneller
- bildreiche Seiten mindestens 15 Prozent schneller
- kein höherer maximaler Heapverbrauch
- Binärgröße wächst nicht wesentlich
- Darstellung bleibt fachlich identisch

### Risiko

Niedrig bis mittel.

---

## M6 – PDF-1.5-Basis: XRef-Streams und Object Streams

### Ziel

Den wichtigsten Kompatibilitätssprung für neuere PDFs umsetzen.

Viele neuere Dateien scheitern nicht an Grafikoperatoren, sondern bereits daran, dass ihre Objekte in XRef- und Object Streams abgelegt sind.

### Teil A: XRef-Streams

1. Erkennen, ob `startxref` auf ein indirektes Objekt statt auf `xref` zeigt.
2. XRef-Stream-Dictionary lesen:
   - `/Type /XRef`
   - `/Size`
   - `/W`
   - `/Index`
   - `/Prev`
   - `/Root`
   - `/Info`
   - `/ID`
3. Stream durch die vorhandene Flate-Unterstützung dekodieren.
4. Eintragstypen verarbeiten:
   - Typ 0: frei
   - Typ 1: normaler Dateioffset
   - Typ 2: Objekt in Object Stream
5. Klassische und gestreamte XRefs gemeinsam unterstützen.
6. Hybrid-XRef-Dateien über `/XRefStm` unterstützen.

### Teil B: Object Streams

1. `/ObjStm` erkennen.
2. `/N` und `/First` auswerten.
3. Objektindex lesen.
4. Einzelne Objekte bei Bedarf extrahieren.
5. Kleinen Cache für zuletzt verwendete Object Streams einführen.
6. Rekursion und ungültige Offsets absichern.

### Abnahmekriterien

- klassische PDFs funktionieren unverändert
- reine XRef-Stream-PDFs können geöffnet werden
- Objekte aus `/ObjStm` werden korrekt gelesen
- hybride Referenztabellen funktionieren
- fehlerhafte Einträge führen nicht zum Absturz

### Risiko

Hoch, aber klar begrenzbar.

---

## M7 – Rendering- und Farbkompatibilität

### Ziel

Geöffnete Dateien sollen auch korrekt dargestellt werden.

### Priorisierte Funktionen

1. Farbräume:
   - `Indexed`
   - `Separation`
   - `DeviceN`
   - `ICCBased` zunächst mit sinnvoller Näherung
   - verbesserte Behandlung von `CalGray` und `CalRGB`
2. PDF-Funktionen:
   - Funktionstyp 0: sampled
   - Funktionstyp 2: exponential interpolation
   - Funktionstyp 3: stitching
   - Funktionstyp 4 gegebenenfalls später
3. Grafikzustand:
   - ExtGState
   - Alpha zunächst ignorieren oder auf deckend abbilden
   - Blend Modes kontrolliert degradieren
4. Form-XObjects robuster verarbeiten.
5. Clipping und Transformationsmatrizen prüfen.
6. CropBox, MediaBox und Rotation zwischen Anzeige und Druck vereinheitlichen.
7. Rundungsprobleme bei Vektorgrafik reduzieren.
8. Optional intern höhere Präzision verwenden und erst beim GEOS-Aufruf runden.

### Bewusste Einschränkungen

Transparenzgruppen, komplexe Blend Modes und vollständiges Color Management sollten nicht in der ersten Ausbaustufe umgesetzt werden. Sie sind für 16-Bit-Real-Mode und GEOS-GStrings vergleichsweise teuer.

### Abnahmekriterien

- keine falschen schwarzen Flächen aufgrund unbekannter Farbräume
- Form-XObjects und Clipping funktionieren in den Testdateien
- Anzeige und Druck verwenden dieselben Seitengrenzen
- nicht unterstützte Funktionen werden kontrolliert angenähert

### Risiko

Mittel bis hoch.

---

## M8 – Fonts und Textdarstellung

### Ziel

Die sichtbare Qualität der Textdarstellung systematisch verbessern.

### Arbeitspakete

1. Font-Erkennung und Ersatzfont-Auswahl bereinigen.
2. Standard-14-Fonts zuverlässig abbilden.
3. Encoding-Unterstützung verbessern:
   - WinAnsi
   - MacRoman
   - StandardEncoding
   - Symbol
   - ZapfDingbats
   - benutzerdefinierte Differences
4. `ToUnicode`-CMaps zunächst für Textgewinnung, später gegebenenfalls für Darstellung verwenden.
5. Type-1-, TrueType- und CID-Fontpfade klar trennen.
6. Eingebettete Fonts schrittweise behandeln:
   - eingebettetes Encoding auswerten
   - passende Systemfonts substituieren
   - unbekannte Glyphen kontrolliert darstellen
7. Große statische Fonttabellen in `gfxFont2.goh` untersuchen:
   - tatsächlich benötigte Einträge
   - mögliche Komprimierung
   - Aufteilung in ladbare Ressourcen
8. Optional ein Textobjekt-Modell vorbereiten für:
   - Textkopieren
   - Seitentext
   - Suche

### Abnahmekriterien

- Standardfonts werden konsistent dargestellt
- Symbolfonts verschlechtern regulären Text nicht
- Encoding-Differenzen funktionieren
- große Fonttabellen belegen nicht unnötig dauerhaft Heap
- Textpositionierung bleibt stabil

### Risiko

Mittel bis hoch.

---

## M9 – Weitere Decoder und moderne PDF-Funktionen

### Ziel

Erweiterung über die grundlegende PDF-1.5-Kompatibilität hinaus.

### Empfohlene Reihenfolge

1. Verbesserte Flate- und Predictor-Kompatibilität.
2. Robustere JPEG-Behandlung.
3. JPX/JPEG 2000 nur nach Machbarkeitsprüfung.
4. JBIG2 nur nach Machbarkeitsprüfung.
5. Verschlüsselung:
   - zunächst ältere Standard-Security-Handler
   - moderne AES-Verfahren nur bei vertretbarem Speicher- und Codegrößenbedarf
6. Linearized PDF:
   - zunächst korrekt öffnen
   - echtes progressives Laden ist nicht zwingend erforderlich
7. Annotations:
   - Links
   - einfache Text- und Widget-Anmerkungen
8. Optional:
   - Textsuche
   - Textkopieren
   - Dokumentinformationen
   - Lesezeichen und Outlines

### Bewertungskriterien

Jede größere Funktion sollte einzeln bewertet werden nach:

- Binärgröße
- Heapbedarf
- Stackbedarf
- Geschwindigkeit auf echter Hardware
- Wartungsaufwand
- praktischem Nutzen

---

# Empfohlene Release-Schnitte

## Release A – Stabilized Legacy Viewer

Enthält M0 bis M5:

- Fehlerbereinigung
- PC/GEOS-Konformität
- Speicherstabilität
- Parser-Härtung
- aufgeräumte Architektur
- risikoarme Performanceverbesserungen

Dieser Stand unterstützt noch weitgehend denselben PDF-Umfang, ist aber wesentlich stabiler und wartbarer.

## Release B – PDF 1.5 Core

Enthält zusätzlich M6:

- XRef-Streams
- Object Streams
- hybride XRefs

Dieser Stand dürfte den größten praktischen Kompatibilitätsgewinn bringen.

## Release C – Improved Rendering

Enthält M7 und ausgewählte Teile von M8:

- verbesserte Farbräume
- PDF-Funktionen
- bessere Font- und Encoding-Unterstützung
- stabilere Vektorgrafik

M9 sollte anschließend funktionsweise umgesetzt werden.

---

# Empfohlene Priorisierung

1. Testkorpus und Messwerte
2. Aussagekräftige Fehlercodes
3. Speicher- und Ressourcenprüfung
4. Parser-Härtung
5. Aufteilung von `ui/pdfvu.goc`
6. Gepufferter Stream und Decoderoptimierung
7. XRef-Streams
8. Object Streams
9. Farbräume
10. Fonts

## Nicht als Erstes umsetzen

- vollständige Migration auf aktuelles Poppler
- JPEG 2000
- JBIG2
- Transparenz
- vollständiges Rendering eingebetteter Fonts
- moderne Verschlüsselung
- umfangreiche neue UI-Funktionen

Diese Punkte würden die technische Unsicherheit deutlich erhöhen, bevor die bestehende Basis stabil ist.

---

# Empfohlener erster Entwicklungsblock

## PDF Viewer Stabilization 1

Umfang:

- M0 vollständig
- M1 vollständig
- Speicheranalyse aus M2

Dieser erste Block soll noch keine sichtbaren neuen PDF-Funktionen enthalten.

### Erwartetes Ergebnis

- reproduzierbarer Build
- dokumentierter Funktionsumfang
- belastbare Testbasis
- verständliche Fehlerdiagnose
- dokumentierte Besitz- und Freigaberegeln
- erste Messergebnisse zu Speicherverbrauch und Performance
- klare Grundlage für die Entscheidung, welche Teile der alten xpdf-Architektur weiterverwendet oder ersetzt werden sollen
