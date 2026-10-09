# Cobalt ICU Data (`icudtl.dat`)

This directory contains the customized ICU data bundle for Cobalt (`icudtl.dat`).

## Overview

Cobalt Evergreen and Modular Linux run hermetically without relying on system libc locale data or host ICU installations. Instead, Cobalt's POSIX libc locale subsystem (`<locale.h>`, `<langinfo.h>`) and web engine delegate to ICU to provide standard locale, calendar, and monetary formatting.

To minimize binary size and memory footprint while supporting all 82 YouTube UI languages, Cobalt uses a tailored ICU data filter rather than the full ~31 MB upstream ICU data bundle.

## Configuration

The contents of `icudtl.dat` are defined by the filter configuration in:
- **`third_party/icu/filters/cobalt.json`**

### What the Filter Controls:
1. **`localeFilter`**: Defines the included languages based on YouTube's supported UI languages.
2. **`curr_tree`**: Includes currency names and formatting for supported locales, with whitelisted currency codes.
3. **`locales_tree`**: Configures calendar, date/time formatting, month/day names, and numeric separators.
4. **`coll_tree`**: Configures collation (sorting) rules, stripping redundant legacy character set collations.
5. **`zone_tree`**: Configures timezone data, stripping unused exemplar cities to conserve space.

## Build-Time Generation (`generate_cobalt_icudata`)

Rather than checking a ~4–7 MB binary `icudtl.dat` file into Git, Cobalt generates `icudtl.dat` automatically at build time from `third_party/icu/filters/cobalt.json`:

1. **GN Action (`//third_party/icu:generate_cobalt_icudata`)**:
   - Defined in `third_party/icu/BUILD.gn` under `default_toolchain` so it runs once per build output directory (`$root_build_dir/gen/third_party/icu/cobalt/icudtl.dat`), even in multi-toolchain builds (Android, Evergreen).
   - Executes `third_party/icu/scripts/generate_cobalt_icudata.py`.
2. **Host Tool Caching**:
   - Compiles ICU host generator tools (`genrb`, `gencmn`, `icupkg`, etc.) once per output directory in `$root_build_dir/gen/third_party/icu/host_build` using the host compiler (`Linux/gcc` on Linux, `MacOSX` on Darwin).
   - Subsequent edits to `third_party/icu/filters/cobalt.json` reuse the cached host tools and rebuild `icudtl.dat` in ~3–4 seconds.
3. **Standalone / Manual Invocation**:
   - You can also generate `icudtl.dat` manually from the command line via:
     ```bash
     ./third_party/icu/scripts/make_data_cobalt.sh
     ```
