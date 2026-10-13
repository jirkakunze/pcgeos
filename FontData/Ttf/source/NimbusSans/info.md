# Font Documentation: NimbusSans

## 1. General Information
* **Original File Name:** NimbusSans-Regular.ttf, NimbusSans-Italic.ttf
* **GEOS File Name:** nsansre.ttf, nsansri.ttf
* **License:** [https://github.com/ArtifexSoftware/urw-base35-fonts/blob/master/COPYING](https://github.com/ArtifexSoftware/urw-base35-fonts/blob/master/COPYING)
* **Upstream Source:** https://github.com/ArtifexSoftware/urw-base35-fonts

## 2. PC/GEOS Specifics
* **GEOS Font ID:** Mapped in geos.ini to 0x5200
* **Supported Styles:** Regular, Italic

## 3. Modifications
*Document any changes made to the original font for optimal display in PC/GEOS.*

### Technical Adjustments:
- [x] **Charset Reduction:** Removed all non-GEOS glyphs to save memory.
- [x] **Kerning Pairs:** Reduced to a minimum to optimize driver performance and memory.

### Visual Optimizations / Hinting:
- [x] **Auto-Hinting:** Applied auto-hinting for low-resolution screens
- [x] **Optimized Hinting:** (Hinting specifically for **Black & White rendering** to ensure sharp stems and consistent glyph heights)
- [x] **Blue Values:** (Alignment zones adjusted for typical 72dpi screens)

## 4. Known Issues / To-Do

---
*Last updated: 10.10.2026 by jkunze*
