# GN Missing Dependencies

This document outlines how to investigate and resolve GN missing dependencies, which cause intermittent build failures. These errors are highly flaky—typically only manifesting on highly parallelized builds.

## The Symptoms

**The Failure:** If a target starts compiling before its unlisted dependency finishes generating, the build fails with an error like this:

```
[22664/70808] CXX obj/components/bar/baz.o
../../third_party/llvm-build/Release+Asserts/bin/clang++ ...
components/bar/baz.cc:12891:32: fatal error: 'components/foo/foo_features.h' file not found
#include "components/foo/foo_features.h"
         ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 error generated.
```

**The False Success:** If the dependency happens to finish generating first, or if the generated file already exists from a previous run, the build succeeds and hides the bug.

## Reliable Reproduction

To reliably reproduce a missing dependency:
1.  **Clean the build directory:**
    ```bash
    gn clean out/Default
    ```
2.  **Build only the failing target:**
    Build only the target containing the failing source file (from the error message):
    ```bash
    autoninja -C out/Default obj/components/bar/baz.o
    ```

If the dependency is missing, this will fail 100% of the time on a clean build.

## Investigation Workflow

### 1. Find the Generating Target
Use `gn refs` to find which GN target generates the missing header:
```bash
gn refs out/Default //out/Default/gen/components/foo/foo_features.h --relation=output
```
This should return the generating target (e.g., `//components/foo:foo_features`).

### 2. Get the Suggestion using `gn suggest`
GN's `gn suggest` tool can analyze the build graph and suggest the exact dependency change:
```bash
gn suggest out/Default //components/bar/baz.cc=components/foo/foo_features.h
```

**Example Output:**
```
Suggestion: Add deps = [ "//components/foo:foo_features" ] to :baz (defined at //components/bar/BUILD.gn:10)
```

Apply the suggestion to the corresponding `BUILD.gn` file.

## Verification

Re-run the reproduction steps to verify the fix. The build should now succeed.
