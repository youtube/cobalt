# GYP → GN Conversion Cookbook (Cobalt Edition)

[TOC]

## Foreword

Read the GN docs to familiarize yourself with the GN build system. I recommend
reading at least our [Quick Start][quick-start] guide, and skimming
the [Language document][language] and [Style Guide][style-guide] too.
The [GN and GYP Patterns][gn-gyp-patterns] document has some useful info too,
although not all of it is applicable to Cobalt.

Also read [Chromium's GYP → GN conversion cookbook][chromium-cookbook] if you
haven't already. The guidelines in that cookbook generally apply here. This
document consists of Cobalt-specific addenda to that document.

If you are uncertain how a certain idiom is expressed in GN, I suggest
consulting the Chromium code to see how they did it. Not all of Chromium's GN
idioms are adaptable to Cobalt (primarily because Chromium has no Starboard
concept), but it is a good place to start.

[quick-start]: https://cobalt.googlesource.com/cobalt/+/master/src/cobalt/doc/gn_quick_start.md
[language]: https://chromium.googlesource.com/chromium/src/+/master/tools/gn/docs/language.md
[style-guide]: https://chromium.googlesource.com/chromium/src/+/master/tools/gn/docs/style_guide.md
[gn-gyp-patterns]: https://docs.google.com/document/d/1xuInfaOjQQ00mtzaTPfiLH-Hw1wlxfH-60jHoa921Lc/edit
[chromium-cookbook]: https://chromium.googlesource.com/experimental/chromium/src/+/refs/wip/bajones/webvr/tools/gn/docs/cookbook.md

## gyp_to_gn.py

There is a script to partially automate the GYP to GN conversion in
`cobalt/tools/gyp_to_gn.py`. To run it, use:

    cobalt/tools/gyp_to_gn.py path/to/module/module.gyp | gn format --stdin > path/to/module/BUILD.gn

This script is able to produce reasonably close GN output for many GYP files.
However, it is not perfect, and some manual inspection and revision of the
output is often necessary. As of this writing, known issues in addition to the
ones the script warns about are:

1. It doesn't translate over comments. This is because the script uses `eval`
   to load the GYP file, which naturally cuts out all of the comments.
1. Sometimes the script will output `sources =` when it should have outputted
   `sources +=`. This is because the GYP file had `'sources':` in both places,
   but one of those places was inside a `conditions` block or similar. This bug
   can be fixed, but it will take a little work.

   The same problem can happen with other variables besides `sources`, e.g.
   `defines`, `deps`, etc.

The script relies on two helper files in its directory, `variable_rewrites.dict`
and `deps_substitutions.txt`. `variable_rewrites.dict` is a Python dictionary
containing variables which were renamed or changed into booleans.
`deps_substitutions.txt` is a tab-delimited file containing a (non-exhaustive)
list of targets which were renamed.

## File Correspondences

`gyp_gn_files.md` in this directory contains a list of GYP -> GN file
correspondences. If you create a new irregular file correspondence, please add
it to this list before you forget!

## Variables vs Build Args

GN distinguishes between variables and build args. Build args are parameters
declared inside a `declare_args` block, that are intended for developers to
specify at the time they run gn. Ordinary variables are specified outside of
`declare_args` blocks, and they are intended to not change from build to build.

An example of a build arg is `use_goma`; if this build arg is turned on,
compilation is done with Goma instead of locally. (As of this writing,
`use_goma` is by default on for stub and linux platforms, and off on the
others.) An example of a variable is `gl_type`; this is something which depends
on the Starboard platform, and it doesn't make sense for individual developers
to change the value of this variable at compile time.

Some build behaviors controlled by environment variables in GYP been refactored
into GN build args. Here is a list of correspondences:

GYP            | GN                     | Defined in
-------------- | ---------------------- | -----------------------------------------
`LB_FASTBUILD` | `cobalt_use_fastbuild` | `//starboard/build/config/fastbuild.gni`
`USE_ASAN`     | `use_asan`             | `//starboard/build/config/sanitizers.gni`
`USE_TSAN`     | `use_tsan`             | `//starboard/build/config/sanitizers.gni`

## Feature Flags

Most variables defined in `cobalt/build/config/base.gypi` and
`build/common.gypi` have been moved to `cobalt/build/config/base.gni` or
`starboard/build/config/base.gni`, depending on whether they are Cobalt or
Starboard variables. (A few, like `cobalt_config` and `starboard_path`, have
been moved to `BUILDCONFIG.gn`, and others may have been moved to other places
as well.)

Some variables have not been copied over. Most commonly this is because the
variable in question has been replaced by a config, group or something in GN
more appropriate.

Also, since GN has a true boolean type, variables which took on 0/1 values in
GYP have been converted to take on true/false values in GN.

Variables in Starboard platforms' `gyp_configuration.gypi` file have been moved
to `configuration.gni` under the Starboard platform's directory.
`base.gni` automatically imports the `configuration.gni` file of the
right Starboard platform.

GN doesn't allow defining a build arg twice with different values or changing
the value of a variable/build arg by importing a file. It also has no analog to
GYP's %-variable. Unfortunately for us, this means the code of `base.gni`
must necessarily check whether every variable/build arg it defines has already
been defined by the Starboard platform in `configuration.gni`.

Chromium frowns upon files with too many variables or build arg definitions.
They prefer to keep such definitions close to the files which actually use them.
In contrast, we keep a single `base.gni` file, because it would be inconvenient
for porters to have to browse through dozens of files to find out which variables
they should override.

## Build Args with Platform-Specific Defaults

There are some build args, like `use_goma`, which have different default values
on different Starboard platforms. Here is the generic declaration of `use_goma`,
located in `//starboard/shared/toolchain/goma.gni`:

    if (!defined(use_goma)) {
      # Set to true to enable distributed compilation using Goma. By default
      # we use Goma for stub and linux.
      use_goma = false
    }

Notice a few things:

1. As with feature flags, `goma.gni` first checks to see if the Starboard
   platform configuration has already defined it (with a different default)
   in `configuration.gni`.
2. The comment is located right above the variable, inside the if statement.
   This looks weird, but it's intentionally done this way so that the comment
   will be printed out in `gn args --list`.
3. Starboard platforms which define alternative defaults are encouraged to
   copy the comment when making their own definition of `use_goma`. (See the
   stub platform configuration for an example.) It's not strictly necessary, but
   if it's not done, then `gn args --list` won't print any documentation for
   this build arg.

## BUILDCONFIG Variables

Some things which must necessarily be defined in `BUILDCONFIG.gn` may depend on
values from the Starboard platform, like:

 * The `target_os` and `target_cpu`
 * The `test_target_type`, and `final_executable_type` (which correspond to the
   GYP variables `gtest_target_type` and `final_executable_type`)
 * The host and target toolchain

These values are obtained from the Starboard platform's `buildconfig.gni`
file.

## Generic Compiler Options

In GYP, we have variables like `sb_pedantic_warnings` and keys like `rtti`,
which, if set for a target, turn on or off additional compiler flags. In GN,
these variables have been refactored into configs.

Consult the following examples:

### Pedantic Warnings

GYP:

    'variables': {
      'sb_pedantic_warnings': 1
    }

GN:

    configs -= [ "//starboard/build/config:no_pedantic_warnings" ]
    configs += [ "//starboard/build/config:pedantic_warnings" ]

### Optimizations

GYP:

    'optimizations': 'debuggable'

GN:

    configs -= [ "//starboard/build/config:default_optimizations" ]
    configs += [ "//starboard/build/config:debuggable_optimizations" ]

Ditto for `none` and `full`.

### RTTI

GYP:

    'rtti': 1

GN:

    configs -= [ "//starboard/build/config:default_rtti" ]
    configs += [ "//starboard/build/config:rtti" ]

GYP:

    'rtti': 0

GN:

    configs -= [ "//starboard/build/config:default_rtti" ]
    configs += [ "//starboard/build/config:no_rtti" ]

### -Wexit-time-destructors

GYP:

    'enable_wexit_time_destructors': 1

GN:

    configs += [ "//starboard/build/config:wexit_time_destructors" ]

There is no `//starboard/build/config:no_wexit_time_destructors` config.

### Implementation Details

Each Starboard generic compiler config is implemented by referencing a
platform-dependent implementation as a subconfig. For instance, here is, in
essence, the code for `//starboard/build/config:pedantic_warnings`:

    config("pedantic_warnings") {
        configs = [ "//$starboard_path/config:pedantic_warnings" ]
    }

Each Starboard platform is then expected to define a pedantic warnings config
(and a no-pedantic-warnings config) in its `BUILD.gn` file. For instance, the
stub platform's implementation is:

    config("pedantic_warnings") {
      cflags = [
        "-Wall",
        "-Wextra",
        "-Wunreachable-code",
      ]
    }

Similarly, each Starboard platform is expected to define configs for turning on
and off RTTI, or what the default RTTI state should be; configs for turning on
and off optimizations, and the like. In reality, Starboard platforms often in
turn delegate to configs implemented by toolchains (which stub does for e.g.
RTTI).

## Variables Renamed

In general, following Chromium's practice, booleans have been renamed to have a
`use_`, `enable_` or `is_` prefix, if they did not already have such a prefix.
These prefixes come after any `cobalt_` or `sb_` prefix.

Here is a partial table of some renames:

GYP                         | GN
--------------------------- | --------------------------
`cobalt_fastbuild`          | `cobalt_use_fastbuild`
`cobalt_version`            | `cobalt_build_id`
`sb_allows_memory_tracking` | `sb_allow_memory_tracking`
`target_arch`               | `target_cpu`


## Table of Variables Refactored into Configs

Variable                                              | Config
----------------------------------------------------- | --------------------------------------------------------------
`sb_pedantic_warnings`                                | `//starboard/build/config:{no_}pedantic_warnings`
`compiler_flags`, `linker_flags`                      | `//$starboard_path:compiler_defaults`
`compiler_flags_debug`, `compiler_flags_c_debug`, ... | `//$starboard_path:compiler_defaults_debug`
`compiler_flags_devel`, `compiler_flags_c_devel`, ... | `//$starboard_path:compiler_defaults_devel`
`compiler_flags_qa`, `compiler_flags_c_qa`, ...       | `//$starboard_path:compiler_defaults_qa`
`compiler_flags_gold`, `compiler_flags_c_gold`, ...   | `//$starboard_path:compiler_defaults_gold`
`compiler_flags_host`, `compiler_flags_c_host`, ...   | `//$starboard_path:compiler_defaults($host_toolchain)`
`platform_libraries`                                  | `//$starboard_path:compiler_defaults` (in the `libs` variable)

## gtest_target_type and friends

The `gtest_target_type` variable has been renamed `test_target_type`. Platforms
wishing to override the default value of this variable (e.g. Android) should
put the override in `buildconfig.gni`. `final_executable_type` is similar.

To use these types in targets, consult the following conversion table:

GYP                                                  | GN
---------------------------------------------------- | ---------------------------
`'type': '<(gtest_target_type)', 'name': 'foo',`     | `test("foo") {`
`'type': '<(final_executable_type)', 'name': 'foo',` | `final_executable("foo") {`

## Deploy

Replace

    {
      'target_name': 'target_deploy',
      'type': 'none',
      'dependencies': [
        'target',
      ],
      'variables': {
        'executable_name': 'target',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

with

    deploy("deploy") {
      executable_name = "target"

      deps = [
        ":target",
      ]
    }

and import `//starboard/build/deploy.gni` at the top of the file.

## Runner Script

The GYP build system has a runner script, `gyp_cobalt`, which does a ton of
preprocessing, then runs GYP four times, once for each Cobalt configuration.

GN does not use a runner script. The preprocessing that `gyp_cobalt` does is
being moved into the actual GN build itself, making a runner script mostly
unnecessary. Furthermore, a nontrivial runner script would potentially interfere
with Ninja correctly rerunning GN when GN build files have changed.

## Generic GN Advice for Build Args

If you're adding a flag inside a `declare_args` block, read these tidbits of
advice:

 * Use boolean values when possible. If you need a default value that expands
   to some complex thing in the default case (like the location of the
   compiler which would be computed by a script), use a default value of -1 or
   the empty string. Outside of the `declare_args` block, conditionally expand
   the default value as necessary.

 * Use a name like `use_foo` or `is_foo` (whatever is more appropriate for
   your feature) rather than just `foo`.

 * Write good comments directly above the declaration with no blank line.
   These comments will appear as documentation in `gn args --list`.

 * Don't call `exec_script` inside `declare_args`. This will execute the script
   even if the value is overridden, which is wasteful. See first bullet.
