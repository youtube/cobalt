# Common GN Dependency Errors

Use this reference to classify Chromium GN dependency failures before editing.

## `Include not allowed by DEPS`

The include is blocked by a `DEPS` file, not merely by a missing `BUILD.gn`
dependency.

Typical causes:

- The directory's `DEPS` rules intentionally forbid this layer from including
  the target header.
- The include path needs a narrow `specific_include_rules` exception.
- The code should depend on a lower-level abstraction instead of the forbidden
  header.

Safe response:

1. Read the nearest relevant `DEPS` file and any parent `DEPS` files.
2. Check whether similar files in the same directory already have an allow rule.
3. Prefer changing the code to follow the intended layering.
4. Add an allow rule only when it matches existing architecture and is as narrow
   as possible.

## `Can't include this header from here`

GN can see the header, but the current target is not allowed to include it
through the declared dependency path. This often means the dependency is private
to another target or only reachable transitively.

Safe response:

1. Find the owner target of the including file.
2. Find the owner target of the included header.
3. Add a direct dependency to the including target when the dependency direction
   is valid.
4. If the include is in a public header, decide whether the owner target needs
   `public_deps` or whether the include can move to the `.cc` file.

## Missing direct dependency

The build succeeds only because another target brings in the header
transitively, or it fails because direct dependency checking is enabled.

Safe response:

- Add the direct dependency to the target that owns the source file.
- Keep it in `deps` unless a public header exposes the dependency.

## Private header visibility

The header belongs to an implementation target that is not meant to be consumed
directly.

Safe response:

- Look for a public/interface target in the same directory.
- Include the public API header instead of a private implementation header.
- If no public API exists, consider whether the dependency direction is wrong.

## Circular dependency

Adding the obvious dependency creates a GN cycle.

Safe response:

- Do not add broader aggregate deps to hide the cycle.
- Split public headers from implementation if that reduces the cycle.
- Move shared interfaces to a lower-level target.
- Use forward declarations in headers when possible.
- Consider dependency injection instead of direct concrete-type dependencies.
