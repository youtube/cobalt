# WAMR in Cobalt (proof of concept)

WebAssembly Micro Runtime, https://github.com/bytecodealliance/wasm-micro-runtime

The source isn't checked in. `fetch_wamr.py` clones upstream at commit
`8c18e3f68b16c4bcaf05996b2636f6ed2b4cf629` (closest release WAMR-2.4.4, the
same version as google3's `//third_party/wasm_micro_runtime`) into `src/`,
which is git-ignored. `BUILD.gn` runs it at gn time (`exec_script`), so a
fresh checkout builds with plain `autoninja`; the first run needs network
access.

`BUILD.gn` builds the fast interpreter only (no AOT, JIT, WASI, libc-builtin,
threads or SIMD) for Linux and Android, replacing WAMR's CMake build. No local
modifications. License: Apache 2.0 with LLVM exception, see `src/LICENSE`.

Used by `//cobalt/shell/browser/splash` to run the splash selector module.
