# Stub to Platform GN Migration

This document outlines a step by step process for converting the stub platform's
GN files to GN files that will be able to be built for your platform. It assumes
you have an already working port of Starboard using GYP.

## Steps to Migrate Stub Files to your platform's GN Files

This is **one** way for migrating your platform from GYP to GN. The benefit of
following this is that you can have regular checkpoints to see if your migration
is going correctly, rather than trying to do the entire migration at once where
it's uncertain how much progress is being made. \
Here are the steps to do your migration:

1.  [Copy stub files over to your platform and build them](#copy-stub-files-over-to-your-platform-and-build-them).
2.  [Replace stub toolchain with your platform's toolchain](#replace-stub-toolchain-with-your-platforms-toolchain).
3.  [Replace stub configuration with your platform's configuration](#replace-stub-configuration-with-your-platforms-configuration).
4.  [Replace stubbed starboard_platform target sources with your platform's
    sources](#replace-stubbed-starboardplatform-sources-with-your-platforms-sources).

After each step, you should be able to build the starboard_platform target.
For example, you would build raspi2 starboard_platform target with the following
commands:
```
$gn gen out/raspi-2gn_devel --args='target_platform="raspi-2" build_type="devel"'
$ninja -C out/raspi-2gn_devel/ starboard
```

### Copy Stub Files Over to Your Platform and Build Them

Here is a list of steps outlining which files to copy over and how to build
those files:

1.  Copy over files from the stub implementation to the platform folder. This
    list gives you an example of which files to copy over for your platform.
    This is an example for files to be copied over for your platform's port at
    starboard/YOUR_PLATFORM
    *   starboard/stub/BUILD.gn > starboard/YOUR_PLATFORM/BUILD.gn
    *   starboard/stub/platform_configuration/BUILD.gn >
        starboard/YOUR_PLATFORM/platform_configuration/BUILD.gn
    *   starboard/stub/platform_configuration/configuration.gni >
        starboard/YOUR_PLATFORM/platform_configuration/configuration.gni
    *   starboard/stub/toolchain/BUILD.gn >
        starboard/YOUR_PLATFORM/toolchain/BUILD.gn
2.  Add your platform path to starboard/build/platforms.gni as referenced
    [here](../migrating_gyp_to_gn.md#adding-your-platform-to-starboard)
3.  Resolve any errors which come up for missing/incorrect file paths. Then, you
    should be able to build your platform target with the stubbed out files
    suggested in the above section.

### Replace Stub Toolchain with Your Platform's Toolchain

Follow instructions [here](../migrating_gyp_to_gn.md#migrating-a-toolchain) for
migrating the toolchain. Resolve errors and build the starboard_platform target
with the stubbed files.

### Replace Stub Configuration with Your Platform's Configuration

This involves migrating the compiler flags and build variables as referenced
[here](../migrating_gyp_to_gn.md#migrating-a-platform).

> **Highly recommended** \
> It’s good to turn off the `treat_warnings_as_errors flag` until you can compile
> the starboard_platform target with the platform files.
> If this flag is not disabled you might run into a lot of
> warnings turned errors and it might take time to solve all those errors.
> Meanwhile you won't be in a buildable state which might make it uncertain as to
> how much progress you are actually making.
> For disabling the flag you can pass that as an argument to gn.
> Here's an example for disabling the flag for raspi2:
> ```
> $gn gen out/raspi-2gn_devel --args='target_platform="raspi-2" build_type="devel" treat_warnings_as_errors=false'
> ```

Resolve errors and build the starboard_platform target with the stubbed files.

### Replace Stubbed starboard_platform Sources with Your Platform's Sources

This involves adding files for the starboard_platform target as suggested
[here](../migrating_gyp_to_gn.md#migrating-a-platform).

While building any target, follow the recommendation above of building the
target with `treat_warnings_as_errors=false`.

Once you can build your platform files, you can remove the
`treat_warnings_as_errors=false` flag and resolve the warning errors.

## FAQ

1.  **I’m getting a build error! What should I do?** \
    Some common questions to ask yourself:

    *   Is the same target building with GYP + ninja (as GN + Ninja)?

        > For example if the `nplb` target is not being built by GN, check first
        > if it can be built with GYP. If GYP cannot build it, this indicates
        > that some flags are missing in GYP itself so it might be prudent to
        > solve that first before migrating to GN.

    *   Am I missing a config/dependency to include the missing file?

        > [gn check](https://cobalt.googlesource.com/third_party/gn/+/refs/heads/main/docs/reference.md#cmd_check)
        > can help point out missing dependencies.

    *   Is the same file being included in the build by GYP?

        > Add a preprocessor directive like #error "This file is included" in
        > that file and see if GYP + Ninja prints out that error message.

    *   Is the same code path being followed by GYP + ninja ?

        > Use the same method as above.

    *   Are the compiler flags for this file the same as in GYP ?

        > To compare flags for GYP vs GN refer
        > [section](../migrating_gyp_to_gn.md#validating-a-target). To check if
        > the variables/flags you are compiling have changed since GYP, refer
        > [page](../migration_changes.md).

    *   Have you passed in the default arguments for your platform correctly?

        > Default variables such as `target_cpu`, `target_os` and others can be
        > overridden by passing arguments to gn while building. Here's an
        > example of passing the default argument `target_cpu` for raspi2:
        > ```
        > $gn gen out/raspi-2gn_devel --args='target_platform="raspi-2" build_type="devel" target_cpu="arm"'
        > ```
