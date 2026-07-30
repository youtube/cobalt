# GN Dependency Patterns

Use this reference to choose the narrowest correct dependency fix.

## `deps` vs `public_deps`

Use `deps` when the dependency is needed only by implementation files:

```gn
source_set("impl") {
  sources = [ "foo.cc" ]
  deps = [
    ":foo",
    "//components/prefs",
  ]
}
```

Use `public_deps` only when consumers of this target's public headers also need
the dependency to compile:

```gn
source_set("foo") {
  sources = [ "foo.h" ]
  public_deps = [
    "//chrome/browser/profiles:profile",
  ]
}
```

`public_deps` is transitive. Every downstream target inherits it, so adding
entries there widens the dependency graph. Treat `public_deps` as part of the
API contract.

## Forward declarations

If a header only mentions a type by pointer or reference, prefer a forward
declaration:

```cpp
class PrefService;

class Foo {
 public:
  explicit Foo(PrefService* prefs);
};
```

Move the concrete include to the `.cc` file:

```cpp
#include "components/prefs/pref_service.h"
```

This keeps implementation dependencies private and avoids leaking them through
public headers.

Forward declarations are not enough when the header:

- Stores the type by value.
- Inherits from the type.
- Uses inline methods that dereference the type.
- Needs the complete type for templates, constants, or member layout.

## Choosing the target to edit

Edit the smallest target that owns the failing source file. Avoid fixing a small
include failure by adding dependencies to:

- `//chrome/browser:browser`
- `//chrome/test:unit_tests`
- umbrella `:all` targets
- unrelated parent directories

If the source file is in a `source_set()` that is consumed by a larger test or
executable target, add the dependency to the `source_set()` that owns the file,
not the final executable, unless the final executable owns the file directly.

## Platform-specific dependencies

If the failing include is in a platform-specific file, keep the dependency under
the same platform condition whenever possible. For example, an include used only
by `icon_loader_win.cc` should usually produce a Windows-only dependency:

```gn
source_set("icons") {
  sources = [
    "icon_loader.cc",
  ]

  if (is_win) {
    sources += [
      "icon_loader_win.cc",
    ]
    deps += [
      "//ui/gfx:win",
    ]
  }
}
```

Do not add platform-only targets unconditionally unless the target is already
unconditional and valid on every supported platform:

```gn
deps += [ "//ui/gfx:win" ]
```

Common file suffixes include `_win.cc`, `_mac.mm`, `_linux.cc`, `_android.cc`,
`_ios.mm`, `_chromeos.cc`, and `_posix.cc`. Match the existing conditions in the
nearest `BUILD.gn`, such as `is_win`, `is_mac`, `is_linux`, `is_android`,
`is_ios`, `is_chromeos`, or `is_posix`.

## DEPS include rules

`DEPS` rules enforce source-level layering. A `BUILD.gn` dep does not override a
blocked include path.

When changing DEPS:

- Prefer the narrowest `specific_include_rules` entry.
- Match existing patterns in the nearest `DEPS` file.
- Do not add broad directory-wide allow rules for a single file.
- Treat DEPS failures as possible architecture signals, not just syntax errors.

## Generated headers

Generated headers are often owned by generated targets rather than the directory
where the include path appears to live. Search for existing includes of the same
generated header and inspect their dependencies before adding a new dep.

## Circular dependencies

If adding a direct dependency creates a cycle, the first fix is usually not a
broader dep. Use `gn path` to print the dependency chain between the two targets
and identify the boundary where the cycle should be broken:

```bash
gn path out/Default //target_a //target_b
```

Use the relevant generated output directory instead of hard-coding `out/Default`
when another out dir is known. After locating the dependency chain, consider:

- Splitting `:public` / `:<feature>` headers from `:impl`.
- Moving a shared interface to a lower-level target.
- Replacing concrete dependencies with dependency injection.
- Moving includes from headers into `.cc` files.

Only use `allow_circular_includes_from` when the surrounding component already
uses that pattern and the dependency cycle is intentional and documented.
