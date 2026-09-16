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

## How to Regenerate `icudtl.dat`

A helper script is provided to automate the configuration, host tool compilation, and data packaging:

```bash
# From the Cobalt src root:
./third_party/icu/scripts/make_data_cobalt.sh
```

### Manual Steps (What the Script Does):
1. **Build Host Tools**: Configures and compiles ICU host generator tools (`genrb`, `gencmn`, `icupkg`, etc.) in a temporary build directory using `source/runConfigureICU Linux/gcc`.
2. **Apply Filter**: Configures `data/Makefile` using `ICU_DATA_FILTER_FILE=third_party/icu/filters/cobalt.json`.
3. **Compile Data**: Runs `make -C data` to compile CLDR source files into binary `.res` files and package them into `icudt<version>l.dat`.
4. **Copy Data**: Invokes `third_party/icu/scripts/copy_data.sh cobalt` to install the resulting bundle to `third_party/icu/cobalt/icudtl.dat`.

## Why `icudtl.dat` is Pre-Generated and Checked In

1. **Hermetic & Fast Builds**: Compiling ICU host tools and processing ~1,000 data files from scratch takes ~40–60 seconds. Checking in the pre-built binary avoids this overhead on every clean build.
2. **Remote Execution (Siso / RBE)**: Remote build execution requires all build actions to run within hermetic sandboxes with explicitly declared GN inputs. Autotools-based host generation pipelines cannot run inside Siso/RBE sandboxes.
3. **Cross-Compilation**: Cobalt targets platforms across multiple architectures (Android ARM/ARM64, RDK ARM, Apple tvOS/iOS, Linux x64). Generating data during the build would require cross-compiling host tools or having platform-specific generator binaries.
4. **Binary Assembly**: In GN (`third_party/icu/BUILD.gn`), `scripts/make_data_assembly.py` reads `icudtl.dat` and generates `icudtl_dat.S`, which is compiled directly into `libcobalt.so` and `libnplb.so`.
