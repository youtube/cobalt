---
name: latency-instrumentation
description: >-
  Surgically injects trace macros (TRACE_EVENT) into Chromium C++ files to
  expose "black box" latency gaps, resolves headers, and compiles the browser
  using a self-healing loop. Strictly no performance optimizations or refactoring.
---

# Latency Instrumentation & Self-Healing Compile Loop

This skill guides the agent through surgically injecting `TRACE_EVENT` macros
into Chromium C++ files to break down uninstrumented main-thread blocking gaps
("black boxes") and verifying the changes via an automated compilation loop.

## ⚠️ CRITICAL DESIGN CONSTRAINT: NO OPTIMIZATIONS

This skill is strictly for **data collection and instrumentation**. You **MUST
NOT** make any performance optimization or refactoring changes (such as caching,
background-thread offloading, or logic restructuring). Your sole purpose is to
add instrumentation to help the Trace Analyzer drill down into untraced gaps.

______________________________________________________________________

## 1. Prerequisites & Input Context

Before executing this skill, you must have:

- `target_method`: The fully qualified C++ method name (e.g.
  `LocationIconView::Update`).
- `category`: The trace category (e.g., `"omnibox"`, `"navigation"`).
- `instructions`: Step-by-step C++ trace injection instructions (e.g.,
  identifying loops or blocks to wrap).
- `parent_session_id`: The unique session ID.

### Sandbox & Safety (Zero-Grant Rule)

- **ALWAYS** format all modified C++ files using `git cl format` immediately
  after editing.
- **ALWAYS** commit successful changes locally on the active E2E branch to
  checkpoint progress. Never create a CL or push to remote.
- If compilation fails after the self-healing loop, **ALWAYS** revert all
  uncommitted changes using `git reset --hard HEAD` to cleanly restore the last
  successful iteration's checkpoint.

______________________________________________________________________

## 2. Workflow

### Step 1: Locate target files

Use the codesearch tool to find the `.cc` and `.h` files declaring the
`target_method`. Example:

```query
lang:cpp "LocationIconView::Update"
```

If remote search yields zero results (common for unsubmitted local edits), run
local ripgrep:

```bash
rg -n --type cpp "LocationIconView::Update"
```

Read the files using `view_file`.

### Step 2: Identify Method Boundaries

Analyze the C++ source file to find the exact start and end lines of the
`target_method`. Take care to match namespace scope, class qualifiers, and
specific overloads.

### Step 3: Surgically Inject Trace Macros

1. Parse the code within the identified method boundaries to locate internal
   sub-operations (e.g., heavy loops, helper function calls, or large nested
   blocks).
2. Wrap these blocks in scoped trace macros:
   ```cpp
   {
     TRACE_EVENT0("category", "ParentMethodName::SubBlockName");
     // original code block
   }
   ```
3. Verify if `#include "base/trace_event/trace_event.h"` is present at the top
   of the C++ file. If missing, inject it programmatically inside the include
   section.
4. **CRITICAL:** Use file content modificatio tools for all modifications.
   **NEVER** use command-line tools like `sed` or `cat << 'EOF'`.
5. Run `git cl format <modified_files>` via `run_command` to format the code.

### Step 4: Rebuild & Self-Healing Loop (Max 3 Attempts)

You are allowed up to **3 total compilation attempts** to resolve any syntax,
missing include, or structural errors:

1. **Trigger Build:** Run `autoninja` to compile:
   ```bash
   autoninja -C out/Default chrome
   ```
2. **Success & Checkpoint:** If it compiles successfully:
   - Run `git add <modified_files>` (add files manually, avoid wildcards).
   - Run `git commit -m "feat(e2e-nla): Instrument [target_method]"` to
     checkpoint your progress.
   - Report success and stop.
3. **Failure & Repair Heuristics:**
   - Parse the compiler `stderr` output.
   - Identify the compilation error (e.g., namespace collision, undeclared
     identifier, missing header).
   - Apply a targeted code repair using code editing tools.
   - Re-run `autoninja`.
4. **Revert on Hard Failure:** If all 3 attempts fail, run
   `git reset --hard HEAD` to revert all uncommitted modifications back to the
   last successful checkpoint (preserving previous successful iterations), and
   report failure.

______________________________________________________________________

## 3. Output Contract

Report back with a JSON payload matching this structure:

```json
{
  "status": "SUCCESS" | "FAILED",
  "modified_files": ["chrome/browser/...cc"],
  "compile_output": "Compiler logs or error details"
}
```
