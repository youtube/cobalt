# Starboard

Starboard is Cobalt's porting layer and OS abstraction. It attempts to encompass
all the platform-specific functionality that Cobalt actually uses, and nothing
that it does not.


## Current State

Starboard is still very much in development, and does not currently run
Cobalt. Mainly, it runs NPLB (see below), Chromium base, Chromium net, ICU,
GTest, GMock, and only on Linux.

The work so far has focused on supporting Chromium base and net, and their
dependencies, so the APIs for things like media decoding, decryption, DRM, and
graphics haven't yet been defined.


## Interesting Source Locations

All source locations are specified relative to `src/starboard/` (this directory).

  * `.` - This is the root directory for the Starboard project, and contains all
    the public headers that Starboard defines.
  * `examples/` - Example code demonstrating various aspects of Starboard API
    usage.
  * `linux/` - The home of the Linux Starboard implementation. This contains a
    `starboard_platform.gyp` file that defines a library with all the source
    files needed to provide a complete Starboard Linux implementation. Source
    files that are specific to Linux are in this directory, whereas shared
    implementations are pulled from the shared directory.
  * `nplb/` - "No Platform Left Behind:" Starboard's platform verification test
    suite.
  * `shared/` - The home of all code that can be shared between Starboard
    implementations. Subdirectories delimit code that can be shared between
    platforms that share some facet of their OS API.


## Building with Starboard

Follow the Cobalt instructions, except when invoking gyp:

    $ cobalt/build/gyp_cobalt -C Debug starboard_linux

and when invoking ninja:

    $ ninja -C out/SbLinux_Debug cobalt


## Quick Guide to Starting a Port

### I. Enumerate and Name Your Platform Configurations

Before starting a Cobalt/Starboard port, first you will need to define the
canonical names for your set of platform configurations. These will be used when
organizing the code for your platforms.

What determines what goes into one platform configuration versus another? A
platform configuration has a one-to-one mapping to a production binary. So, if
you will need to produce a new binary, you are going to need a new platform
configuration for that.

The recommended naming convention for a `<platform-configuration>` is:

    <family-name>_<binary-variant>

Where `<family-name>` is a name specific to the family of products you are
porting to Starboard and `<binary-variant>` is one or more tokens that uniquely
describe the specifics of the binary you want that configuration to produce.

For example, let's say your company is named BobCo. BobCo employs multiple
different device architectures so it will need to define multiple platform
configurations.

All the BobCo devices are called BobBox, so it's a reasonable choice as a
product `<family-name>`. But they have both big- and little-endian MIPS
chips. So they might define two platform configurations:

  #. `bobbox_mipseb` - For big-endian MIPS devices.
  #. `bobbox_mipsel` - For little-endian MIPS devices.


### II. Choose a Location in the Source Tree for Your Starboard Port

To be perfectly compatible with the Cobalt source tree layout, any code that is
written by a party that isn't the Cobalt team should be in the
`src/third_party/` directory. The choice is up to you, but we recommend that you
follow this practice, even if, as we expect to be common, you do not plan on
sharing your Starboard implementation with anyone.

Primarily, following this convention ensures that no future changes to Cobalt or
Starboard will conflict with your source code additions. Starboard is intended
to be a junction where new Cobalt versions or Starboard implementations can be
replaced without significant (and hopefully, any) code changes.

We recommend that you place your code here in the source tree:

    src/third_party/starboard/<family-name>/

With subdirectories:

  * `shared/` - For code shared between architectures within a product family.
  * `<binary-variant>/` - For any code that is specific to a specific binary
    variant. Each one of these will likely at least have a
    `configuration_public.h` file and a `starboard_platform.gypi` file.

In the BobCo's BobBox example, we would see something like:

  * `src/third_party/starboard/bobbox/`
    * `shared/`
    * `mipseb/`
      * `configuration_public.h`
      * `starboard_platform.gypi`
    * `mipsel/`
      * `configuration_public.h`
      * `starboard_platform.gypi`

And so on.

#### Forward Your GYP

Starboard looks in a particular place for the GYP file that builds an
implementation, and for some header files. But you should just forward these
files to the location that your Starboard implementation will live.

You need to add `src/starboard/<platform-configuration>/starboard_platform.gyp`
for every platform configuration you need, but it should just include the
associated
`src/third_party/starboard/<family-name>/<binary-variant>/starboard_platform.gypi`
file like this:

```python
{
  'includes': [
    '../../third_party/starboard/bobbox/mipseb/starboard_platform.gypi',
  ],
}
```

Note there is no trailing comma on the outer dict, GYP will complain with a very
strange error if you include it.

#### Forward Your Includes

You should also add `configuration_public.h`, `atomic_public.h`, and
`thread_types_public.h`, files to the `src/starboard/<platform-configuration>/`
configuration. These can just contain a single line that includes the real files
in `third_party`, for an example with BobBox and configuration_public.h:

```c++
#include "third_party/starboard/bobbox/mipseb/configuration_public.h"
```

You might further want to forward these public platform includes to the
`shared/` directory, if they are common to all binary variants. Thread types
will probably be common, Atomics may be, and some portion of your configuration
will almost certainly be specific to the target architecture.


### III. Base Your Port on a Reference Port

If your device runs Linux, you should start off by copying the Linux-specific
files from `src/starboard/linux/...` to your port's location.

Rename the `x86/` directory to `<binary-variant>` (e.g. `mipseb`).

Modify `x86/configuration_public.h` and `shared/configuration_public.h` as
appropriate (you will probably be coming back to these files a lot).

Update `<binary-variant>/starboard_platform.gypi` to point at all the source
files that you want to build as your new Starboard implementation. `'<(DEPTH)'`
expands to enough `../` to take you to the `src/` directory of your source tree.


### IV. Add Your Platform Configurations to cobalt_gyp

In order to use a new platform configuration in a build, you will first need to
add it to the `cobalt_gyp` script, so it will generate ninja files for you.

  #. Add your platform configuration name to the `VALID_PLATFORMS` array in
     `src/cobalt/build/config/base.py`.
  #. Set up `<platform-configuration>.py`
    #. In `src/cobalt/build/config/`, copy `starboard_linux.py` to
       `<platform-configuration>.py`.
    #. In `src/cobalt/build/config/<platform-configuration>.py`
      #. In the `_PlatformConfig.__init__()` function, remove checks for Clang
         or GOMA.
      #. Again in the `_PlatformConfig.__init__()` function, pass your
         `<platform-configuration>` into `super.__init__()`, like
         `super.__init__('bobbox_mipseb')`.
      #. In `GetPlatformFullName`, have it return a name, like `BobBoxMIPSEB`.
         This name will be used in your output directory,
         e.g. `BobBoxMIPSEB_Debug`, and must be the name of your build
         configurations defined in your build configuration GYPI file.
      #. In `GetVariables`
        #. Set `'clang': 1` if your toolchain is clang.
        #. Delete other variables in that function that are not needed for your
           platform.
      #. In `GetEnvironmentVariables`, set the dictionary values to point to the
         toolchain analogs for the toolchain for your platform.
  #. Set up `<platform-configuration>.gypi`
    #. In `src/cobalt/build/config/`, copy `starboard_linux.gypi` to
       `<platform-configuration>.gypi`.
    #. Update your platform variables.
      #. Set `'target_arch'` to your `<platform-configuration>` name. It's not
         really named correctly.
      #. Set `'gl_type': 'system'` if you are using the system EGL + GLES2
         implementation.
      #. set `'in_app_dial'` to `1` or `0`. This enables or disables the DIAL
         server that runs inside Cobalt, only when Coblat is running. You do not
         want in-app DIAL if you already have system-wide DIAL support.
    #. Update your toolchain command-line flags and libraries. Make sure you
       don't assume a particular workstation layout, as it is likely to be
       different for someone else.
    #. Update the global defines in `'target_defaults'.'defines'`, if necessary.
    #. Replace "SbLinux" with the PlatformFullName you chose above.


You should now be able to run gyp with your new port. From your `src/` directory:

    $ cobalt/build/gyp_cobalt -C Debug bobbox_mipseb
    $ ninja -C out/BobBoxMIPSEB_Debug nplb

This will attempt to build the "No Platform Left Behind" test suite with your
new Starboard implementation, and you are ready to start porting!
