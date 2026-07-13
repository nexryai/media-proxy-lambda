# Media fixtures

The APNG inputs under `apng/` are generated entirely from the pixel matrices in
`generate-apng-fixtures.mjs`. They are project-authored MIT-licensed fixtures;
no external image or historical implementation is required.

The checked-in `apng/manifest.json` records each input SHA-256, fixed-offset
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
