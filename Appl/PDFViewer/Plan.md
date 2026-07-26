# Milestone Plan for Modernizing the PC/GEOS PDF Viewer

## Objectives

The existing PDF Viewer is based on xpdf 0.8 and should be modernized step by step. The main goals are:

- identify and fix potential defects
- clean up and improve the code structure
- improve performance
- take memory usage and binary size into account
- improve the technical foundation and PDF compatibility
- align the code with PC/GEOS project conventions
- reduce risk through small, verifiable development steps

The viewer is not a thin PC/GEOS wrapper around xpdf, but a substantial port. C++ classes were converted into C structures with manually managed lifecycles. Output is generated through GEOS GStrings, and floating-point operations are partly implemented using `WWFixed`.

For this reason, a complete upgrade to a modern xpdf or Poppler version is not recommended as the first step. A gradual stabilization followed by selective backporting of newer PDF features is more appropriate.

---

## Initial Findings from the Code

Particularly relevant areas:

- `main/stream.goc`
  - contains filters, image decoders, Flate, LZW, JPEG, and CCITT support
  - likely the main performance and defect hotspot
- `ui/pdfvu.goc`
  - mixes document management, rendering, VM files, page caching, user interface, and printing
- `main/gfx.goc`
  - processes PDF operators and generates GEOS GStrings
- `gfxFont2.goh`
  - contains large static font and mapping tables

Additional observations:

- many non-descriptive `EC_WARNING(-1)` calls
- potential memory leaks caused by manually ported C++ object lifecycles
- only classic XRef tables are supported
- no XRef stream support
- no object stream support
- damaged XRef tables are not reconstructed
- encrypted files are rejected entirely
- current filter support includes:
  - Flate
  - LZW
  - ASCIIHex
  - ASCII85
  - RunLength
  - CCITT Fax
  - DCT/JPEG
- no support for newer filters such as JPX/JPEG 2000 or JBIG2
- color spaces and PDF functions are only partially implemented
- known resource management issues, for example with VM bitmaps
- a fixed heap setting of `70k`
- many fine-grained allocations and frequent `MemLock` and `MemUnlock` calls
- the name `USE_NATIVE_FLOAT_TYPE` is misleading because the implementation uses a 16.16 fixed-point type

---

# Milestones

## M0 – Establish a Reference Baseline and Test Corpus

### Goal

Before changing the code, the current behavior must be documented and reproducible.

### Work Packages

1. Build the current viewer unchanged with the designated PC/GEOS compiler.
2. Record all compiler and linker warnings.
3. Assemble a representative PDF test corpus:
   - simple PDF 1.2 and PDF 1.3 files
   - multi-page documents
   - compressed content streams
   - JPEG and monochrome images
   - embedded and non-embedded fonts
   - rotated and cropped pages
   - malformed or truncated PDFs
   - larger files
   - later, XRef stream and object stream files
4. Record the following for each document:
   - whether the file opens
   - whether the page count is correct
   - visible rendering defects
   - loading time
   - memory usage
   - behavior when repeatedly opening and closing the file
5. Test repeated opening and closing under SWAT using `hgwalk`.
6. Save reference screenshots for relevant pages.

### Acceptance Criteria

- at least 20 to 30 representative test files
- reproducible build instructions
- complete list of compiler warnings
- baseline values for loading time and memory usage
- documented known rendering defects

### Result

A reliable baseline for all subsequent changes.

---

## M1 – PC/GEOS Compliance and Diagnostic Foundation

### Goal

The code should follow current project conventions and report errors clearly.

### Work Packages

1. Replace all `EC_WARNING(-1)` calls with named warning codes, for example:
   - `PDF_WARNING_INVALID_XREF`
   - `PDF_WARNING_INVALID_OBJECT`
   - `PDF_WARNING_UNSUPPORTED_FILTER`
   - `PDF_WARNING_OUT_OF_MEMORY`
   - `PDF_WARNING_BAD_PAGE_TREE`
2. Define warning codes centrally in a header file.
3. Review outdated copyright and confidentiality notices.
4. Standardize file names, include order, and function declarations.
5. Remove obsolete commented-out xpdf code or preserve it separately as reference material.
6. Clean up remaining C++ fragments in comments and function bodies.
7. Use PC/GEOS types consistently:
   - `Boolean`
   - `byte`
   - `word`
   - `dword`
   - `MemHandle`
   - `VMBlockHandle`
8. Clearly separate public and internal functions.
9. Rename `USE_NATIVE_FLOAT_TYPE` to something clearer, such as `USE_FIXED_POINT_GDOUBLE`.
10. Replace scattered magic numbers with named constants:
    - XRef search window
    - cache limits
    - image buffer sizes
    - maximum recursion depth

### Acceptance Criteria

- no anonymous `EC_WARNING(-1)` calls remain
- new errors can be identified clearly in SWAT
- no unnecessary commented-out implementation blocks remain
- no new compiler warnings are introduced
- behavior remains unchanged compared with M0

### Risk

Low.

---

## M2 – Memory and Resource Stability

### Goal

Eliminate leaks, double frees, invalid handles, and unsafe error paths.

### Areas to Review

- `ObjFree`
- `StreamFree`
- `ParserFree`
- `LexerFree`
- `GfxFree`
- `GfxStateFree`
- `CatalogFree`
- `PageFree`
- `GfxFontFree`
- VM bitmaps
- GStrings
- `MemLock` and `MemUnlock` pairs
- error paths using early `return` or `goto`

### Work Packages

1. Document ownership rules for every major structure:
   - who creates it
   - who owns it
   - who frees it
   - whether contained objects are freed recursively
2. Design initialization functions so that objects can be freed safely at any point.
3. Standardize `XxxInitNull()` and `XxxClear()` functions.
4. Review error paths involving partially initialized structures.
5. Make cleanup functions idempotent where practical:
   - set handles to `0`
   - set pointers to `NULL`
   - set object types to `objNull`
6. Investigate VM bitmap lifecycles.
7. Ensure the page cache is fully released when closing or switching documents.
8. Test repeated open and close operations systematically.
9. Review deeply nested object structures for stack usage.
10. Measure the `heapspace 70k` setting and replace it with a justified value.
11. Evaluate whether `GeodeRequestSpace` and `GeodeReturnSpace` should be used for large temporary allocations.

### Acceptance Criteria

- no steadily increasing memory usage after at least ten open/close cycles
- no orphaned VM blocks or GStrings
- malformed PDFs can be aborted without leaving allocated resources behind
- switching documents works even after a previous load failure
- heap space requirements are measured and documented

### Risk

Medium, but mandatory.

---

## M3 – Harden the Parser and Object Model

### Goal

Malformed or unusual PDFs must not cause crashes, infinite loops, or memory corruption.

### Affected Modules

- `main/lexer.goc`
- `main/parser.goc`
- `main/obj.goc`
- `main/array.goc`
- `main/dict.goc`
- `main/xref.goc`

### Work Packages

1. Check size and offset calculations for overflows.
2. Reject negative lengths and object counts.
3. Enforce dictionary and array bounds consistently.
4. Limit recursion for:
   - indirect object references
   - page trees
   - Form XObjects
   - nested resources
5. Detect cycles in indirect references.
6. Standardize handling of unexpected end-of-file conditions.
7. Remove direct parser access to internal stream data.
8. Return defined error states instead of relying only on warnings.
9. Prepare reconstruction of XRef tables when trailers are damaged.
10. Handle object generations and free XRef entries correctly.
11. Validate allocation sizes before calling `MemAlloc`.
12. Skip unknown objects safely where continued processing is possible.

### Acceptance Criteria

- no crashes with the malformed-file test corpus
- no infinite parsing loops with cyclic references
- damaged files produce understandable error messages
- valid PDFs from M0 continue to render identically

### Risk

Medium.

---

## M4 – Decouple the Architecture and Clean Up the Code

### Goal

Separate the parser, PDF model, renderer, and PC/GEOS user interface.

### Possible Target Structure

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

The code does not need to be split into this many files immediately. Clear interfaces are more important than the exact directory structure.

### Work Packages

1. Move document opening and closing out of `ui/pdfvu.goc`.
2. Move the page cache into a separate module.
3. Encapsulate image decoding more clearly.
4. Centralize PDF error codes.
5. Hide file access behind a small stream interface.
6. Remove direct knowledge of UI objects from the parser and renderer.
7. Split large functions into smaller, understandable units.
8. Centralize shared helper functions.
9. Reduce header dependencies.
10. Modify public structures only through defined functions where possible.

### Acceptance Criteria

- document opening can be understood without UI-specific code
- parsing and rendering can be tested separately
- parser changes do not require UI changes
- no functional changes compared with M3

### Risk

Medium.

---

## M5 – Low-Risk Performance Improvements

### Goal

Achieve measurable improvements without changing supported PDF functionality.

### Likely Areas for Improvement

#### `MemLock` and `MemUnlock`

Locks should be moved out of inner loops wherever possible, especially in decoders.

#### File Access

Many small `StreamGetChar()` calls are probably expensive. A buffered input stream could significantly improve parser and decoder performance.

#### Allocations

Temporary objects and buffers are created and freed frequently. Reusable buffers could reduce fragmentation and allocation overhead.

#### Operator Lookup

Graphics operators may be resolved more efficiently using better tables or dispatch mechanisms.

#### Page Cache

Cache limits should be based on actual memory usage rather than only on a fixed number of pages.

### Work Packages

1. Profile the following separately:
   - opening a document
   - reading the XRef table
   - parsing page content
   - decoding images
   - generating GStrings
   - displaying pages
2. Introduce buffered file access.
3. Move locks out of inner loops.
4. Replace byte-by-byte buffer access with block operations where practical.
5. Reduce frequent small allocations.
6. Move constant tables into appropriate read-only or shared resources.
7. Identify unused tables and font data.
8. Generate page GStrings only when required.
9. Add LRU behavior to the page cache.
10. Preserve cancellation checks during expensive image operations.
11. Measure every optimization independently.

### Acceptance Criteria

Realistic initial targets:

- document opening at least 10 to 20 percent faster
- image-heavy pages at least 15 percent faster
- no increase in peak heap usage
- no significant increase in binary size
- rendering remains functionally identical

### Risk

Low to medium.

---

## M6 – PDF 1.5 Foundation: XRef Streams and Object Streams

### Goal

Implement the most important compatibility improvement for newer PDFs.

Many newer files fail not because of unsupported graphics operators, but because their objects are stored in XRef streams and object streams.

### Part A: XRef Streams

1. Detect whether `startxref` points to an indirect object instead of the `xref` keyword.
2. Read the XRef stream dictionary:
   - `/Type /XRef`
   - `/Size`
   - `/W`
   - `/Index`
   - `/Prev`
   - `/Root`
   - `/Info`
   - `/ID`
3. Decode the stream using the existing Flate support.
4. Process entry types:
   - type 0: free
   - type 1: normal file offset
   - type 2: object stored in an object stream
5. Support both classic and streamed XRefs.
6. Support hybrid-reference files using `/XRefStm`.

### Part B: Object Streams

1. Detect `/ObjStm`.
2. Read `/N` and `/First`.
3. Parse the object index.
4. Extract individual objects on demand.
5. Add a small cache for recently used object streams.
6. Guard against recursion and invalid offsets.

### Acceptance Criteria

- classic PDFs continue to work unchanged
- pure XRef-stream PDFs can be opened
- objects from `/ObjStm` are read correctly
- hybrid-reference files work
- invalid entries do not cause crashes

### Risk

High, but clearly bounded.

---

## M7 – Rendering and Color Compatibility

### Goal

Files that open successfully should also render correctly.

### Prioritized Features

1. Color spaces:
   - `Indexed`
   - `Separation`
   - `DeviceN`
   - `ICCBased`, initially with a reasonable approximation
   - improved handling of `CalGray` and `CalRGB`
2. PDF functions:
   - function type 0: sampled
   - function type 2: exponential interpolation
   - function type 3: stitching
   - function type 4 at a later stage if required
3. Graphics state:
   - ExtGState
   - initially ignore alpha or treat it as fully opaque
   - degrade unsupported blend modes gracefully
4. Improve Form XObject handling.
5. Review clipping and transformation matrices.
6. Unify CropBox, MediaBox, and rotation handling between display and printing.
7. Reduce rounding errors in vector graphics.
8. Optionally retain higher internal precision and round only when calling GEOS graphics functions.

### Deliberate Limitations

Transparency groups, complex blend modes, and full color management should not be part of the first implementation stage. They are comparatively expensive for a 16-bit real-mode environment and GEOS GStrings.

### Acceptance Criteria

- no incorrect black areas caused by unsupported color spaces
- Form XObjects and clipping work in the test files
- display and printing use consistent page boundaries
- unsupported features degrade in a controlled way

### Risk

Medium to high.

---

## M8 – Fonts and Text Rendering

### Goal

Improve the visible quality and reliability of text rendering.

### Work Packages

1. Clean up font detection and fallback selection.
2. Map the Standard 14 fonts reliably.
3. Improve encoding support:
   - WinAnsi
   - MacRoman
   - StandardEncoding
   - Symbol
   - ZapfDingbats
   - custom Differences
4. Use `ToUnicode` CMaps initially for text extraction and later, where useful, for rendering.
5. Clearly separate Type 1, TrueType, and CID font paths.
6. Handle embedded fonts incrementally:
   - evaluate embedded encodings
   - substitute suitable system fonts
   - handle unknown glyphs predictably
7. Review the large static font tables in `gfxFont2.goh`:
   - identify actually required entries
   - evaluate compression
   - split data into loadable resources
8. Optionally prepare a text-object model for:
   - copying text
   - page text extraction
   - search

### Acceptance Criteria

- standard fonts render consistently
- Symbol fonts do not interfere with regular text
- encoding differences work
- large font tables do not occupy heap unnecessarily
- text positioning remains stable

### Risk

Medium to high.

---

## M9 – Additional Decoders and Modern PDF Features

### Goal

Extend compatibility beyond the core PDF 1.5 feature set.

### Recommended Order

1. Improve Flate and predictor compatibility.
2. Improve JPEG robustness.
3. Add JPX/JPEG 2000 only after a feasibility study.
4. Add JBIG2 only after a feasibility study.
5. Encryption:
   - first support older standard security handlers
   - add modern AES support only if memory and binary-size budgets allow it
6. Linearized PDF:
   - first ensure correct opening
   - true progressive loading is not mandatory
7. Annotations:
   - links
   - simple text and widget annotations
8. Optional features:
   - text search
   - text copying
   - document information
   - bookmarks and outlines

### Evaluation Criteria

Each larger feature should be assessed individually based on:

- binary size
- heap usage
- stack usage
- performance on real hardware
- maintenance effort
- practical user benefit

---

# Recommended Release Stages

## Release A – Stabilized Legacy Viewer

Includes M0 through M5:

- defect cleanup
- PC/GEOS compliance
- memory stability
- parser hardening
- cleaner architecture
- low-risk performance improvements

This release would still support largely the same PDF feature set, but would be much more stable and maintainable.

## Release B – PDF 1.5 Core

Additionally includes M6:

- XRef streams
- object streams
- hybrid references

This release is expected to provide the largest practical compatibility improvement.

## Release C – Improved Rendering

Includes M7 and selected parts of M8:

- improved color-space support
- additional PDF functions
- better font and encoding support
- more reliable vector rendering

M9 should then be implemented feature by feature.

---

# Recommended Priority Order

1. Test corpus and baseline measurements
2. Meaningful error codes
3. Memory and resource review
4. Parser hardening
5. Split up `ui/pdfvu.goc`
6. Buffered stream access and decoder optimization
7. XRef streams
8. Object streams
9. Color spaces
10. Fonts

## Features That Should Not Be Implemented First

- complete migration to a current Poppler version
- JPEG 2000
- JBIG2
- transparency
- full embedded-font rendering
- modern encryption
- extensive new UI features

These items would add substantial technical uncertainty before the current foundation has been stabilized.

---

# Recommended First Development Package

## PDF Viewer Stabilization 1

Scope:

- complete M0
- complete M1
- memory analysis from M2

This first package should not introduce any visible new PDF features.

### Expected Result

- reproducible build
- documented current feature set
- reliable test baseline
- meaningful error diagnostics
- documented ownership and cleanup rules
- initial memory and performance measurements
- a clear basis for deciding which parts of the old xpdf architecture should be retained or replaced
