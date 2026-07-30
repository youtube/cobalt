# Starboard POSIX APIs


## Background
Updating existing Chromium components in the Cobalt repository or
bringing new ones requires porting to Starboard. To reduce the effort in this
Starboardization process POSIX APIs are leveraged by introducing a Stable
POSIX ABI per CPU architecture.


### Verification
To verify a platform integration the `nplb` test should be run in Evergreen mode.

A test suite [starboard/nplb/posix_compliance](../nplb/posix_compliance) is added to `nplb`
to verify the POSIX APIs specification and to enforce uniformity across all platforms.

To run `nplb` in Evergreen mode:
1. Build the `nplb_loader` for the target platform. This will build both the
   `elf_loader_sandbox` (the equivalent of `loader_app` but for running tests
   on the command line) and the `libnplb.so` library.
    ```shell
    ninja -C out/${PLATFORM}_devel/ nplb_loader
    ```
1. Run the `nplb` test
    ```shell
    out/${PLATFORM}_devel/nplb_loader.py
    ```

`nplb_loader.py` is a wrapper script that runs the `elf_loader_sandbox`. The
`elf_loader_sandbox` takes two command line switches: `--evergreen_library` and
`--evergreen_content`. These switches are the path to the shared library to be
run and the path to that shared library's content. These paths should be
*relative to the content of the elf_loader_sandbox*.

```
.../elf_loader_sandbox
.../content/app/nplb/lib/libnplb.so
.../content/app/nplb/content
```

If the test fails in the `posix_compliance_tests` please take a look at the next section **Evergreen integration**. You may need to fix your **POSIX wrapper impl**.

### Evergreen integration
![Evergreen Architecture](resources/starboard_16_posix.png)

#### POSIX ABI headers (musl_types)
The POSIX standard doesn't provide an ABI specification. There is guidance
around type definitions, but the actual memory representation is left
to the platform implementation. For example the `clock_gettime(CLOCK_MONOTONIC, &ts)` uses
various constants (e.g. CLOCK_MONOTONIC) to define which system clock to query
and a `struct timespec` pointer to populate the time data. The struct has 2
members, but the POSIX standard does not explicitly require that these members
be a specific number of bytes (e.g. the type long may be 4 or 8 bytes),
nor the padding/layout of the members in the struct
(e.g. a compiler is allowed to add additional padding).

The Starboard POSIX ABI is defined by a combination of existing
[third_party/musl](../../third_party/musl) types and values and new types
and values added under [third_party/musl/src/starboard](../../third_party/musl/src/starboard).
All of the types have a concrete stable ABI defined per CPU architecture.

#### POSIX wrapper impl
The musl ABI types are translated to the native POSIX types in the
POSIX wrapper implementation.
The actual implementation, which assumes a native POSIX support is
implemented in `starboard/shared/modular`. For example
[starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h](../shared/modular/starboard_layer_posix_time_abi_wrappers.h),
[starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.cc](../shared/modular/starboard_layer_posix_time_abi_wrappers.cc).

If the provided implementation doesn't work out of the box
partners can fix it and adjust for their own internal platform. If the
fix is applicable to other POSIX implementations they can upstream the change
to the Cobalt project.

#### Exported Symbols Map
The Exported Symbols Map is part of the Evergreen loader and provides all the unresolved
external symbols required by the Cobalt Core binary. The map is defined as
`std::map<std::string, const void*>` and all the Starboard and POSIX symbols are
registered there. The implementation resides in
[starboard/elf_loader/exported_symbols.cc](../elf_loader/exported_symbols.cc).
For POSIX APIs a wrapper function is used whenever a translation
from `musl` types to platform POSIX types is needed e.g.

```
map_["clock_gettime"] = reinterpret_cast<const void*>(&__abi_wrap_clock_gettime);
```
The symbol may be registered directly without a wrapper if there is no need
for any translation or adjustments of the API e.g.
```
REGISTER_SYMBOL(malloc);
```

# POSIX API Reference

The complete list of supported POSIX APIs can be found in [starboard/elf_loader/exported_symbols.cc](../elf_loader/exported_symbols.cc).
