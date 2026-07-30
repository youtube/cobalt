---
name: gn-deps-debugging
description: >-
  Diagnose Chromium GN dependency and include-visibility failures, including
  BUILD.gn deps/public_deps, DEPS include rules, private headers, and circular
  dependencies. Use for build or gn check dependency errors, not
  C++/link/runtime/test failures.
---

# GN Dependency Debugging

Fix Chromium GN dependency metadata, include visibility, and DEPS rule failures
by correcting the dependency graph accurately, not by adding broad dependencies
that only make errors disappear.

## Core principles

1. **Read the exact error first.** Identify the including source file, the
   included header, the failing target, and the diagnostic category before
   editing.
2. **Find both owning targets.** Determine which GN target owns the including
   file and which GN target owns the included header.
3. **Prefer the narrowest dependency.** Add the dependency to the smallest
   target that needs it, not a parent or aggregate target.
4. **Preserve platform conditions.** If the failing include is in a
   platform-specific file such as `*_win.cc`, add the dependency inside the
   matching platform block such as `if (is_win)`.
5. **Use `deps` for implementation-only dependencies.** If a symbol is used only
   in `.cc` files, keep the dependency private.
6. **Use `public_deps` only for public API exposure.** If a public header
   exposes a type from another target in its declarations or inline code,
   downstream consumers may need that dependency transitively.
7. **Prefer forward declarations in headers.** If a header only needs a pointer,
   reference, or return type declaration, forward declare and move the include
   to the `.cc` file when possible.
8. **Do not paper over architecture issues.** Circular dependencies usually
   indicate the need to split an interface target, move shared abstractions, or
   invert dependencies.
9. **Avoid `// nogncheck`.** Use it only for known GN limitations with
   conditional includes, and include the required bug reference if the local
   codebase already documents one.

## Workflow

1. **Capture the failing command and output.**

   - If the user provided a truncated error, ask for or rerun the relevant
     `gn check` or build command when an output directory and target are known.
   - Do not start a broad build just to reproduce unless the output directory
     and target are established.

2. **Classify the failure.**

   - Read [common-errors.md](references/common-errors.md) and map the diagnostic
     to one of the supported categories.
   - Extract the including file, included header, and target names from the
     diagnostic.

3. **Find the target that owns the including file.**

   - Inspect nearby `BUILD.gn` files first.
   - Use GN queries when an output directory is known:
     ```bash
     gn refs out/Default //path/to/file.cc --all
     ```
   - Prefer the most specific non-aggregate target that directly lists the file
     or owns the source set containing it.
   - Check whether the file is platform-specific, such as `*_win.cc`,
     `*_mac.mm`, `*_linux.cc`, `*_android.cc`, or `*_ios.mm`. Platform-specific
     files usually require platform-specific dependencies.

4. **Find the target that owns the included header.**

   - Search for the header in `BUILD.gn` sources lists.
   - If several targets list it, prefer the public/interface target intended for
     consumers.
   - Check whether the header belongs to a generated target, a `public`
     variable, or a private implementation target.

5. **Choose the correct fix.**

   - Read [dependency-patterns.md](references/dependency-patterns.md) for `deps`
     vs `public_deps`, forward declaration, DEPS, and cycle handling rules.
   - If the include appears only in `.cc`, add a private `deps` entry.
   - If the include appears only in a platform-specific source file, add the
     private `deps` entry inside the matching platform condition, for example
     `if (is_win)`.
   - If the include appears in a public header, first try to remove the include
     using a forward declaration. If the header must expose the dependency, add
     or promote the dependency to `public_deps`.
   - If a `DEPS` file blocks the include, verify whether the dependency
     direction is allowed by the architecture before adding include rules.
   - If adding the dependency creates a cycle, stop and redesign the dependency
     boundary instead of adding broader deps. When an output directory is known,
     use `gn path <out_dir> //target_a //target_b` to inspect the dependency
     chain that connects the targets.

6. **Validate the narrow change.**

   - Run `git cl format` if `BUILD.gn` or `DEPS` files were edited.
   - Re-run the smallest relevant `gn check` or build command that reproduced
     the issue.
   - If the first fix reveals additional dependency errors, repeat the workflow
     one error at a time.

## Common anti-patterns

- Adding dependencies to `//chrome/browser:browser` or another aggregate target
  when a smaller target owns the failing file.
- Adding a Windows-only, macOS-only, Android-only, or iOS-only dependency
  unconditionally when the include is only used by a platform-specific source
  file.
- Adding a dependency to `public_deps` because it makes the build pass, without
  proving the type is part of a public header API.
- Adding `// nogncheck` instead of fixing the dependency graph.
- Adding broad DEPS allow rules without checking the intended layering.
- Fixing only the first error while leaving the target's public/private boundary
  inconsistent.

## Reporting

When presenting a fix, state:

- The failing include and source file.
- The target that owns the source file.
- The target that owns the included header.
- Why the selected fix is `deps`, `public_deps`, forward declaration, DEPS, or
  target split.
- The validation command that passed, or why validation could not be run.
