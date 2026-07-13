# Compatibility test manifests

`vector-inventory.json` is the machine-readable inventory for the offline
compatibility suite. Each case has a stable test ID, the normative
`SPECIFICATION.md` sections that it covers, the future test suite that owns it,
and the variants that must be exercised. A variant is a required test vector,
not an example that may be omitted.

Fixture and golden paths will be attached to these IDs as the corresponding
offline assets are added. Implementations may split one inventory case into
multiple executable tests, but they must retain the inventory ID so coverage
can be audited without inspecting test source.

Section 11 is intentionally absent because the specification marks it as
non-normative design provenance.
