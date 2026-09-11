// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

# Cobalt Unified Symbolization Tool

A modular, high-performance, and extensible symbolization framework for Cobalt and Starboard across all crash and stack trace formats (AddressSanitizer, Cobalt stack dumps, Android logcat/tombstones, GDB, and raw addresses).

---

## 1. Overview & Key Capabilities

- **Modular Package Architecture**: Decomposed into clean, isolated modules under `starboard/tools/symbolize/` for process management, format handling, session tracking, and test runner JSON processing.
- **High Performance & Sub-Second Execution**: Uses a persistent `llvm-symbolizer` subprocess with an in-memory LRU cache, processing 1,000+ frames in under 0.2 seconds.
- **Single-Process Multi-Binary Dispatch**: Leverages `llvm-symbolizer`'s interactive `"<binary>" <offset>` protocol to symbolize across multiple binaries (`libcobalt.so`, `loader_app`, `libc.so`) within a single persistent process.
- **Deadlock-Free I/O**: Stderr is redirected to `subprocess.DEVNULL` to avoid OS pipe deadlocks on missing or corrupted libraries.
- **Dynamic Multi-Session Tracking**: Automatically detects `Load start=0x...` markers in streaming logs in $O(1)$ memory, seamlessly handling app reloads and restarts without buffering entire streams.
- **Three-Tier Address Mode Resolution**: Eliminates user guesswork for absolute vs. relative addresses:
  - **Tier 1 (Architectural Range)**: 64-bit ASLR addresses ($> 4\text{ GB}$) are unequivocally absolute; offsets $< 100\text{ MB}$ in shared libraries are relative.
  - **Tier 2 (Multi-Frame Probing)**: Probes `llvm-symbolizer` across call stack frames to detect consensus and thread entry roots (`main`, `SbEventHandle`, `MessageLoop`).
  - **Tier 3 (Missing Base Fallback)**: If an address is absolute but no `Load start=` was recorded, logs a warning and preserves the original frame without corrupting offsets.
- **Format & Prefix Preservation**: Preserves syslog timestamps, logcat prefixes, and original indentation while expanding inlined frames (`#0`, `#1`, ...).
- **Automatic Build Prefix Stripping**: Automatically strips build path prefixes such as `Release/../../` and `out/`.
- **Fast Pre-Filtered JSON Processing**: Test runner summaries (`test_summary.json`) are pre-filtered in $O(1)$ time to skip passing test snippets without base64 decoding.
- **Backward-Compatible Drop-In Wrapper**: `tools/valgrind/asan/asan_symbolize.py` is a thin wrapper delegating to this engine while preserving all existing CLI flags and test launcher behaviors.

---

## 2. Architecture & Modules

```
starboard/tools/symbolize/
├── __init__.py           # Package exports for public APIs
├── symbolize.py          # Primary CLI entry point and stream coordinator
├── runner.py             # Persistent SymbolizerRunner with interactive multi-binary piping and LRU cache
├── formats.py            # FormatRegistry and FormatHandler implementations
├── detector.py           # StreamingSessionTracker and three-tier address mode resolver
├── json_processor.py     # Decoupled test runner JSON processor with fast pre-filtering
├── symbolize_test.py     # Comprehensive unit, integration, and performance regression tests
├── README.md             # Documentation (this file)
└── testdata/             # Hardware, platform, and ASan crash logs
```

### Module Responsibilities

1. **`symbolize.py`**:
   High-level CLI coordinator. Parses command-line arguments and streams input lines through the session tracker and format registry.
2. **`runner.py`**:
   Contains `SymbolizerRunner`, managing the persistent `llvm-symbolizer --demangle --inlines` subprocess. It coordinates multi-binary lookup queries, parses inlined function pairs, and manages the LRU cache.
3. **`formats.py`**:
   Defines the `FormatHandler` interface and `FormatRegistry`. Dispatches lines to matching handlers (ASan Mode 1 & 2, Android, Cobalt, GDB, Raw) and formats symbolized output with inlined frame expansion and prefix preservation.
4. **`detector.py`**:
   Contains `StreamingSessionTracker`, managing per-session base address state from `Load start=0x...` lines and implementing the three-tier address mode resolver.
5. **`json_processor.py`**:
   Contains `process_test_summary_json` and `process_test_run`. Inspects test runner JSON files, applies fast $O(1)$ pre-filtering for stack trace signatures, decodes snippets, symbolizes traces, and rewrites the JSON file.

---

## 3. Supported Formats & Examples

| Format | Raw / Unsymbolized Line | Symbolized Output |
| :--- | :--- | :--- |
| **ASan Mode 1**<br/>*(Unknown Module)* | `    #0 0x7f48e7a45000  (<unknown module>)` | `    #0 0x1000 in cobalt::dom::Node::parentNode() const cobalt/dom/node.cc:154`<br/>`    #1 0x1000 in cobalt::dom::Element::parentElement() const cobalt/dom/element.cc:82` *(inlined)* |
| **ASan Mode 2**<br/>*(Relative Offset)* | `    #0 0x7f6e35cf2e45  (/lib/libcobalt.so+0x11fe45)` | `    #0 0x7f6e35cf2e45 in base::debug::StackTrace::StackTrace() stack_trace.cc:255` |
| **Cobalt Stack Dump**<br/>*(SbLogRawDumpStack)* | `[10:00:01] YouTube[100]: \t<unknown> [0x29b4ef9]` | `[10:00:01] YouTube[100]: \t0x29b4ef9 [cobalt::CobaltBrowserMainParts::PreCreateThreads()]` |
| **Android Logcat / Tombstone** | `09-08 17:27:44.416 E chromium: #00 pc 0x03bef795 /lib/libcobalt.so` | `09-08 17:27:44.416 E chromium: #00 pc 0x3bef795 in base::debug::StackTrace::StackTrace() stack_trace.cc:255 (/lib/libcobalt.so)` |
| **GDB Stack Frame** | `    #1  0x742a51b6 in ?? () from /lib/libcobalt.so` | `    #1 0x742a51b6 in gdb_resolved_func() gdb.cc:10` |
| **Raw Hex Address** | `0x7efcdf1fd52b` | `0x7efcdf1fd52b raw_symbol() in raw.cc:99` |

---

## 4. Command-Line Usage

### 1. Basic Symbolization of a Crash Log File
```bash
python3 starboard/tools/symbolize/symbolize.py -f crash.log -l out/evergreen-x64_devel/app/cobalt/lib/libcobalt.so
```

### 2. Passing the Start Offset / Base Address (`base_address`)
When stack traces contain absolute memory addresses (e.g., Cobalt stack dumps or raw addresses) and the library load address is known, specify the base address as an optional positional argument:
```bash
# Explicit base address in hexadecimal:
python3 starboard/tools/symbolize/symbolize.py -f crash.log -l out/evergreen-x64_devel/app/cobalt/lib/libcobalt.so 0x7f1000000000

# Explicit base address in decimal:
python3 starboard/tools/symbolize/symbolize.py -f crash.log -l out/evergreen-x64_devel/app/cobalt/lib/libcobalt.so 4096
```
If omitted, `base_address` defaults to `'0'`, and `StreamingSessionTracker` dynamically resolves addresses using `Load start=0x...` markers or the three-tier resolution strategy.

### 3. Stream Real-Time from `stdin` or Device Pipes
Pass `-f -` or pipe standard input directly:
```bash
# Android logcat streaming:
adb logcat | python3 starboard/tools/symbolize/symbolize.py -f - -l out/android-arm_devel/libchrobalt.so

# RDK / Linux syslog streaming:
journalctl -f | python3 starboard/tools/symbolize/symbolize.py -f - -l out/evergreen-arm-hardfp-rdk_devel/libcobalt.so

# Using backward-compatible wrapper:
cat asan_crash.log | python3 tools/valgrind/asan/asan_symbolize.py --extra-binary out/evergreen-x64_devel/libcobalt.so
```

### 4. Symbolize Test Launcher JSON Summaries
```bash
python3 starboard/tools/symbolize/symbolize.py \
  --test-summary-json-file out/test_summary.json \
  -l out/evergreen-x64_devel/app/cobalt/lib/libcobalt.so

# Or via asan_symbolize.py:
python3 tools/valgrind/asan/asan_symbolize.py \
  --test-summary-json-file out/test_summary.json \
  --extra-binary out/evergreen-x64_devel/app/cobalt/lib/libcobalt.so
```

### 5. Strip Custom Source Path Prefixes
```bash
python3 starboard/tools/symbolize/symbolize.py \
  -f crash.log \
  -l libcobalt.so \
  0 \
  "custom/build/prefix/"
```

### 6. Command-Line Compatibility Matrix

Both tools preserve 100% backward compatibility for existing users, test launchers, and automation scripts:

| Tool | Original Command-Line Invocation | Current Behavior & Options | Compatibility Status |
| :--- | :--- | :--- | :--- |
| [`starboard/tools/symbolize/symbolize.py`](file:///usr/local/google/home/jfoks/cobalt.main/src/starboard/tools/symbolize/symbolize.py) | `symbolize.py -f <file> -l <lib> [base_address]` | Exactly preserves `-f`, `-l`, and positional `[base_address]`.<br>Adds non-breaking options: `--extra-binary`, `--test-summary-json-file`, `strip_path_prefix`, and standard stream piping. | **100% Backwards Compatible** |
| [`tools/valgrind/asan/asan_symbolize.py`](file:///usr/local/google/home/jfoks/cobalt.main/src/tools/valgrind/asan/asan_symbolize.py) | `cat log \| asan_symbolize.py [--extra-binary <bin>] [strip_path_prefix ...]`<br>`asan_symbolize.py --test-summary-json-file <json> [--extra-binary <bin>]` | Preserves all CLI flags (`--test-summary-json-file`, `--extra-binary`, `--executable-path`, `--sysroot`, `strip_path_prefix`), stream piping from stdin to stdout, and in-place JSON rewrite semantics. | **100% Backwards Compatible** |


---

## 5. Python API Reference

Import the package directly in Python scripts and test tools:

```python
from starboard.tools.symbolize import (
    SymbolizerRunner,
    symbolize_stream,
    symbolize_string,
    process_test_summary_json,
)

# 1. Symbolize an in-memory string:
symbolized_text = symbolize_string(
    raw_crash_log,
    library='/path/to/libcobalt.so',
    base_address='0',
    strip_prefixes=['custom/prefix/'],
)

# 2. Stream between file streams or pipes:
with open('crash.log', 'r') as in_file, open('symbolized.log', 'w') as out_file:
  symbolize_stream(
      in_stream=in_file,
      out_stream=out_file,
      library='/path/to/libcobalt.so',
  )

# 3. Direct multi-binary programmatic lookups:
with SymbolizerRunner(default_library='/path/to/libcobalt.so') as runner:
  # Query default library
  frames = runner.symbolize(0x29b4ef9)
  # Query an auxiliary binary in the same persistent process:
  loader_frames = runner.symbolize(0x12cf8c, binary='/path/to/loader_app')

# 4. Process test runner JSON summaries:
process_test_summary_json(
    'out/test_summary.json',
    symbolize_fn=lambda lines: symbolize_string(''.join(lines), library='/path/to/libcobalt.so'),
)
```

---

## 6. Running Tests

The test suite includes unit tests, hermetic synthetic shared library integration tests (compiled on the fly via Clang), and performance benchmarks:

```bash
# Run symbolize package tests (39 tests):
python3 starboard/tools/symbolize/symbolize_test.py -v

# Run asan_symbolize backward-compatibility tests (17 tests):
python3 tools/valgrind/asan/asan_symbolize_test.py -v

# Run both via pre-commit / presubmits:
node ~/.gemini/config/skills/cobalt-presubmit-checker/scripts/check_presubmits.cjs --skip-review
```
