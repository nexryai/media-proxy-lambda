# MediaProxy Compatibility Specification

## 1. Status and scope

This document is the normative, self-contained compatibility contract for the
MediaProxy Lambda implementation. The implementation, tests, and release
process must not require a legacy source tree or a separately downloaded
reference implementation. Historical projects may be cited as provenance, but
the behavior to implement is completely stated here.

The intentional media changes in the initial C++ release are the APNG
`BLEND_OP_OVER` fix in section 8 and removal of SVG input support. All other
unrelated legacy behavior, including unusual resize decisions, fixed-offset
format checks, APNG first-frame handling, and response content-type selection,
remains part of this contract.

Where this document labels a rule as a security exception, the safer rule is
normative even if a historical implementation accepted more input.

The repository deliverable is an arm64 `bootstrap` implementing this runtime
and HTTP contract. Provisioning or updating AWS resources, deployment
automation, IAM, monitoring, budgets, and cost controls are outside this
specification and are supplied by the consumer of the artifact.

## 2. HTTP request contract

### 2.1 Lambda event input

The handler accepts an AWS Lambda Function URL payload-format 2.0 event. It
uses:

- `requestContext.http.method` as the method for logging and tests;
- `rawPath` as the encoded path source;
- `rawQueryString` as the only query source;
- the Runtime API request ID for logging and response/error submission.

Do not use `queryStringParameters` to decide behavior because that map loses
ordering, duplicate keys, and some encoding distinctions.

Decode the path once using URL path unescaping, without converting `+` to a
space. The exact decoded path `/status` is the only special path. Invalid path
escaping is a bad request. Every other path invokes the media proxy. Request
method does not change routing; GET, HEAD, POST, and other methods run the same
handler and return the same body semantics.

### 2.2 Query parsing

Parse `rawQueryString` as follows:

1. Split fields on `&`, preserving their order.
2. Skip empty fields.
3. Reject an individual field containing an unescaped semicolon; continue
   parsing the remaining fields.
4. Split each field at the first `=`. A missing `=` means an empty value.
5. Query-unescape the key and value: `+` becomes space and `%HH` decodes one
   byte. If either side contains an invalid percent escape, discard that field.
6. Append duplicate values in encounter order.
7. Lookup returns the first value for a key, or an empty string when absent.

Unknown parameters are ignored. A boolean option is true only when its first
value is exactly the single character `1`; values such as `true`, `01`, and
`1 ` are false.

### 2.3 Routes and selector precedence

`/status` returns:

- status: 200
- `Content-Type: application/json`
- body: `{"status":"OK"}` with no trailing newline

All other paths require a non-empty first `url` value. Missing or empty `url`
returns the error specified in section 2.5.

Select dimensions and preferred output using the first true row:

| Precedence | Query flag | Width limit | Height limit | Preferred output |
| ---: | --- | ---: | ---: | --- |
| 1 | `avatar=1` | 320 | 320 | AVIF |
| 2 | `emoji=1` | 700 | 128 | AVIF |
| 3 | `preview=1` | 200 | 200 | WebP |
| 4 | `badge=1` | 96 | 96 | AVIF |
| 5 | `thumbnail=1` | 500 | 400 | WebP |
| 6 | `ticker=1` | 64 | 64 | AVIF |
| 7 | none | 3200 | 3200 | WebP |

`static=1` is evaluated independently after selector choice.

### 2.4 Successful media response

A successful media response has status 200 and these headers:

- `Content-Type: image/avif` when the selected preferred output is AVIF;
  otherwise `image/webp`.
- `CDN-Cache-Control: max-age=604800`
- `Cache-Control: max-age=432000`

The `Content-Type` is derived from the request selector, not by inspecting the
encoded bytes. Therefore an animated input selected through `avatar`, `emoji`,
`badge`, or `ticker` can have a WebP body labeled `image/avif`. This mismatch
is compatibility behavior and must have a regression test.

### 2.5 Error responses

| Condition | Status | Body |
| --- | ---: | --- |
| Empty/missing URL or invalid request event/path structure | 400 | `Bad request\n` |
| URL rejected before fetch | 403 | `Access denied\n` |
| Unsupported MIME, decode failure, dimension rejection, resize failure, or encode failure | 400 | `Failed to resize image: invalid image?\n` |
| DNS, connect, TLS, redirect, download, body-length, origin non-200, or internal failure | 500 | `Internal Server Error\n` |

All error responses use `Content-Type: text/plain; charset=utf-8` and do not
include successful media cache headers.

## 3. Target URL policy

### 3.1 Syntax acceptance

The first decoded `url` query value is accepted only when all conditions hold:

- The scheme is exactly lowercase `https`.
- The URL has no user name or password.
- `hostname` is non-empty, contains at least one `.`, and contains no `:`.
  This rejects single-label names and all literal IPv6 spellings.
- The explicit port is absent, `80`, or `443`. HTTPS on explicit port 80 is
  intentionally accepted.
- Parsing consumes a syntactically valid URL. Fragments may be parsed but are
  not sent in the HTTP request.
- If the hostname itself is an IP literal, it passes section 3.2.

Convert the parsed hostname to its canonical ASCII form before applying the
dot, IP-literal, DNS, redirect, SNI, or certificate-hostname rules. Preserve
that canonical hostname across resolution and connection pinning.

IDNA conversion uses Unicode 17.0.0 UTS #46 nontransitional processing with
STD3 ASCII rules, hyphen checks, Bidi checks, ContextJ checks, and DNS-length
verification enabled. The pinned implementation performs mapping, NFC
normalization, validity checks, and Punycode conversion. The MediaProxy boundary
must independently validate its result because the low-level conversion API
does not promise to return a syntactically valid DNS name:

- Do not apply an additional Unicode script or identifier-category allowlist.
  UTS #46-valid symbols, including emoji, are encoded as A-labels; network
  safety is enforced on the canonical DNS name and every resolved address.
- Reject an empty input and reject a decoded UTF-8 hostname longer than 4096
  bytes before conversion.
- Reject invalid UTF-8, a failed conversion, forbidden domain code points,
  empty labels, and ASCII output containing anything other than lowercase
  letters, digits, hyphen-minus, and label separators.
- Reject a label longer than 63 bytes or beginning or ending with a hyphen.
  Reject hyphens in both the third and fourth positions except for a valid
  `xn--` A-label that the IDNA implementation has decoded and validated.
- Reject a canonical hostname longer than 253 bytes. A single trailing root
  dot is retained and permits a total textual length of 254 bytes; leading or
  interior empty labels remain invalid.

The complete accepted/rejected IDNA corpus, including normalization, dot
mapping, Bidi, ContextJ, invalid UTF-8, STD3, hyphen, label-length, and
hostname-length cases, is checked into this repository. Changing the Unicode
version or any profile option requires review of that corpus.

### 3.2 Forbidden network addresses

Address parse failure is unsafe. Reject an address when any condition holds:

- the original textual address begins with `::ffff:` or `::ffff:0:`;
- it is loopback, private, multicast, link-local unicast, unspecified, or not
  global unicast;
- it belongs to any explicit deny range below.

| CIDR | Reason |
| --- | --- |
| `0.0.0.0/8` | current network / non-routable |
| `100.64.0.0/10` | shared address space / carrier-grade NAT |
| `64:ff9b::/96` | IPv4/IPv6 translation |
| `64:ff9b:1::/48` | local-use IPv4/IPv6 translation |
| `2001:10::/28` | deprecated ORCHID space |
| `2001:db8::/32` | documentation space |
| `::/96` | IPv4-compatible/reserved IPv6 space |

The generic classifications also reject, among others, `127.0.0.0/8`,
`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `169.254.0.0/16`,
`224.0.0.0/4`, `255.255.255.255`, `::1`, `fe80::/10`, `fc00::/7`, and
`ff00::/8`.

### 3.3 DNS, redirects, and connection pinning

Resolve the hostname before connecting. Validate every candidate address and
connect only to a validated address while retaining the logical hostname for
SNI and certificate checks. Do not resolve a second time inside the connection
library. A DNS response containing any forbidden address is rejected rather
than relying on address order.

Redirects are a security exception to legacy behavior:

- Treat status 301, 302, 303, 307, and 308 as redirects only when the response
  supplies a non-empty `Location` field. Every other non-200 status is a
  download failure.
- Follow at most 10 redirects.
- Resolve relative `Location` values against the current URL.
- Apply the complete syntax and address policy to every hop.
- Strip URL user information and reject a redirect containing it.
- Detect loops.
- Never forward caller credentials or origin authorization headers.

## 4. Origin download contract

- Method: GET.
- User agent: `Misskey-Media-Proxy-Go v0.10`.
- TLS: BoringSSL with certificate-chain and hostname verification enabled,
  using the embedded CA bundle.
- HTTP versions: allow HTTP/1.1 and HTTP/2 negotiation; do not require HTTP/3.
- Content coding: advertise/accept gzip behavior equivalent to automatic
  transparent decompression and sniff the decompressed bytes.
- Maximum retained decompressed body: 10 * 1024 * 1024 bytes.
- If `Content-Length` exists, parse it as signed base-10 64-bit. Parse failure
  or a value above the limit is a download failure.
- With an absent length, read at most the limit. Historical behavior does not
  read an extra byte to distinguish an exact-size body from a longer chunked
  body; decoding therefore sees the first 10 MiB. Preserve this behavior for
  compatibility while never retaining additional bytes.
- A response header exactly equal to `Blocked-By: NextDNS` is a download
  failure.
- Read and sniff the body and apply the header and body safety checks before
  evaluating each hop's status. Redirect statuses are handled by section 3.3;
  after redirect processing, only status 200 succeeds.
- Bound connect, transfer, and total time by the remaining Lambda invocation
  deadline after subtracting exactly 1,000 milliseconds to submit a response.
  Refuse to begin or continue an origin hop when that remaining budget is zero.
  There is no independent legacy wall-clock timeout to preserve.

## 5. MIME sniffing

Inspect at most the first 512 body bytes. Signature order matters. Matching is
case-sensitive unless noted.

### 5.1 Relevant signatures

| Bytes/condition | MIME |
| --- | --- |
| `00 00 01 00` | `image/x-icon` |
| `BM` | `image/bmp` |
| `GIF87a` or `GIF89a` | `image/gif` |
| `RIFF` + four wildcard bytes + `WEBPVP` | `image/webp` |
| `89 50 4e 47 0d 0a 1a 0a` | `image/png` |
| `ff d8 ff` | `image/jpeg` |
| `%PDF-` | `application/pdf` |
| `%!PS-Adobe-` | `application/postscript` |
| `ID3` | `audio/mpeg` |
| `OggS 00` | `application/ogg` |
| `1a 45 df a3` | `video/webm` |
| `RIFF` + four wildcard bytes + `AVI ` | `video/avi` |
| `RIFF` + four wildcard bytes + `WAVE` | `audio/wave` |
| `PK 03 04` | `application/zip` |
| `1f 8b 08` | `application/x-gzip` |
| `00 61 73 6d` | `application/wasm` |

Before binary fallback, recognize the standard HTML leading tags after
skipping ASCII whitespace, `<?xml` as `text/xml; charset=utf-8`, Unicode BOMs
as their corresponding `text/plain` charset, common MP4 `ftyp` brands, and
standard TrueType/OpenType/WOFF/EOT font signatures. These signatures must be
captured in unit vectors so non-image input remains rejected consistently.

If no signature matches, return `text/plain; charset=utf-8` when the sample has
no binary control byte; otherwise return `application/octet-stream`. Binary
control bytes are `0x00..0x08`, `0x0b`, `0x0e..0x1a`, and `0x1c..0x1f`.

### 5.2 Overrides

- SVG has no origin `Content-Type` override. SVG-looking text remains
  `text/plain; charset=utf-8`, including when the origin value is exactly
  `image/svg+xml`, and is rejected as unsupported media.
- If sniffing returns `application/octet-stream` and bytes 4 through 11 are
  exactly `ftypavif`, use `image/avif`.
- No analogous override exists for other AVIF brands or HEIF. HEIF/HEIC is
  intentionally unsupported; the pinned media graph provides AVIF through
  libheif's built-in libaom backend without any HEVC decoder or encoder.

## 6. Media classification and output selection

Convertible MIME values are exactly:

- `image/avif`
- `image/ico`
- `image/jpeg`
- `image/png`
- `image/webp`
- `image/gif`
- `image/x-icon`

Every other MIME is a conversion failure.

Animation classification before decode is:

- GIF: animated unless `static=1`.
- WebP: animated only when bytes `0x1e..0x21` are exactly `ANIM`, and only
  unless `static=1`.
- Other MIME types: not animated at this stage.

An animated GIF or WebP forces the encoder to WebP. A non-animated request uses
AVIF only when its selector prefers AVIF; otherwise it uses WebP. Animated
output remains WebP even when the successful response header is selected as
`image/avif` under section 2.4.

Load images from the in-memory body with all pages enabled. No loader may make
network requests or read arbitrary external files. No SVG loader or rendering
stack is present.

## 7. General image conversion

### 7.1 Decoder fallback and dimensions

Use libvips with execution-environment initialization equivalent to:

- concurrency: 1
- maximum cache memory: 8 MiB
- maximum cache entries: 32
- maximum cached files: 32
- all pages requested on load

If an ICO/x-icon cannot be loaded by libvips, decode its directory entries,
choose the first successfully decoded image in file order, and pass that
decoded image directly to the remaining libvips pipeline. The source request
bytes remain alive until conversion completes; do not create an intermediate
PNG or a full-image copy solely to extend their lifetime. Do not choose the
largest icon.

For a static image, width and height are libvips image dimensions. For an
animated image, width is the loaded width and per-frame height is loaded height
divided by page count. Zero pages or non-integral/invalid dimensions fail.

Except for the non-palette APNG early-return path in section 8, accept width up
to 7680 and per-frame height up to 4320 so a landscape 16:9 8K UHD image is
convertible. Reject a larger width or per-frame height. No explicit
decoded-pixel limit may alter valid golden cases; security limits must be set
above the maximum valid corpus and documented.

### 7.2 Static resize algorithm

Let `w`, `h`, `W`, and `H` be source width, source height, width limit, and
height limit.

1. Resize only when `w > W || h > H`.
2. Compute `widthExcess = w - W` and `heightExcess = h - H`.
3. If `widthExcess < heightExcess`, set `scale = H / h`; otherwise set
   `scale = W / w`. Use floating-point division.
4. Call libvips resize once with that scale and `VIPS_KERNEL_AUTO`.

This compares absolute excess rather than ratios and can produce a result that
does not fit one limit. Preserve it. Never upscale when step 1 is false.

### 7.3 Animated GIF/WebP resize algorithm

Initialize `newWidth=w`, `newHeight=h`. If `w > W || h > H`:

1. `aspect = w / h` as floating point.
2. Compute the same absolute excess values.
3. If both dimensions exceed their non-zero limits, set `W=0` when
   `widthExcess < heightExcess`; otherwise set `H=0`.
4. If `W != 0` and `w > W`, set `newWidth=W` and
   `newHeight=round(newWidth/aspect)`.
5. Else, only when `W == 0`, if `H != 0 && h > H`, set `newHeight=H` and
   `newWidth=round(newHeight*aspect)`.
6. Call libvips thumbnail with `newWidth`, `newHeight`,
   `VIPS_INTERESTING_ALL`, and `VIPS_SIZE_DOWN`.

The step-5 dependency on `W == 0` is intentional: a height-only overflow can
remain unresized when the width limit is non-zero.

### 7.4 Encoding

- Static WebP: quality 70, lossy.
- Animated GIF/WebP conversion: animated WebP quality 70, lossy, preserving
  the pages/timing behavior produced by the pinned libvips/codec graph.
- AVIF: quality 65, effort 1, lossy.
- A static request for an animation encodes only the first decoded page.

Pin libvips, libwebp, libheif, libaom, and every decoding/resampling dependency.
Build libheif with only its built-in libaom AV1 decoder and encoder. Disable
libde265, x265, every other HEVC backend, and plugin loading. An HEIF/HEIC input
is not convertible even if libheif can parse generic ISO base-media metadata.
Encoded bytes, metadata, orientation, profiles, alpha, dimensions, frame pixels,
frame delays, and loop behavior are all compatibility surfaces.

## 8. APNG conversion with corrected `BLEND_OP_OVER`

### 8.1 Entry conditions and retained quirks

An input is treated as APNG only when:

- it starts with the PNG signature; and
- total length is greater than 41; and
- bytes 37 through 40 are exactly `acTL`.

Palette use is detected only when total length is greater than 64 and bytes 57
through 60 are exactly `PLTE`. Do not replace either fixed-offset check with a
general chunk scan in the compatibility path.

A palette APNG is converted as a static image through section 7, so
`static=1` and AVIF preference take effect normally. A non-palette APNG always
uses the APNG-to-animated-WebP path, even with `static=1`, ignores route resize
limits, forces WebP encoding, and returns before the general dimension check.

The APNG target width and height are the width and height reported by the
all-pages libvips load at the point the APNG branch is entered. The APNG IHDR
width and height are the composition-canvas dimensions. A zero dimension fails.

### 8.2 Frame input

Parse `acTL`, `fcTL`, `IDAT`, and `fdAT` with CRC and bounds validation. For
each decoded callback obtain:

- zero-based callback number;
- RGBA frame pixels and frame width/height;
- x/y offset;
- delay numerator and denominator;
- `blend_op` (`SOURCE=0`, `OVER=1`);
- `dispose_op` (`NONE=0`, `BACKGROUND=1`, `PREVIOUS=2`).

Reject a frame rectangle outside the IHDR canvas and reject unknown operation
values. Preserve the legacy delay callback behavior; a zero denominator is a
conversion failure rather than silently normalizing it.

The callback numbered zero initializes the base canvas and is not emitted to
the animated WebP. Its pixels form the starting canvas exactly as decoded; its
dispose operation is not applied. This first-frame omission is intentionally
retained.

### 8.3 Correct composition state machine

Maintain one full-size, straight-alpha RGBA canvas and a saved pre-frame copy.
For every callback `n >= 1`, perform these steps in order:

1. Copy the current canvas to `previousCanvas` before drawing.
2. For `BLEND_OP_SOURCE`, clear only the frame rectangle to transparent, then
   source-copy the decoded frame into that rectangle at `(xOffset,yOffset)`.
   Pixels outside the rectangle remain unchanged.
3. For `BLEND_OP_OVER`, copy the current canvas and alpha-composite the decoded
   frame over it at `(xOffset,yOffset)` using source-over semantics equivalent
   to Go `image/draw.Over`. Transparent source pixels reveal the prior canvas;
   partial alpha combines source and destination rather than replacing them.
4. The resulting full canvas is the displayed frame. Import the entire IHDR
   canvas into `WebPPicture`, then call `WebPPictureRescale` exactly once to the
   APNG target dimensions. Offsets are applied before scaling.
5. Add the displayed frame to `WebPAnimEncoder`.
6. Prepare the canvas for the next callback according to the current frame's
   disposal:
   - `DISPOSE_OP_NONE`: keep the displayed canvas.
   - `DISPOSE_OP_BACKGROUND`: clear only the current frame rectangle to
     transparent; keep pixels outside it.
   - `DISPOSE_OP_PREVIOUS`: restore `previousCanvas` exactly.

This is the intentional correction. The older conversion path encoded frame
rectangles without a persistent composition canvas, so `BLEND_OP_OVER` lost
prior-frame pixels. The full-canvas copy plus source-over pattern is informed
by `watercolor`, while the rectangle-scoped background disposal and explicit
pre-frame restoration above define the complete normative behavior.

### 8.4 APNG WebP timing and options

Initialize `WebPAnimEncoderOptions` with libwebp defaults and create the encoder
at the APNG target dimensions. Do not copy the APNG loop count; retain the
libwebp default animation options.

For callback number `n >= 1`, compute its timestamp as:

```text
timestamp_ms = trunc_toward_zero(float32(n + 1) * delay_seconds * 1000)
```

This is not a cumulative sum and is intentionally retained. Add frames with a
null/default per-frame `WebPConfig`, do not add a synthetic terminal frame, and
assemble with `WebPAnimEncoderAssemble`.

### 8.5 Required APNG tests

The fixture matrix must include:

- opaque and partial-alpha `OVER` at zero and non-zero offsets;
- transparent holes revealing two or more prior frames;
- `SOURCE` after `OVER` and `OVER` after `SOURCE`;
- every blend operation crossed with `NONE`, `BACKGROUND`, and `PREVIOUS`;
- background disposal proving only the frame rectangle is cleared;
- previous disposal proving exact pre-frame restoration;
- frame rectangles touching each canvas edge and invalid out-of-bounds frames;
- the retained first-callback omission and non-cumulative timestamp rule;
- palette APNG static fallback;
- `static=1` on non-palette APNG still producing animated WebP;
- an AVIF-preferring selector producing WebP bytes with the compatibility
  response content type;
- varying delays, zero denominator failure, loop-count non-propagation, and
  deterministic output hashes.

## 9. Lambda response streaming

Complete validation, fetch, and conversion before starting the external
response so an image failure can still use section 2.5. Then POST to:

`/2018-06-01/runtime/invocation/{request-id}/response`

with:

- `Lambda-Runtime-Function-Response-Mode: streaming`
- `Transfer-Encoding: chunked`
- `Content-Type: application/vnd.awslambda.http-integration-response`
- a `Trailer` declaration for
  `Lambda-Runtime-Function-Error-Type` and
  `Lambda-Runtime-Function-Error-Body`

The chunked entity consists of valid JSON metadata containing `statusCode` and
single-value `headers`, exactly eight NUL bytes, then raw response bytes. Image
bytes are never base64 encoded. End with a valid terminal chunk and close the
underlying connection. A failure after streaming begins uses the declared
trailers, with the error body base64 encoded as required by the Runtime API.

## 10. Runtime artifact contract

The produced arm64 `bootstrap` is a statically linked musl PIE. Its ELF type is
`ET_DYN`, it has no `PT_INTERP` program header, and it has no runtime shared
library or loadable-module dependency.

A `.dynamic` section and matching `PT_DYNAMIC` program header are permitted as
a narrow exception when the pinned musl/LLD static-PIE link uses them for
self-relocation and process-startup metadata. Relocation, symbol/hash,
initialization/finalization, RELRO, and PIE/immediate-binding flag entries do
not by themselves constitute a runtime shared-library dependency. The section
and program header are not required when a future pinned toolchain can produce
an otherwise equivalent static PIE without them.

The dynamic table must contain none of these external-object tags:

- `DT_NEEDED`, `DT_SONAME`, `DT_RPATH`, or `DT_RUNPATH`;
- `DT_FILTER` or `DT_AUXILIARY`;
- `DT_CONFIG`, `DT_AUDIT`, or `DT_DEPAUDIT`.

The release verifier must inspect the dynamic-table entries rather than reject
the `.dynamic` section or `PT_DYNAMIC` program header by name. It must still
reject an ELF interpreter, any forbidden dynamic tag, or an executable stack.

Undefined weak references are permitted for optional static-runtime hooks.
When no definition is linked, ELF weak-symbol semantics resolve such a
reference to zero; it does not cause a loader or shared object to be consulted.
Every undefined non-weak reference remains forbidden. The verifier must run
`llvm-nm --undefined-only --no-weak` and require empty output, while release
evidence retains the complete undefined-symbol inventory as
`bootstrap.undefined-symbols.txt` so weak references remain reviewable.

Link-map, SBOM, and minimal-filesystem checks must prove that no dynamic loader,
shared object, or loadable codec is required at runtime.

## 11. Test and compatibility artifacts

All normal tests are offline and self-contained. Check in:

- raw Function URL event fixtures and expected metadata/body outputs;
- URL/query acceptance vectors and forbidden-address vectors;
- a local TLS origin test PKI and deterministic origin-response scripts;
- input images with redistribution metadata;
- expected encoded-body SHA-256, decoded dimensions, per-frame pixel hashes,
  delays, loop behavior, alpha, orientation, profile, and response headers;
- explicit security exceptions, currently redirect revalidation and rejection
  of mixed safe/unsafe DNS answers;
- the exact dependency/version/build-option manifest that produced image
  golden files.

Generation tools may be used once to import historical observations, but the
checked-in suite must not clone, import, execute, or inspect a legacy project.
Changing a normative value requires updating this document and reviewing the
corresponding fixture diff in the same change.

## 12. Design provenance (non-normative)

The corrected full-canvas `BLEND_OP_OVER` approach was informed by:

- `watercolor/blob/main/anim.go` APNG animation code:
  <https://github.com/nexryai/watercolor/blob/main/anim.go>

This link explains the design history only. Availability or contents of that
repository are not required to build, test, or understand this project; the
complete required algorithm is section 8.
