# Starboard

Starboard is Cobalt's porting layer and OS abstraction. It attempts to encompass
all the platform-specific functionality that Cobalt actually uses, and nothing
that it does not.


## GN Migration Notice

Cobalt and Starboard are currently migrating from the GYP build system to the GN
build system. This readme contains instructions for both systems. As of now, GN
is not ready for general use for Cobalt/Starboard. If you are not a core Cobalt
developer, you should probably ignore the sections titled "GN Instructions" for
now.


## Documentation

See [`src/starboard/doc`](doc) for more detailed documentation.


## Interesting Source Locations

All source locations are specified relative to `src/starboard/` (this directory).

  * [`.`](.) - This is the root directory for the Starboard project, and
    contains all the public headers that Starboard defines.
  * [`examples/`](examples) - Example code demonstrating various aspects of
    Starboard API usage.
  * [`stub/`](stub) - The home of the Stub Starboard implementation. This
    contains a `starboard_platform.gyp` file that defines a library with all the
    source files needed to provide a complete linkable Starboard implementation.
  * [`nplb/`](nplb) - "No Platform Left Behind," Starboard's platform
    verification test suite.
  * [`shared/`](shared) - The home of all code that can be shared between
    Starboard implementations. Subdirectories delimit code that can be shared
    between platforms that share some facet of their OS API.


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

    <family-name>-<binary-variant>

Where `<family-name>` is a name specific to the family of products you are
porting to Starboard and `<binary-variant>` is one or more tokens that uniquely
describe the specifics of the binary you want that configuration to produce.

For example, let's say your company is named BobCo. BobCo employs multiple
different device architectures so it will need to define multiple platform
configurations.

All the BobCo devices are called BobBox, so it's a reasonable choice as a
product `<family-name>`. But they have both big- and little-endian ARM
chips. So they might define two platform configurations:

  1. `bobbox-armeb` - For big-endian ARM devices.
  1. `bobbox-armel` - For little-endian ARM devices.


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
    variant. Each one of these must at least have `configuration_public.h`,
    `atomic_public.h`, `thread_types_public.h`, `gyp_configuration.py`,
    `gyp_configuration.gypi`, and `starboard_platform.gyp` files.

In the BobCo's BobBox example, we would see something like:

  * `src/third_party/starboard/bobbox/`
      * `shared/`
      * `armeb/`
          * `atomic_public.h`
          * `configuration_public.h`
          * `gyp_configuration.gypi`
          * `gyp_configuration.py`
          * `starboard_platform.gyp`
          * `thread_types_public.h`
      * `armel/`
          * `atomic_public.h`
          * `configuration_public.h`
          * `gyp_configuration.gypi`
          * `gyp_configuration.py`
          * `starboard_platform.gyp`
          * `thread_types_public.h`

And so on.

#### GN Instructions

Each `<binary-variant>/` directory must have at least `configuration_public.h`,
`atomic_public.h`, `thread_types_public.h`, `BUILD.gn`, `configuration.gni`,
and `buildconfig.gni`.

In the BobCo's BobBox example, we would see a directory tree like:

  * `src/third_party/starboard/bobbox/`
      * `shared/`
      * `armeb/`
          * `buildconfig.gni`
          * `configuration.gni`
          * `atomic_public.h`
          * `BUILD.gn`
          * `configuration_public.h`
          * `thread_types_public.h`
      * `armel/`
          * `buildconfig.gni`
          * `configuration.gni`
          * `atomic_public.h`
          * `BUILD.gn`
          * `configuration_public.h`
          * `thread_types_public.h`


### III. Base Your Port on a Reference Port

You can start off by copying files from a reference port to your port's
location. Currently these reference ports include:

  * `src/starboard/stub`
  * `src/starboard/linux`
  * `src/starboard/raspi`

The `starboard_platform.gyp` contains absolute paths, so the paths will still be
valid if you copy it to a new directory. You can then incrementally replace
files with new implementations as necessary.

The cleanest, simplest starting point is from the Stub reference
implementation. Nothing will work, but you should be able to compile and link it
with your toolchain. You can then replace stub implementations with
implementations from `src/starboard/shared` or your own custom implementations
module-by-module, until you have gone through all modules.

You may also choose to copy either the Desktop Linux or Raspberry Pi ports and
work backwards fixing things that don't compile or work on your platform.

For example, for `bobbox-armel`, you might do:

    mkdir -p src/third_party/starboard/bobbox
    cp -R src/starboard/stub src/third_party/starboard/bobbox/armel

Modify the files in `<binary-variant>/` as appropriate (you will probably be
coming back to these files a lot).

Update `<binary-variant>/starboard_platform.gyp` to point at all the source
files that you want to build as your new Starboard implementation. The
`'<(DEPTH)'` expression in GYP expands to enough `../`s to take you to the
`src/` directory of your source tree. Otherwise, files are assumed to be
relative to the directory the `.gyp` or `.gypi` file is in.

In order to use a new platform configuration in a build, you need to ensure that
you have a `gyp_configuration.py`, `gyp_configuration.gypi`, and
`starboard_platform.gyp` in their own directory for each binary variant, plus
the header files `configuration_public.h`, `atomic_public.h`, and
`thread_types_public.h`. `gyp_cobalt` will scan your directories for these
files, and then calculate a port name based on the directories between
`src/third_party/starboard` and your `gyp_configuration.*` files. (e.g. for
`src/third_party/starboard/bobbox/armeb/gyp_configuration.py`, it would choose
the platform configuration name `bobbox-armeb`.)

#### GN Instructions

Update `<binary-variant>/BUILD.gn` to point at all the source files that you
want to build as your new Starboard implementation. The `//` expression in GN
refers to the `src/` directory of your source tree. Otherwise, files are assumed
to be relative to the directory the `BUILD.gn` or `.gni` file is in. The
`BUILD.gn` file contains absolute paths, so the paths will still be valid if you
copy it to a new directory. You can then incrementally replace files with new
implementations as necessary.

In order to use a new platform configuration in a build, you need to ensure that
you have a `BUILD.gn`, `configuration.gni`, and `buildconfig.gni` in their
own directory for each binary variant, plus the header files
`configuration_public.h`, `atomic_public.h`, and `thread_types_public.h`. The GN
build will scan your directories for these files, and then calculate a port name
based on the directories between `src/third_party/starboard` and your
`configuration.gni` files. (e.g. for
`src/third_party/starboard/bobbox/armeb/configuration.gni`, it would choose the
platform configuration name `bobbox-armeb`.)


### IV. A New Port, Step-by-Step

  1. Recursively copy `src/starboard/stub` to
     `src/third_party/starboard/<family-name>/<binary-variant>`.  You may also
     consider copying from another reference platform, like `raspi-2` or
     `linux-x64x11`.
  1. In `gyp_configuration.py`
      1. In the `CreatePlatformConfig()` function, pass your
         `<platform-configuration>` as the parameter to the PlatformConfig
         constructor, like `return PlatformConfig('bobbox-armeb')`.
      1. In `GetVariables`
          1. Set `'clang': 1` if your toolchain is clang.
          1. Delete other variables in that function that are not needed for
             your platform.
      1. In `GetEnvironmentVariables`, set the dictionary values to point to the
         toolchain analogs for the toolchain for your platform.
  1. In `gyp_configuration.gypi`
      1. Update the names of the configurations and the default_configuration to
         be `<platform-configuration>_<build-type>` for your platform
         configuration name, where `<build-type>` is one of `debug`, `devel`,
         `qa`, `gold`.
      1. Update your platform variables.
          1. Set `'target_arch'` to your architecture: `'arm'`,
             `'x64'`, `'x86'`
          1. Set `'target_os': 'linux'` if your platform is Linux-based.
          1. Set `'gl_type': 'system_gles2'` if you are using the system EGL +
             GLES2 implementation.
          1. Set `'in_app_dial'` to `1` or `0`. This enables or disables the
             DIAL server that runs inside Cobalt, only when Coblat is
             running. You do not want in-app DIAL if you already have
             system-wide DIAL support.
      1. Update your toolchain command-line flags and libraries. Make sure you
         don't assume a particular workstation layout, as it is likely to be
         different for someone else.
      1. Update the global defines in `'target_defaults'.'defines'`, if
         necessary.
  1. Go through `configuration_public.h` and adjust all the configuration values
     as appropriate for your platform.
  1. Update `starboard_platform.gyp` to point at all the source files you want
     to build as part of your new Starboard implementation (as mentioned above).
  1. Update `atomic_public.h` and `thread_types_public.h` as necessary to point
     at the appropriate shared or custom implementations.


You should now be able to run gyp with your new port. From your `src/` directory:

    $ cobalt/build/gyp_cobalt -C debug bobbox-armeb
    $ ninja -C out/bobbox-armeb_debug nplb

This will attempt to build the "No Platform Left Behind" test suite with your
new Starboard implementation, and you are ready to start porting!

#### GN Instructions

Follow the above list, except:

  1. Ignore the steps about `gyp_configuration.py` and `gyp_configuration.gypi`.
  1. Update `BUILD.gn` instead of `starboard_platform.gyp`.
  1. Also in `BUILD.gn`:
      1. Update your toolchain command-line flags and libraries, for all
         configurations as well as for each individual configuration.
      1. Implement generic compiler configs such as `sb_pedantic_warnings`
         and `rtti`.
      1. If you're not using a predefined toolchain, define one.
  1. In `buildconfig.gni`:
      1. If your platform is Linux-based, set `target_os_ = "linux"`.
      1. Set `target_cpu_` to your target architecture (e.g. `arm`,
         `x64`, `x86`).
      1. Set the target and host toolchains. If you defined a target
         toolchain in `BUILD.gn`, you'll want to set it to that.
  1. In `configuration.gni`, set platform-specific defaults for any
     variables that should be overridden for your Starboard platform.  You
     can find a list of such variables at `//cobalt/build/config/base.gni`
     and `//starboard/build/config/base.gni`.


You should now be able to run GN with your new port. From your `src/` directory:

    $ gn args out/bobbox-armeb_debug

An editor will open up. Type into the editor:

    cobalt_config = "debug"
    target_platform = "bobbox-armeb"

Save and close the editor. Then run

    $ ninja -C out/bobbox-armeb_debug nplb

This will attempt to build the "No Platform Left Behind" test suite with your
new Starboard implementation, and you are ready to start porting!


## Suggested Implementation Order

When bringing up a new Starboard platform, it is suggested that you try to get
the NPLB tests passing module-by-module. Because of dependencies between
modules, you will find it easier to get some modules passing sooner than other
modules.

Here's a recommended module implementation order in which to get things going
(still significantly subject to change based on feedback):

  1. Configuration
  1. main(), Application, & Event Pump (i.e. the call into SbEventHandle)
  1. Memory
  1. Byte Swap
  1. Time
  1. String/Character/Double
  1. Log
  1. File
  1. Directory
  1. System
  1. Atomic
  1. Thread & Thread Types
  1. Mutex
  1. Condition Variable
  1. Once
  1. Socket
  1. SocketWaiter
  1. Window
  1. Input
  1. Blitter (if applicable)
  1. Audio Sink
  1. Media & Player
  1. DRM
  1. TimeZone
  1. User
  1. Storage
