# Security Policy

## Reporting a vulnerability

Please report security issues privately via GitHub Security Advisories for this
repository (or by contacting the maintainers). Do not open a public issue for
exploitable vulnerabilities.

## Trust model

- **Domain inputs** passed to `to_ascii` / `to_unicode` should be treated as
  **untrusted**. Callers must not assume the empty-string return from `to_ascii`
  is the only error channel; prefer the `bool` overloads when available.
- **Percent-decoding**, trimming of spaces/tabs/control characters, and host
  forbidden-code-point checks are the caller's responsibility (as in the WHATWG
  URL host parser).
- Prefer the full pipeline (`to_ascii` / `to_unicode`) over calling Punycode
  helpers alone on untrusted data.

## Resource limits

- Inputs longer than `ada::idna::max_domain_input_bytes` (see
  `include/ada/idna/limits.h`) are rejected by `to_ascii` and cause
  `to_unicode(in, out)` to return false. This bounds heap growth under
  adversarial input.

## Internal Unicode tables

- Large Unicode/IDNA tables are stored DEFLATE-compressed and expanded once on
  first use into a **process-lifetime** heap buffer (never freed).
- After inflate, a **CRC-32** of the uncompressed payload is checked against the
  value embedded at build time. A mismatch fails initialization; subsequent API
  calls fail closed.
- Do not `dlclose` a shared library that owns this buffer while other code may
  still call into `ada::idna`.

## First-use cost

- The first call that needs tables pays a one-time inflate (~1 ms typical).
  Steady-state lookups are O(1) multi-stage table access.
