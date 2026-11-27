# PDF Viewer Test Documents

This folder contains the ten test documents of the PC/GEOS PDF viewer.
Each page starts with a short description of its features and of the
expected result in the viewer.

The documents are generated with mktestdocs.py (not in this repository,
see ~/pcgeos-tools/PdfTestDocs on the development system). Do not edit the
documents by hand; change the generator and make them again.

## Documents

01_text.pdf (3 pages)
: The 14 standard fonts. WinAnsi, MacRoman and Standard encoding,
  /Differences with glyph names. Text state (Tc, Tw, Tz, TL, Ts, TJ, Tm).
  Rendering modes 0 to 7 with text clipping. An embedded Type 1 font with a
  built-in encoding. TeX font names. Greek and math characters.

02_cid_fonts.pdf (1 page)
: Type0 fonts with Identity-H, /W, /DW and /ToUnicode (with a ligature).
  A font without /ToUnicode and a font with Identity-V (both not drawn).

03_graphics.pdf (2 pages)
: Paths, curves, nonzero and even-odd fill, clipping. Line width, caps,
  joins, miter limit, dashes. ExtGState line parameters and font. The cm
  operator. q/Q nested 40 levels deep.

04_color.pdf (3 pages)
: Device, ICCBased, CalGray, CalRGB, Indexed, Separation and DeviceN color
  spaces, /DefaultRGB. Constant alpha (ca, CA), soft mask, transparency
  group. Shadings and patterns (the viewer uses one replacement color).

05_images.pdf (3 pages)
: Images with 1 to 16 bits per component, Indexed, /Decode, inline images.
  Stencil masks, color key masks, /Mask stream, /SMask, image with ca 0.5.
  JPEG in gray, RGB, CMYK and progressive, and a large JPEG that the
  viewer decodes at a reduced size.

06_filters.pdf (2 pages)
: The same picture with every filter: ASCIIHex, ASCII85, LZW (EarlyChange
  0 and 1, clear code), RunLength, Flate, PNG and TIFF predictors, filter
  chains. CCITT G4 as image and as stencil. Large images to measure the
  decoding time.

07_pages_links.pdf (10 pages)
: Letter, landscape, /Rotate 90, 180, 270, CropBox. Nested form XObjects
  and a form that calls itself. Attributes inherited from the page tree.
  A damaged page (error message expected). Links with all destination
  types. Document information with UTF-16 text.

08_file_structure.pdf (2 pages)
: Hybrid file (xref table and /XRefStm), cross-reference stream with a
  predictor, object stream, direct font dictionaries, incremental update.

09_rc4_no_password.pdf (2 pages)
: RC4 128-bit encryption with an empty user password. Printing and copying
  are not allowed. Opens without a password dialog.

10_rc4_password.pdf (2 pages)
: RC4 40-bit encryption. User password "user" (printing and copying not
  allowed), owner password "owner" (no restrictions).

## Notes for other PDF readers

- Other PDF readers can show some pages differently. The page text tells
  where: for example real gradients and tiles (04) or other glyphs for the
  not embedded CID fonts (02).
- pdftoppm (poppler) shows the full MediaBox. Thus document 07, page 7
  also shows the red frame outside the crop box. Use the option -cropbox
  to see only the crop box.
