# Media fixtures

The APNG inputs under `apng/` are generated entirely from the pixel matrices in
`generate-apng-fixtures.mjs`. They are project-authored MIT-licensed fixtures;
no external image or historical implementation is required.
`apng/issue-1-first-frame.png` recreates the relevant chunk and frame layout
of the reported image with project-authored pixels. The reported input is
identified by SHA-256 in `SPECIFICATION.md`; it is not a test dependency.
`apng/issue-2-color-timing.png` combines adjacent opaque colors with a
five-second first-frame delay and is also project-authored.
`apng/16bit-srgb-midtones.png` has 16-bit midtone samples without `gAMA` or
`sRGB`; it detects unwanted lightening during frame decode.
The `palette-*.png` fixtures cover indexed colors, `tRNS` alpha, palette
chunks after the first frame control, opaque palette entries, and a default
image that is only a static fallback.

`animated/tiny.jxl` is `jxl/spline_on_first_frame.jxl` from pinned libjxl
testdata revision `873045a9c42ed60721756e26e2a6b32e17415205`, SHA-256
`70f753c0de4ccc28859b3aa5c6810030784c1b6989831c81b9f72d4ca9776193`.
It covers a JPEG XL input whose basic information does not signal animation.
`animated/anim-icos.jxl` is the sample linked from issue #3, downloaded from
`https://jpegxl.info/images/anim-icos.jxl`, SHA-256
`4a4c545a478e1fbebf674ee465be0afd498f94501564f9b71d9f745f960da1fe`.
It covers animated JPEG XL routing, frame decoding, and static preference offline.
`animated/issue-4-alpha.avif` is the sample linked from issue #4, downloaded
from `https://naradesign.github.io/img/animated-avif.avif`, SHA-256
`424c54cfb9c721efb423df8257817cecab6306436c3b25bf4a0c5d6e045088`.
Its monochrome alpha track uses a `pict` handler and an `auxl` reference to the
visual track. The regression test checks all decoded WebP frames and the
static first-frame output offline.
The sample is distributed under the source repository's MIT license; its
copyright and full license notice are in `animated/issue-4-LICENSE`.

The checked-in `apng/manifest.json` records each input SHA-256, chunk-scan
classification, input loop count, and full-canvas RGBA SHA-256 before and after
disposal for every emitted callback. Malformed inputs record the parser error
category they are intended to exercise.

The generator validates normal PNG chunk CRCs and APNG sequence numbers before
writing the manifest. Regenerate from the repository root with:

```sh
node tests/fixtures/media/generate-apng-fixtures.mjs
```

Generated files remain checked in so normal tests do not depend on running the
generator.
