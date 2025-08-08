# Cobalt Build Flavours

This document details naming conventions and common idioms for
build/preprocessor flags in Cobalt. This only applies to Cobalt on Chromium,
i.e. versions 26 (included) and later, i.e. project "Chrobalt" and after.

Note: This document is heavily inspired on the internal go/cobalt-naming doc.

The existing Cobalt and Starboard build logic defines a number of variables
that developers are free to rely upon. Some important patterns follow.

## Values for Common Variables per Cobalt Platform

GN variable          | Chromium (all platforms)        | Cobalt Android TV (`is_androidtv`) |   Cobalt AppleTV tvOS (`is_ios_tvos`) | Cobalt 3P Evergreen/Modular                                                               | Cobalt AOSP
-------------------  | ------------------------------- | -----------------------------------|-------------------------------------- |------------------------------------------------------------------------------------------ | ------------
`is_cobalt`          | <p style='color: red'>false</p> | <p style='color: green'>true</p>   | <p style='color: green'>true</p>      | <p style='color: green'>true</p>                                                          | <p style='color: green'>true</p>
`is_starboard`       | <p style='color: red'>false</p> | <p style='color: red'>false</p>    | <p style='color: red'>false</p>       | <p style='color: green'>true</p>                                                          | <p style='color: green'>true</p>
`use_starboard_media`| <p style='color: red'>false</p> | <p style='color: green'>true</p>   | <p style='color: green'>true</p>      | <p style='color: green'>true</p>                                                          | <p style='color: green'>true</p>
`use_evergreen`      | <p style='color: red'>false</p> | <p style='color: red'>false</p>    | <p style='color: red'>false</p>       | <p style='color: green'>Evergreen: true</p> <br> <p style='color: red'>Modular: false</p> | <p style='color: green'>true</p> ?
`sb_is_modular`      | <p style='color: red'>false</p> | <p style='color: red'>false</p>    | <p style='color: red'>false</p>       | <p style='color: green'>true</p>                                                          | <p style='color: green'>true</p>
`is_android`         | <p >Depends on the OS</p>       | <p style='color: green'>true</p>   | <p style='color: red'>false</p>       | <p style='color: red'>false</p>                                                           | <p style='color: green'>true</p>
`is_androidtv`       | <p style='color: red'>false</p> | <p style='color: green'>true</p>   | <p style='color: red'>false</p>       | <p style='color: red'>false</p>                                                           | <p style='color: green'>false</p>

Most of these variables are specified in files under
[//cobalt/build/configs](https://source.corp.google.com/h/github/youtube/cobalt/+/main:cobalt/build/configs)
and are propagated to Chromium's GN build configuration when using the
[gn.py](https://source.corp.google.com/h/github/youtube/cobalt/+/main:cobalt/build/gn.py)
generator.

After that point developers can use them in GN files (`is_bla`, `use_foo`) and
C++ (`#if BUILDFLAG(IS_BLA)`, `#if BUILDFLAG(USE_FOO)`).

Note that at some point we may also want to support a build configuration where
`is_cobalt=false` and `use_starboard_media=true` to support the Starboard player
in a default Chromium build.


## Cobalt Configurations

Cobalt supports four configurations, which are summarized below from the source
of truth in
[gn.py](https://source.corp.google.com/h/github/youtube/cobalt/+/main:cobalt/build/gn.py;l=26).

Cobalt flavor | is_debug                         | is_official_build                | Purpose
------------- | -------------------------------- | -------------------------------- | -------
debug         | <p style='color: green'>true</p> | <p style='color: red'>false</p>  | All symbols, no optimizations, doesn't run tests in CI
devel         | <p style='color: red'>false</p>  | <p style='color: red'>false</p>  | All symbols, partial optimization, runs unit tests in CI
qa            | <p style='color: red'>false</p>  | <p style='color: green'>true</p> | Symbols kept but stripped, full optimization
gold          | <p style='color: red'>false</p>  | <p style='color: green'>true</p> | Release

## Creating a New Build Variable

Before making a new build-time configuration, consider if it should instead be a
[runtime
feature](https://chromium.googlesource.com/chromium/src/+/main/docs/configuration.md#Features).
Features allow more ability to A/B test the setting, while build-time
configurations may require us to expand our build and CI matrix, which is
costly.

### GN

GN flags should generally be as localized as possible. If the flag only needs to
be used in a single `BUILD.gn` file, it should be declared via a
`declare_args()` block in that file directly. If it needs to be shared among
multiple files, it should be in a `declare_args()` block of a gni file that can
be imported into the files that use it as necessary. Variables should almost
never be added directly to the `BUILDCONFIG.gn` file (and similarly the
`BUILDCONFIG.gn` file should almost never import a gni file) as that would make
the variable global.

Our naming conventions follow the guidelines set forth by Chromium in the
[GN style guide](https://gn.googlesource.com/gn/+/main/docs/style_guide.md#naming-conventions).

### C++

Flags should be made as local as possible; if a flag is only needed in
Cobalt/Starboard-owned code, follow the section directly below. New flags that
need to be used in upstream code should be added very sparingly as we already
have a number of them to guard Cobalt and Starboard customizations to the code.
If one is needed, follow the section
[Needed in Upstream Code](#needed-in-upstream-code) below.

#### Local to Cobalt/Starboard Code

Local flags should use Chromium's
[buildflag_header template](https://source.corp.google.com/h/github/youtube/cobalt/+/main:build/buildflag_header.gni;l=82?q=buildflag_header&sq=&ss=h%2Fgithub%2Fyoutube%2Fcobalt%2F%2B%2Frefs%2Fheads%2Fmain;drc=fd2b36a1fad77d113ffaba1cf35f988d860d70d9):

```gn
# //cobalt/foo/BUILD.gn
import("//build/buildflag_header.gni")

static_library("foo") {
  sources = [ "foo.cc" ]
  deps = [ ":foo_buildflags" ]
}

buildflag_header("foo_buildflags") {
  header = "foo_buildflags.h"

  flags = [
    This uses the GN build flag enable_doom_melon as the definition.
    "ENABLE_DOOM_MELON=$enable_doom_melon",
  ]
}
```

```c++
// foo.cc
#include "cobalt/foo/foo_buildflags.h"

#if BUILDFLAG(ENABLE_DOOM_MELON)
...
#endif
```

#### Needed in Upstream Code

All Cobalt/Starboard C++ buildflags that are used in Chromium code live in
`//build/build_config.h`. Future flags should be added here, and should
correspond to a GN variable of the same (or similar) name. To make a GN variable
`variable_name` into a C++ buildflag, you need to define it globally for the
build. This is usually done in `//cobalt/build/configs/BUILD.gn`
([link](https://source.corp.google.com/h/github/youtube/cobalt/+/main:cobalt/build/configs/BUILD.gn;l=65)).
You would then add a `BUILDFLAG` define in `//build/build_config.h`:

```gn
# cobalt/build/configs/BUILD.gn
config("buildflag_defines") {
  ...
  if (variable_name) {
    defines += [ "ENABLE_BUILDFLAG_VARIABLE_NAME" ]
  }
}
```

```c++
// build/build_config.h
#if defined(ENABLE_BUILDFLAG_VARIABLE_NAME)
#define BUILDFLAG_INTERNAL_VARIABLE_NAME() (1)
#else
#define BUILDFLAG_INTERNAL_VARIABLE_NAME() (0)
#endif
```
