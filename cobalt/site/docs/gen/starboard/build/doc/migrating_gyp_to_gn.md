---
layout: doc
title: "Migrating GYP to GN"
---
# Migrating GYP to GN

Cobalt is currently in the process of migrating from our current build system
which uses [GYP][gyp_home] to an updated one with
[Generate Ninja (GN)][gn_home]. This allows us to remove our dependencies on
Python 2.

## Getting Started with GN

### Getting the Binary

There are a few ways to get a binary. Follow the instructions for whichever way
you prefer [here][gn_getting_a_binary].

### Read the Docs

Most of the documentation for GN is located [here][gn_doc_home].

*   For a hands-on example with GN, check out the
    [Quick Start Guide][gn_quick_start] and walk through the example.

*   To learn more about the language and coding in it, read through the
    [Language page][gn_language] and the [Style Guide][gn_style_guide].

*   For a full reference of the language, run `gn help` or use the
    [Reference page][gn_reference].

If you're familiar with GYP but not with GN, it may be helpful to read
Chromium's [GYP->GN Conversion Cookbook][gyp_to_gn_cookbook]. Keep in mind we
don't want to follow everything that's recommended here—much of the advice is
specific to Chromium. In particular:

*   Whenever possible, avoid using a `source_set`. Instead, use a
    `static_library`.
*   Many of the flags under [Variable Mappings][gyp_to_gn_variable_mappings] do
    not apply to Cobalt.
*   Cobalt code is not Chromium code by default, so you can ignore
    [that section][gyp_to_gn_chromium_code].

### Know the Tools

The flow of GN is fairly similar to that of GYP: you'll configure a build then
actually build it (using ninja). Here's how to build `nplb` target for
`stub_debug`:

```
$ gn gen out/stub_debug --args='target_platform="stub" build_type="debug"'
$ ninja -C out/stub_debug nplb
```

You can change the directory argument to `gn gen` (`out/stub_debug`) if you'd
like; unlike GYP, we can put the build root wherever we want.

There are some additional important tools: `gn format`, which is a code
formatter for GN, and `gn check`, which checks that build dependencies are
correctly set. See the documentation for [gn format][gn_format_tool] and
[gn check][gn_check_tool] for how to use both. The full list of commands GN
supports can be found on the [reference page][gn_reference].

## Migrating a Single Target

GYP and GN are very similar within the scope of a single target. The GYP->GN
Conversion Cookbook is a good reference for this, particularly
[this section][gyp_to_gn_typical_modifications]. The GYP syntax stays very
similar in general, though configuration will differ: in GN, you should create a
`config` targets and have your target add that to their list of configs:

```
config("foo_config") {
  cflags = ...
}

static_library("foo") {
  sources = ...
  deps = ...

  configs += [ ":foo_config" ]
}
```

You also may need to remove default configs. The default configs are listed in
[BUILDCONFIG.gn](../config/BUILDCONFIG.gn). You remove a config like so:

```
static_library("foo") {
  configs -= [ "//full/path/to/config:config_name" ]
}
```

## Migrating a Platform

When porting your platform with GYP following
[the porting guide][cobalt_porting_guide], we expected a few build files to be
present under your starboard path:

*   `gyp_configuration.py`
*   `gyp_configuration.gypi`
*   `starboard_platform.gyp`

These contain your toolchain, your compilation flags, your platform-specific
build variables, and your definition of the `starboard_platform` target. This
maps to the GN files needed to port your platform:

*   Your toolchain: `toolchain/BUILD.gn`
*   Your compilation flags: `platform_configuration/BUILD.gn`
*   Your platform-specific build variables:
    `platform_configuration/configuration.gni`
*   Your definition of the `starboard_platform` target: `BUILD.gn`

Some of these files need to define certain targets:

*   `toolchain/BUILD.gn`

    The toolchain implementation is discussed in more detail
    [below](#migrating-a-toolchain).

    ```
    toolchain("host") {
      ...
    }

    toolchain("target") {
      ...
    }
    ```

*   `platform_configuration/BUILD.gn`

    ```
    config("platform_configuration") {
      # Set the flags that were in gyp_configuration.gypi.
    }
    ```

*   `BUILD.gn`

    ```
    static_library("starboard_platform") {
      # Largely the same as the current starboard_platform.gyp.
    }
    ```

### Adding Your Platform to Starboard

Instead of implicitly searching directories for certain files like GYP did, we
explicitly enumerate our ports and their locations.
[platforms.gni](../platforms.gni) contains all of this information, and you'll
need to add your platform to that list following the same format.

### Migrating a Family of Platforms

Cobalt's reference platforms when implemented in GYP mainly used variables to
share sources and dependencies. In GN, we prefer putting shared sources and
dependencies in a static_library that we depend on in the final
`starboard_platform` `static_library` target. This means that configurations to
particular files should be in the same `static_library` that files are in.

### Migrating a Toolchain

Cobalt expects you to set a target and a host toolchain for your build like so:

```
toolchain("host") {
  ...
}

toolchain("target") {
  ...
}
```

You may define a toolchain from scratch following the [reference][gn_toolchain],
or you can use the
[gcc/clang templates](../../../build/toolchain/gcc_toolchain.gni) provided.
Almost all of the reference platforms use these templates, so look to those as
examples for how to use it correctly. Here's the linux-x64x11
[toolchain/BUILD.gn file](../../linux/x64x11/toolchain/BUILD.gn).

## Checking Your Migration

There are a few different ways to ensure you've migrated a target successfully.
You'll of course want to make sure you can build things after you've migrated
them.

### Validating a Target

If you're migrating a single target, it's simple to check: just configure the
build using the the necessary arguments then build that target with `ninja`,
i.e.:

```
static_library("new_target") { ... }
```

```
$ gn gen out/stub_debug --args='target_platform="stub" build_type="debug"'
$ gn check out/stub_debug
$ ninja -C out/stub_debug new_target
```

If this was equivalent to a GYP target, you can compare the ninja compilation
databases by using [format_ninja.py](../../../tools/format_ninja.py) and a
comparison tool, i.e. [meld](https://meldmerge.org/). This will allow you to see
any changes in commands, i.e. with flags or otherwise.

The following differences for ninja flags between GYP and GN don't cause any
issues:

1. The name of the intermediate .o, .d files is different in both cases: Here is
   an example while compiling the same source file
   ```
   starboard/common/new.cc
   ```
   GYP generates:
   ```
   obj/starboard/common/common.new.cc.o
   ```
   GN generates:
   ```
   obj/starboard/common/common/new.o
   ```
2. The `-x` flag for specifying language is not present in GN migration.
   For example GYP specifies `-x c` flag while building c language files for
   certain targets. This flag is not specified while building any GN targets.

### Validating a Platform

Checking that an entire platform has been migrated successfully is slightly more
involved. It can be easiest to start by copying provided stub implementation and
continuously migrating it over, checking that it builds as you go along. If you
don't go this route, you can instead start by building a small target (with few
dependencies) then work your way up to building the `all` target.

You can use the same comparison method of using `format_ninja.py` as discussed
[in the section above](#validating-a-target).

### Step by Step Stub to Your Platform Migration Guide

This [document](../gn_migrate_stub_to_platform.md) outlines a step by step
process for converting the stub platform's GN files to GN files that will be
able to be built for your platform.

[cobalt_porting_guide]: https://cobalt.dev/starboard/porting.html
[gn_check_tool]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/reference.md#cmd_check
[gn_doc_home]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs
[gn_format_tool]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/reference.md#cmd_format
[gn_getting_a_binary]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/#getting-a-binary
[gn_home]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/
[gn_language]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/language.md
[gn_reference]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/reference.md
[gn_style_guide]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/style_guide.md
[gn_toolchain]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/reference.md#func_toolchain
[gn_quick_start]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/quick_start.md
[gyp_home]: https://gyp.gsrc.io/index.md
[gyp_to_gn_chromium_code]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/cookbook.md#chromium-code
[gyp_to_gn_cookbook]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/cookbook.md
[gyp_to_gn_typical_modifications]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/cookbook.md#typical-sources-and-deps-modifications
[gyp_to_gn_variable_mappings]: https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/cookbook.md#variable-mappings
