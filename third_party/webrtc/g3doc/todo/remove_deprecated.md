<!-- go/cmark -->

<!--* freshness: {owner: 'hta' reviewed: '2026-04-24'} *-->

# Plan: Iterative Removal of Deprecated Symbols

This document outlines the iterative process for identifying and removing
deprecated symbols from the WebRTC codebase. The goal is to safely retire old
APIs that have zero footprint in internal and external downstream projects.

## Removal Criteria

A symbol is a candidate for removal only if it meets **all** of the following:

1. **Zero Usage:** No references found in WebRTC internals or downstream
   projects.
2. **Maturity:** The `[[deprecated]]` tag was added at least **3 months** ago.
3. **Batch Size:** No more than **10 symbols** are processed in a single
   iteration.

______________________________________________________________________

## Phase 1: Candidate Identification (Iterative)

Perform the following steps to generate a list of exactly 10 candidates for the
current removal cycle:

1. **Discovery:** Search the entire WebRTC codebase (excluding `third_party/`)
   for the `[[deprecated]]` attribute.
2. **Age Verification:** Use `git blame` on the identified lines. Discard any
   symbols where the deprecation tag was added less than 3 months ago.
3. **Internal Audit:** Run `git grep <SymbolName>` in the WebRTC repository.
   Discard any symbols that still have internal call-sites (these must be
   migrated first). Discard the oldest symbols first.
4. **Downstream Audit (CodeSearch):** For each matured symbol, perform broad
   searches in downstream projects (outside `third_party/webrtc`).
   - **External Usage:** `cs "content:<SymbolName> -file:stable/webrtc"`
   - **Search Optimization:** To minimize false positives with common names
     (like `kPlanB` or `local_ssrc`), prioritize searching for qualified usage
     (e.g., `cs "content:SdpSemantics::kPlanB"` or `cs "content:::kPlanB"`) or
     structural access (e.g., `cs "content:.local_ssrc"`) before performing
     broad searches for the bare symbol name.
   - *Note:* Pay special attention to virtual methods. If a symbol is overridden
     in a downstream project, removing it from the WebRTC base class will break
     the downstream build.
5. **Iteration Stop:** Continue the audit process until exactly **10 symbols**
   have been confirmed to have zero results. Once 10 are found, stop the search.
6. **Documentation:** List the 10 selected symbols, their locations, and their
   modern equivalents.

______________________________________________________________________

## Phase 2: Execution and Validation

For the 10 selected candidates:

1. **Test Migration:** Update any internal WebRTC unit tests or regression tests
   that still use the deprecated symbol.
2. **Code Removal:** Delete the deprecated declarations and implementation.
3. **Bug Tracking:** If a `TODO` with a bug number was attached to the
   deprecated function and removed, add that bug number with a `webrtc:` prefix
   to the `Bug:` line in the commit message.
4. **Local Build:** Perform a clean build of all major targets (e.g.,
   `rtc_unittests`, `rtc_pc_unittests`, `peerconnection_unittests`).
5. **Verification:** Run the relevant test suites and ensure 100% pass rate.
6. **Gerrit Upload:** Submit the changes for review. The CL description should
   refer to "downstream projects" generally and MUST NOT contain the specific
   names of these projects.

______________________________________________________________________

## Phase 3: High-Usage Coordination

Symbols that return non-zero usage in downstream projects must not be removed
using this process. Instead:

1. Identify the owner of the consuming code.
2. File a bug against the consuming team referencing the WebRTC deprecation.
3. Wait for the consuming team to migrate before moving the symbol to Phase 1 in
   a future iteration.
