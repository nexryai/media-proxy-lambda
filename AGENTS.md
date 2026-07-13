# AGENTS.md

## Purpose

This repository implements a C++ MediaProxy as an AWS Lambda custom runtime.
The complete public HTTP, URL-safety, download, MIME, image-conversion, APNG,
and response-streaming contract is defined in `SPECIFICATION.md`. Implementation
work must remain possible with only this repository; no legacy source checkout
or downloaded reference project is a build, test, or documentation dependency.

Read `SPECIFICATION.md` and `PLANS.md` before changing behavior, architecture,
or dependencies. Update the specification, tests, and plan together whenever a
normative rule or milestone changes.

## Development environment and missing packages

Treat `.devcontainer/Dockerfile` and `.devcontainer/devcontainer.json` as the
authoritative inventory and configuration for host development tools. Before
using a compiler, linker, build generator, package-config tool, language
runtime, or other host command, verify that it is declared by the devcontainer
and available in the active environment.

Do not install a missing host package, switch to an undeclared substitute, or
silently weaken a build/test workflow. If implementation or validation needs a
host package that is absent from either the devcontainer definition or the
active environment, stop work and notify the user. Identify the missing
package/command and the task that requires it, then wait for the environment to
be updated before continuing. Pinned target libraries built by the CMake
superbuild remain governed by the dependency lock rather than this host-package
rule.

## Sources of truth

Use this precedence when behavior is ambiguous:

1. `SPECIFICATION.md`.
2. Checked-in compatibility vectors, image fixtures, and golden manifests.
3. `PLANS.md` for sequencing and architectural decisions.
4. Upstream protocol/format documentation only where the local specification
   is explicitly silent.

Historical repositories cited under design provenance are non-normative. Do
not clone or inspect one during normal development or tests. If a historical
observation matters, state it completely in `SPECIFICATION.md` and add a local
test vector.

Do not silently replace an unusual specified behavior with a conventional one.
First add a regression test, update the specification with the proposed
divergence, and obtain approval. Security exceptions must be explicit and
covered by tests.

## Non-negotiable build invariants

- Production code is C++20 or newer.
- CMake is the public build interface. Dependencies using Meson or Cargo may be
  driven by a CMake superbuild, but configure, build, test, package, and verify
  workflows must be exposed through CMake presets and CTest.
- Target compilation and linking use Clang, LLD, `llvm-ar`, `llvm-ranlib`,
  `llvm-nm`, and `llvm-strip`.
- Link C++ with musl, static libc++, libc++abi, libunwind, and compiler-rt. Do
  not link glibc, libstdc++, libgcc, or GCC runtime libraries.
- BoringSSL is the sole TLS/cryptography implementation. Do not introduce
  OpenSSL, LibreSSL, an AWS SDK TLS stack, or a second `libcrypto` provider.
- The deployment artifact is a statically linked PIE named `bootstrap`. It has
  no ELF interpreter, `DT_NEEDED` entry, loadable codec, or runtime shared
  library dependency.
- Embed CA roots and all required font/configuration data at build time.
- Pin every third-party revision, archive hash, build option, and enabled codec.
  Never use a rolling distribution package or unpinned branch for a release.
- Use a custom Lambda runtime and Function URL `RESPONSE_STREAM`. Do not replace
  it with a buffered response, Lambda Web Adapter, sidecar HTTP server, or a
  managed language runtime.
- Keep a Function URL streaming deployment outside a VPC. A VPC deployment
  requires a separately approved ingress design using a streaming-capable
  invocation path.

## Serverless architecture boundary

Each invocation performs one inline pipeline:

1. Poll the Lambda Runtime API.
2. Parse the Function URL event's raw path and query.
3. Apply the specified route, query, URL, DNS, and SSRF rules.
4. Fetch at most 10 MiB over verified HTTPS.
5. Sniff, decode, compose, resize, and encode the media.
6. After successful conversion, send HTTP integration metadata and body chunks
   through the Runtime API response stream.

There is no Redis, work queue, UUID cache file, cache-purge loop, or persistent
application cache. Preserve the specified response cache headers so a caller or
CDN may cache results. `/tmp` is bounded invocation-local scratch space only;
it is never a correctness dependency or cross-invocation cache.

Initialize immutable BoringSSL, curl, libvips, codec, CA, and font state once
per execution environment. Reset all request state after every invocation. A
normal Lambda environment processes one invocation at a time.

Do not start the external response until request validation, origin fetch, and
encoding have succeeded. The completed result is still transmitted with
`RESPONSE_STREAM`; direct encoder-to-network streaming is a later optimization
requiring identical output and tested midstream-error behavior.

## Compatibility discipline

`SPECIFICATION.md` is deliberately exhaustive. In particular, do not
reconstruct these rules from a framework's defaults:

- raw query parsing, duplicate-key order, percent decoding, and selector
  precedence;
- exact URL syntax, forbidden CIDRs, DNS pinning, redirect validation, and
  origin-body limits;
- MIME-sniff order and the exact SVG and AVIF overrides;
- separate static and animated resize formulas, including their odd branches;
- response content type being selected independently from actual animation
  bytes;
- fixed-offset APNG and palette checks;
- first APNG callback omission and non-cumulative timestamp calculation.

### APNG `BLEND_OP_OVER` correction

The APNG compositor is the one intentional initial media correction. It must
maintain a full RGBA canvas and, for every emitted frame:

- snapshot the pre-frame canvas;
- source-copy `BLEND_OP_SOURCE` or source-over composite `BLEND_OP_OVER` at the
  frame offsets;
- encode the displayed full canvas after a single resize;
- keep it for `DISPOSE_OP_NONE`, clear only the frame rectangle for
  `DISPOSE_OP_BACKGROUND`, or restore the snapshot for
  `DISPOSE_OP_PREVIOUS`.

The prior-canvas copy plus offset source-over approach is informed by
`watercolor/blob/main/anim.go`, but section 8 of `SPECIFICATION.md` is the
complete normative algorithm. Do not depend on that external file being
available. Do not accidentally change the retained first-frame, palette,
static-flag, target-dimension, timing, loop, or content-type behavior while
fixing blend composition.

## Lambda streaming protocol

The custom runtime must:

- read `AWS_LAMBDA_RUNTIME_API` and poll
  `/2018-06-01/runtime/invocation/next`;
- preserve the invocation request ID and report unrecoverable pre-response
  failures to the corresponding `/error` endpoint;
- POST successful invocation responses with
  `Lambda-Runtime-Function-Response-Mode: streaming`,
  `Transfer-Encoding: chunked`, and
  `Content-Type: application/vnd.awslambda.http-integration-response`;
- write valid HTTP integration metadata, exactly eight NUL bytes, then raw body
  bytes without base64 encoding;
- declare the Lambda runtime error trailers before streaming and use them for
  failures after the response begins;
- write the terminal chunk/trailers and close the underlying connection.

Test the wire protocol against a strict local mock and then an AWS canary.
Lambda Runtime Interface Emulator alone is not proof of Function URL streaming
behavior.

## Project layout

Prefer this structure unless `PLANS.md` records an approved change:

- `cmake/` — LLVM/musl toolchains, dependency superbuild, static checks.
- `include/mediaproxy/`, `src/` — production C++.
- `src/runtime/` — Runtime API loop and streaming writer.
- `src/http/` — event/query parsing, URL policy, DNS, and HTTP client.
- `src/media/` — MIME, APNG/ICO compatibility, and libvips pipeline.
- `tests/` — unit, golden, malformed-input, fuzz, and protocol tests.
- `tests/fixtures/` — redistributable offline events, TLS data, and media.
- `infra/` — Lambda and Function URL deployment with
  `InvokeMode: RESPONSE_STREAM`.

Do not add a normal-test dependency on live internet resources. All URL and
origin behavior must be reproducible with the checked-in local TLS fixture
system.

## Coding rules

Use RAII for BoringSSL, curl, libvips, GLib, file descriptors, and Runtime API
resources. Use bounded containers and checked arithmetic for byte lengths,
dimensions, offsets, rectangles, pages, frame counts, timestamps, and chunk
sizes. Exceptions must not cross C callbacks. Avoid mutable process-global
request state.

For APNG, reject invalid rectangles and unknown blend/dispose values before
touching the canvas. Make canvas snapshots explicit so disposal ordering is
reviewable. Tests must compare full per-frame pixel hashes, not only successful
decode or final dimensions.

Log request IDs, timings, sizes, and error categories. Do not log credentials,
full query strings, or complete source URLs at normal log levels.

## Commit discipline

Create commits at small, coherent boundaries throughout implementation. Each
commit should represent one reviewable concern—for example a specification
change, one dependency integration, one runtime component, one media operation,
or its focused tests—rather than accumulating unrelated work into a large
checkpoint.

Before committing:

- inspect the complete staged diff and exclude unrelated user changes;
- stage explicit paths or hunks instead of relying on a blanket add in a mixed
  worktree;
- run the smallest relevant validation for that unit and include its tests in
  the same commit when practical;
- use a concise imperative commit subject that describes the behavior or
  component changed;
- do not commit generated build trees, credentials, temporary downloads, or
  debugging artifacts.

Keep the branch buildable and its checked-in tests passing after every commit
unless the commit is explicitly marked as a non-buildable mechanical step and
is immediately followed by its completing commit. Prefer restructuring work so
that such exceptions are unnecessary. Do not rewrite or squash commits that
belong to the user unless explicitly requested.

## Build and test gates

Every change runs the smallest relevant preset. A milestone is complete only
after the full release gates pass:

1. Configure and build through supported CMake presets with first-party
   warnings as errors.
2. Run all CTest HTTP/query, URL/SSRF, MIME, media, APNG, golden, malformed,
   and mock Runtime API suites offline.
3. Run ASan/UBSan in an instrumented non-production build.
4. Run libFuzzer smoke corpora for event/query/URL, MIME, PNG/APNG, ICO, and
   streaming metadata parsing.
5. Verify `bootstrap` with `file`, `llvm-readelf`, and `ldd`: static PIE, no
   interpreter, no dynamic section/`DT_NEEDED`, and no unresolved symbol.
6. Inspect the link map and SBOM: BoringSSL is the only TLS provider and no
   GPL-only or unexpected codec is linked.
7. Run in a minimal filesystem and then deploy a canary to verify headers,
   binary integrity, bodies above the buffered limit, timeouts, and cold/warm
   invocations.

Compatibility means exact status, headers, body, and—where a golden hash is
defined—encoded bytes. Image diagnostics additionally compare dimensions,
per-frame pixels, delays, loop behavior, alpha, orientation, and profiles. Do
not weaken a byte gate to pixel equivalence without an approved specification
change.

## Dependency and licensing discipline

Maintain a machine-readable lock with source URL, revision, hash, license,
build options, and reason for every dependency. Disable unused loaders and
features. Do not link ImageMagick/GraphicsMagick, PDF loaders, video support,
AWS SDK, or x265.

Static linking of LGPL components can require notices, corresponding source,
and a relinking mechanism. Produce the chosen compliance bundle from CMake and
treat license verification as a release gate.
