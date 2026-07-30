# Legacy Mojo Core

This directory contains a frozen, self-contained copy of the legacy
(pre-ipcz) Mojo Core implementation, for exclusive use by the ChromeOS Mojo
Proxy in `../service`. The proxy bridges processes running Mojo over ipcz
with ChromeOS processes (e.g. ARCVM) that still run legacy Mojo Core, so it
must embed a legacy Mojo Core node; freezing that implementation here lets
`//mojo` evolve (and eventually delete legacy Mojo Core) without affecting
the proxy. See https://crbug.com/359926651.

Layout mirrors `//mojo`:

* `core/`: the legacy Mojo Core implementation, with `core/ports/` and the
  `core/embedder/` API, plus the legacy unit tests and the `core/test/`
  test-support library.
* `public/c/system/`, `public/cpp/platform/`, `public/cpp/system/`: the
  subset of `//mojo/public` that legacy Mojo Core and its tests need.
* `public/c/test_support/`, `public/tests/`: the C/C++ test-support shim.

The clone retains the legacy Mojo Core unit tests (built as the
`mojo_legacy_unittests` target) so that the coverage `//mojo` loses when it
drops legacy Mojo Core is preserved here.

Everything lives in the `mojo_legacy` namespace (instead of `mojo`), and
`MOJO_LEGACY_`-prefixed macros (instead of `MOJO_`), so that the clone
can be linked into the same binary as the real `//mojo` without any symbol,
type or macro collisions.
