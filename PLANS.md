# MediaProxy Lambda C++ Implementation Plan

## Document status

- Status: revised for self-contained compatibility, corrected APNG blending,
  and musl static-PIE dynamic metadata
- Normative behavior: `SPECIFICATION.md`
- Target: AWS Lambda custom runtime with Function URL `RESPONSE_STREAM`
- Artifact: one statically linked musl C++ `bootstrap` binary
- Build: CMake superbuild using LLVM/Clang and LLD

This repository must remain buildable, testable, and understandable without a
legacy implementation checkout. All public URL and image behavior is captured
in `SPECIFICATION.md`; all normal fixtures and expected results will be stored
under `tests/`.

## Goal

Deliver a C++ Lambda MediaProxy that implements the complete local
compatibility specification, including exact HTTP errors, query precedence,
URL/SSRF rules, MIME detection, resize/encode behavior, and retained edge
cases. Correct APNG `BLEND_OP_OVER` composition using a full-canvas state
machine while leaving unrelated compatibility behavior unchanged.

The release is complete only when the artifact has no dynamic runtime
dependency and all offline compatibility, security, media, APNG, and Runtime
API tests pass.

## Non-goals

- Porting Redis, a work queue, worker concurrency, UUID cache files, or a cache
  lifecycle manager.
- Depending on any temporary source directory or downloaded legacy project.
- Adding new routes, query options, formats, content negotiation, or signing.
- Fixing retained compatibility quirks other than the approved APNG blend bug.
- Using Lambda Web Adapter, a sidecar server, managed runtime, or buffered
  Lambda response.
- Depending on shared objects from a Lambda base image or layer.

## Target architecture

```text
client / CDN
     |
     | HTTPS Function URL (InvokeMode: RESPONSE_STREAM)
     v
AWS Lambda Runtime API
     |
     v
static C++ bootstrap (one request per invocation)
     |
     +--> raw event/path/query parser --> URL + DNS/SSRF policy
     |                                      |
     |                                      v
     |                          pinned-address HTTPS fetch
     |                          curl + BoringSSL, <= 10 MiB
     |                                      |
     |                                      v
     |                           compatibility MIME sniffer
     |                                      |
     |                                      v
     |             libvips + codecs + APNG full-canvas compositor
     |                                      |
     +<------------- completed encoded result ----------------+
     |
     v
chunked Runtime API metadata + 8 NUL bytes + raw response chunks
```

There is no application cache. The specified cache headers allow a CDN or
caller to cache successful media. Conversion completes before response headers
are emitted so late failures can still return the specified HTTP error. The
completed body is then sent through `RESPONSE_STREAM`, avoiding the buffered
response path and its size limit.

Keep Function URL streaming outside a VPC. Private networking requires a
separate ingress design that invokes Lambda through a response-streaming-capable
API.

## Approved APNG behavior change

The former APNG-to-WebP approach did not retain a correct full composition
canvas, so a frame with `BLEND_OP_OVER` could lose pixels from earlier frames.
The revised implementation follows the core pattern demonstrated by
`watercolor/blob/main/anim.go`: copy the prior RGBA canvas and composite the
current frame over it at the APNG offsets.

The implementation must use the complete state machine in specification
section 8:

1. Snapshot the canvas before each emitted frame.
2. Apply `SOURCE` by replacing only the frame rectangle, or `OVER` by
   source-over compositing at its offsets.
3. Encode the displayed full canvas after exactly one resize.
4. Apply `NONE`, rectangle-scoped `BACKGROUND`, or exact `PREVIOUS` restoration
   to prepare the next frame.

The plan intentionally improves on incomplete disposal handling: background
clears only the current frame rectangle, and previous restores the pre-frame
snapshot. The external file is design provenance only; the local specification
contains every required operation.

Do not change these unrelated APNG behaviors while fixing blend:

- fixed-offset APNG and palette detection;
- static fallback for a palette APNG;
- non-palette APNG ignoring `static=1` and route resize limits;
- first callback used as the base and omitted from output;
- target dimensions taken from the all-pages image load;
- non-cumulative `(callback + 1) * currentDelay` timestamp rule;
- default libwebp animation options and loop behavior;
- WebP bytes potentially carrying the selector-derived AVIF response type.

## Expected dependencies

Every dependency is built as a pinned static archive through the CMake
superbuild. The final ELF has no shared-library dependency.

### Approved static-PIE dynamic metadata exception

A normal musl static PIE is `ET_DYN` and may carry a `.dynamic` section with a
matching `PT_DYNAMIC` program header so its startup code can perform
self-relocation. This link-time/startup metadata is permitted and does not
weaken the requirement that `bootstrap` have no dynamic runtime dependency.

The ELF verifier must therefore inspect semantics instead of rejecting the
section or program header by name. It permits relocation, symbol/hash,
initialization/finalization, RELRO, and PIE/immediate-binding metadata, while
rejecting `DT_NEEDED`, `DT_SONAME`, `DT_RPATH`, `DT_RUNPATH`, `DT_FILTER`,
`DT_AUXILIARY`, `DT_CONFIG`, `DT_AUDIT`, and `DT_DEPAUDIT`. `PT_INTERP`,
runtime shared objects, and loadable codecs remain forbidden. Undefined weak
references are permitted for optional static-runtime hooks because an absent
definition resolves to zero without consulting a loader; every undefined
non-weak reference remains forbidden. The verifier requires empty
`llvm-nm --undefined-only --no-weak` output, and release evidence records the
complete undefined-symbol inventory in `bootstrap.undefined-symbols.txt`. The
link map, SBOM, and minimal-filesystem release checks remain independent
evidence that these exceptions introduce no runtime shared dependency.

### Toolchain and build-only tools

- LLVM/Clang, LLD, llvm-ar/ranlib/nm/strip, compiler-rt
- musl, libc++, libc++abi, libunwind
- fortify-headers as a pinned header-only overlay providing level-3 dynamic
  object-size checks that the pinned musl headers do not provide themselves
- CMake and Ninja
- Python, Meson, and pkgconf for libvips/GLib-family builds
- Rust/Cargo only for a pinned librsvg build if required; CMake remains the
  public/orchestrating build and target native code must use the selected LLVM
  toolchain

### Clang hardening profiles

Introduce hardening with the Phase 1 toolchain, before application components
are implemented. Define one CMake interface target as the mandatory baseline
for production and ordinary GoogleTest builds. Dedicated diagnostic presets
start from it but may disable an incompatible item only through a documented,
narrow CMake preset exception. Apply the baseline to all first-party C/C++
targets and to dependency targets where whole-program CFI
or equivalent coverage requires compatible instrumentation. Any dependency
exception must be explicit, justified, and tested; flags must never silently
disappear because a compiler probe failed.

GLib, GObject, GModule, and GIO are the single approved dependency-level CFI
exception. Their public generic callback ABI intentionally permits pervasive
function-pointer conversions which trapping Clang CFI rejects during ordinary
GObject class initialization, interface initialization, and destroy callbacks.
Build these archives without CFI instrumentation while retaining fortify,
ThinLTO, stack protection, zero initialization, hidden visibility, warning
errors, and architecture branch protection. A build-policy test must enforce
both the narrow absence of GLib CFI and the presence of every retained
hardening flag; first-party code and compatible dependencies keep trapping CFI.

The production and test baseline includes:

- `-fstack-protector-strong`, `-ftrivial-auto-var-init=zero`, and
  `-fstack-clash-protection` where the pinned Clang target implements it;
- the highest effective `_FORTIFY_SOURCE` mode supported by the pinned
  musl/Clang headers (target level 3), with compiler probes proving that the
  expected fortified checks are emitted;
- ThinLTO, hidden visibility, and Clang CFI (`-fsanitize=cfi`) with trapping and
  no dynamically loaded code or sanitizer runtime in the final static ELF;
- the production-appropriate libc++ hardening mode, initially
  `_LIBCPP_HARDENING_MODE_FAST`, with ABI/configuration consistency across the
  entire static graph;
- static PIE plus RELRO, immediate binding where applicable, a non-executable
  stack, and fatal linker warnings; and
- architecture-specific branch/control-flow protection: `-fcf-protection=full`
  on `x86_64` and `-mbranch-protection=standard` on `arm64`, validated against
  the actual Lambda execution environments.

Normal unit and integration tests retain this exact production baseline.
Dedicated test presets additionally use ASan/UBSan, MSan, TSan, LSan, fuzzing,
coverage, stronger libc++ debug checks, or other diagnostics as applicable.
Each such preset documents the minimum necessary baseline exception; for
example, MSan disables automatic variable initialization so uninitialized
reads remain observable.
These high-overhead diagnostic configurations are test-only: production must
not use ASan, MSan, TSan, HWASan, LSan, fuzz/coverage instrumentation, debug
iterators, `-O0`, or similar options that are normally unsuitable for release
and materially degrade latency, throughput, memory use, or binary size. Release
optimization must not disable the baseline stack canary, CFI, initialization,
fortification, or link/branch protections.

### Direct runtime libraries

- **BoringSSL**: sole TLS and crypto provider. Initially build without
  architecture assembly so every crypto object participates in ThinLTO and
  trapping CFI. Assembly may be enabled later only with equivalent control-flow
  protection and architecture-specific performance measurements.
- **curl**: origin HTTP and Runtime API client, with HTTP/1.1 forced for the
  Runtime API connection and HTTP/2 permitted for origins.
- **nghttp2**: curl origin HTTP/2 support.
- **yyjson**: bounded Function URL event parsing and response metadata writing.
- **libvips C API**: loaders, page handling, resize, and WebP/AVIF conversion.
- **libwebp**, including mux/demux: WebP and APNG animation assembly.

The AWS SDK and Lambda C++ Runtime Interface Client are not needed. A small
runtime loop is required to implement the streaming protocol exactly, and curl
avoids introducing another TLS provider.

### Image and support closure

The pinned graph is expected to include:

- GLib, GObject, GIO, libffi, PCRE2, libexpat;
- zlib and libpng;
- libjpeg-turbo;
- libheif, libaom, and libde265 where the specified AVIF/HEIF operations require
  them;
- nsgif or giflib, selected and pinned with libvips;
- Highway or ORC when enabled in the pinned libvips graph;
- librsvg plus Cairo, pixman, libxml2, freetype, fontconfig, harfbuzz, and any
  required Pango/fribidi closure for deterministic SVG output;
- lcms2 and libexif if enabled for the golden media graph;
- libidn2/libunistring when required by the checked-in Unicode host vectors;
- an embedded CA bundle and pinned embedded fonts/font configuration.

No separate APNG library is required. Implement the specified parser/compositor
in first-party C++ using zlib/libpng primitives and libwebp. This avoids adopting
another decoder's blend/dispose semantics. No separate ICO dependency is
planned; implement first-entry fallback using the linked PNG/JPEG codecs.

Disable ImageMagick/GraphicsMagick, PDF/PostScript loaders, OpenSlide, TIFF,
OpenEXR, JPEG XL, FFTW, video support, runtime modules, introspection tools, and
x265 unless a future specification revision explicitly requires one.

### Test-only tools

Use GoogleTest for all C++ unit tests. Fetch and build it at a pinned revision
through the CMake dependency graph, and link it only into test executables.
Register each GoogleTest suite and case with CTest through CMake's
`gtest_discover_tests()` integration.

Run the unit suite through a documented `ctest --preset <test-preset>` command
so local and CI execution use the same configuration. Also use Clang sanitizers
and libFuzzer in their dedicated presets. A local TLS origin fixture is part of
the test harness. Normal tests have no live-network or external-project
dependency.

## Work plan

### Phase 0 — Land the self-contained contract and fixture inventory

Deliverables:

- Make `SPECIFICATION.md` normative and link it from agent/build documentation.
- Remove every build/test/documentation dependency on a temporary reference
  directory.
- Add a manifest enumerating every required HTTP, URL, MIME, resize, encoder,
  ICO, and APNG vector from the specification.
- Import any one-time historical expected outputs into `tests/golden/`; retain
  only redistributable fixtures, hashes, metadata, and provenance—not the
  historical source or a runtime harness for it.
- Add APNG fixtures covering alpha `OVER`, offsets, all blend/dispose crosses,
  first-frame omission, timing, palette fallback, and content-type mismatch.
- Define a dependency/version/build-option lock before accepting encoded-byte
  hashes as stable.

Exit criteria:

- `rg` finds no required path, command, include, or test referencing a legacy
  checkout.
- Every normative section has at least one mapped test ID.
- The complete planned test suite can run offline from repository contents.

### Phase 1 — Reproducible LLVM/musl CMake superbuild

Deliverables:

- Add top-level `CMakeLists.txt`, `CMakePresets.json`, and musl toolchain files
  for Lambda `x86_64` and `arm64`.
- Add hardened production and test presets immediately, backed by the shared
  CMake hardening interface target; all later phases inherit these presets.
- Add a dependency lock and CMake `ExternalProject`/`FetchContent`
  orchestration with source hashes and offline rebuild support.
- Build compiler-rt, libc++, libc++abi, and libunwind for musl; force Clang,
  LLD, and LLVM binutils into nested CMake/Meson/Cargo builds.
- Build BoringSSL, curl, nghttp2, yyjson, libvips, and the minimal codec closure
  as static PIC/LTO-compatible archives.
- Disable loadable modules and explicitly register required libvips, GLib, and
  codec operations.
- Generate embedded CA and deterministic font/configuration data.
- Add `bootstrap` packaging, link map, SBOM, notices/source bundle, relink
  bundle, and static ELF verification targets.
- Add GoogleTest at a pinned revision as a test-only CMake dependency behind
  `BUILD_TESTING`, and register its suites with `gtest_discover_tests()`.
- Add positive flag/link inspection and negative stack-smash/CFI violation
  tests for both architectures before building application components.

Exit criteria:

- A trivial `bootstrap` is a static PIE for both architectures.
- `llvm-readelf` reports no interpreter, `DT_NEEDED`, or other forbidden
  external-object dynamic tag. A `.dynamic` section and `PT_DYNAMIC` are
  accepted only as the approved static-PIE self-relocation/startup metadata.
- `llvm-nm --undefined-only --no-weak` reports no symbols; any remaining
  undefined symbols are weak optional hooks recorded in the release evidence.
- The link map contains BoringSSL but no other TLS provider, glibc, libstdc++,
  libgcc, dynamic module, or GPL-only codec.
- A clean builder reproduces the graph from the lock without package-manager
  target libraries.
- `ctest --preset <test-preset>` discovers and runs a GoogleTest smoke suite,
  and the `bootstrap` link map contains no GoogleTest symbols.
- The Phase 1 canary proves the production baseline is present, intentional
  stack corruption and invalid indirect control flow terminate execution, and
  the release ELF contains no sanitizer runtime.

### Phase 2 — Runtime API and `RESPONSE_STREAM`

Deliverables:

- Implement the warm `/next` loop and request ID, deadline, trace, `/response`,
  and `/error` handling.
- Implement strict HTTP/1.1 chunk writing over a small musl socket transport or
  curl connect-only connection.
- Emit streaming response mode, HTTP integration metadata, the eight-NUL
  delimiter, raw payload, terminal chunk, and declared error trailers.
- Add a strict mock Runtime API with fragmented reads/writes, backpressure,
  partial writes, connection closure, and midstream failures.

Exit criteria:

- Wire-byte tests pass for successful media, specified HTTP errors, invocation
  errors, and midstream errors.
- Repeated invocations prove request state resets while immutable initialization
  is reused.

### Phase 3 — Function URL event, route, and query parser

Deliverables:

- Parse bounded payload-format 2.0 events with yyjson.
- Implement path and raw-query decoding exactly as specification section 2,
  including semicolons, malformed escapes, duplicate order, and first-value
  lookup.
- Implement selector precedence, status route, response metadata, cache
  headers, and exact error bodies.
- Add GoogleTest value-parameterized unit tests for every selector collision,
  malformed escape, duplicate-key, and response-mapping case.

Exit criteria:

- Every section-2 vector matches expected status, semantic headers, and body
  bytes.

### Phase 4 — Safe HTTPS downloader with BoringSSL

Deliverables:

- Implement specification section 3 URL syntax and the complete address/CIDR
  classifier.
- Resolve once, reject mixed safe/unsafe answers, pin the validated address,
  and retain the hostname for SNI/certificate verification.
- Build curl with BoringSSL, zlib content decoding, nghttp2, embedded CA, and
  the required IDNA behavior; disable unrelated protocols.
- Implement at most 10 fully revalidated redirects, loop detection, exact user
  agent, `Blocked-By`, status, content-length, truncation, and deadline rules.
- Isolate the trusted local Runtime API client from public origin URL policy.
- Add GoogleTest unit tests for URL syntax, address classification, redirect
  decisions, header parsing, and body-limit state transitions; run local TLS
  origin scenarios as CTest integration tests.

Exit criteria:

- All URL and forbidden-address vectors pass.
- DNS rebinding, mixed answers, unsafe redirects, invalid certificates,
  decompression limits, and body-limit cases are blocked as specified.
- No origin-controlled request can reach a forbidden address.

### Phase 5 — MIME and non-APNG media conversion

Deliverables:

- Implement the 512-byte signature priority, binary fallback, SVG override,
  and exact AVIF brand check from specification section 5.
- Initialize libvips once with concurrency/cache settings from section 7.
- Implement supported MIME classification, all-pages loading, ICO first-entry
  fallback, 5120 rejection, and static/animated resize formulas.
- Implement static/animated WebP and AVIF options exactly.
- Bound arithmetic, pages, frame memory, and decoded resources above valid
  fixture maxima.
- Add GoogleTest unit tests for MIME priority, format selection, dimension
  validation, and the pure static/animated resize calculations.

Exit criteria:

- HTTP metadata, encoded hashes, dimensions, pixels, alpha, orientation,
  profile, delay, and loop diagnostics match every non-APNG golden entry.
- Malformed corpus and ASan/UBSan/fuzz smoke runs pass without leak, hang,
  out-of-bounds access, or unbounded allocation.

### Phase 6 — Correct APNG decoder, compositor, and WebP assembler

Deliverables:

- Parse PNG/APNG chunks, CRCs, frame rectangles, delay fields, blend, and
  disposal operations into bounded first-party structures.
- Preserve fixed-offset entry/palette checks and the palette static fallback.
- Maintain full RGBA `canvas` and `previousCanvas` buffers at IHDR dimensions.
- Implement offset `SOURCE` and alpha-correct `OVER` composition before resize.
- Apply disposal after frame capture: keep, clear only frame rectangle, or
  restore the exact snapshot.
- Preserve first callback omission, target dimension source, timestamp formula,
  default WebP animation options, no loop propagation, and response-type quirk.
- Add named regression fixtures for each section-8.5 case, including
  `BLEND_OP_OVER` with partial alpha and non-zero offsets.
- Implement GoogleTest fixtures for full-canvas `SOURCE`/`OVER` composition
  and parameterized blend/dispose combinations, comparing per-frame pixel
  hashes.

Exit criteria:

- Every displayed APNG frame pixel hash matches its expected full-canvas
  composition.
- `BACKGROUND` and `PREVIOUS` state-transition tests prove the next frame starts
  from the correct canvas.
- Encoded hashes and timestamp/loop diagnostics match the approved golden
  manifest for both architectures.

### Phase 7 — End-to-end handler and deployment

Deliverables:

- Join parsing, fetch, media, and streaming components with explicit typed
  error-to-response mapping.
- Add non-sensitive structured logs and metrics for cold start, fetch, decode,
  compose, resize, encode, byte counts, and error categories.
- Add CloudFormation/SAM for custom `provided.al2023`, Function URL
  `InvokeMode: RESPONSE_STREAM`, parameterized auth, memory, timeout, ephemeral
  storage, concurrency, logs, and alarms.
- Package only `bootstrap` and notices; CA/font/configuration data is compiled
  in.
- Document optional CloudFront use without changing specified cache headers.

Exit criteria:

- A canary returns correct raw binary bodies and headers through the Function
  URL with `curl --no-buffer`.
- A body above the buffered-response limit proves the streaming path is active.
- Cold/warm, disconnect, origin-stall, deadline, and concurrency tests have
  bounded documented behavior.

### Phase 8 — Release verification and evidence

Deliverables:

- Run the dual-architecture release matrix, static checks, SBOM, license/source
  and relink bundles, and reproducibility comparison.
- Verify the Phase 1 hardening manifest and performance budget without adding
  late release-only compiler flags or weakening the tested baseline.
- Review vulnerabilities and every enabled loader/codec.
- Fuzz event/query/URL, MIME, PNG/APNG, ICO, and streaming parsers for the
  agreed duration; retain failures as regression inputs.
- Publish a report containing specification revision, fixture count, hashes,
  security exceptions, performance, maximum memory, and binary size.
- Document dependency, CA, and font refresh procedures with required golden
  review.

Exit criteria:

- Every gate in `AGENTS.md` passes.
- No unexplained difference from a checked-in expected artifact remains.
- Static-link license obligations have an approved and generated compliance
  artifact.

## Key risks and decisions

### 1. APNG composition state is easy to order incorrectly

Blend applies before display; disposal applies after display and affects the
next frame. `PREVIOUS` requires a snapshot from before drawing, while
`BACKGROUND` affects only the current frame rectangle. Full per-frame pixel
hashes and crossed blend/dispose fixtures are mandatory.

### 2. Encoded byte identity requires a pinned codec graph

libvips, libwebp, libheif, libaom, SIMD choices, fonts, and profiles can alter
bytes or pixels. The lock and golden manifest, not a distribution's current
packages, define the release graph.

### 3. SVG parity conflicts with a small binary

Deterministic SVG text can require librsvg, a large static rendering closure,
and embedded fonts. Include and measure the pinned stack unless the normative
format set is explicitly revised.

### 4. Static plugin registration can fail silently

libvips, GLib, libheif, and font systems commonly discover runtime modules.
Explicit registration and final-binary operation enumeration are release tests.

### 5. Streaming cannot change HTTP status after headers begin

The first release converts before opening the response stream. This preserves
specified failures and permits large bodies, but does not overlap encoding with
network transfer. True encoder streaming is a later separately tested change.

### 6. URL compatibility and SSRF defense can conflict

The normative security exceptions validate every redirect and reject DNS sets
containing any forbidden answer. Keep those differences explicit in fixtures.

### 7. Static LGPL distribution needs compliance output

A technically self-contained ELF can still require notices, corresponding
source, and relinking materials. Generate these with the release rather than
treating them as manual follow-up.

## Open deployment choices

- Primary performance architecture; both `arm64` and `x86_64` remain required
  until scope is narrowed.
- The exact embedded font corpus for deterministic SVG text.
- Direct public Function URL versus CloudFront fronting it.
- Memory, timeout, ephemeral storage, and reserved concurrency values after
  benchmark data exists.

These choices may affect deployment or performance but must not change the
normative request and image contract.

## Primary external references

- AWS custom runtime streaming protocol:
  <https://docs.aws.amazon.com/lambda/latest/dg/runtimes-custom.html>
- AWS Lambda response streaming constraints:
  <https://docs.aws.amazon.com/lambda/latest/dg/configuration-response-streaming.html>
- Function URL `RESPONSE_STREAM` configuration:
  <https://docs.aws.amazon.com/lambda/latest/dg/config-rs-invoke-furls.html>
- Streaming HTTP metadata and eight-NUL delimiter:
  <https://docs.aws.amazon.com/apigateway/latest/developerguide/response-transfer-mode-lambda.html>
- libvips build/dependency overview:
  <https://github.com/libvips/libvips>
- BoringSSL build system:
  <https://boringssl.googlesource.com/boringssl/>
- APNG blend implementation used as design provenance:
  <https://github.com/nexryai/watercolor/blob/main/anim.go>

External references provide context only. `SPECIFICATION.md` remains the
complete local behavioral contract.
