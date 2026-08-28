# Cpp Api Migrations

### Detecting and Propagating API Signature Changes

A frequent source of breakage during a rebase is the modification of function or method signatures in the upstream codebase.

**Detection Strategy:**
1.  If a build fails with a "no matching function for call" or "too few/many arguments" error, this strongly indicates a signature change.
2.  Use `TOOL_UPSTREAM_DIFF` on the header file where the function is declared. Look for changes to the function's parameter list or name.

**Resolution Strategy:**
1.  **Renaming:** If the function was simply renamed (e.g., `SetPrimaryMainFrameImportance` -> `SetPrimaryPageImportance`), update all call sites with the new name.
2.  **Added Parameters:** If new parameters were added, update the call site to pass them. For new parameters, you may need to:
    *   Infer a sensible default value (e.g., `ChildProcessImportance::NORMAL`).
    *   Look at upstream test files (`..._unittest.cc`, `..._browsertest.cc`) to see how the new API is intended to be used.
3.  **Removed Parameters:** Simply remove the corresponding arguments from the call site.
4.  **Propagating to Overrides:** If your downstream code *overrides* a virtual method from an upstream class, you **must** update the signature in your derived class's header (`.h`) and implementation (`.cc`) files to match the base class exactly.
