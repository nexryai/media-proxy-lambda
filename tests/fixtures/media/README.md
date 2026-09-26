# Media fixtures

The APNG inputs under `apng/` are generated entirely from the pixel matrices in
`generate-apng-fixtures.mjs`. They are project-authored MIT-licensed fixtures;
no external image or historical implementation is required.
`apng/issue-1-first-frame.png` recreates the relevant chunk and frame layout
of the reported image with project-authored pixels. The reported input is
identified by SHA-256 in `SPECIFICATION.md`; it is not a test dependency.
`apng/issue-2-color-timing.png` combines adjacent opaque colors with a
five-second first-frame delay and is also project-authored.
The `palette-*.png` fixtures cover indexed colors, `tRNS` alpha, palette
chunks after the first frame control, opaque palette entries, and a default
image that is only a static fallback.

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
