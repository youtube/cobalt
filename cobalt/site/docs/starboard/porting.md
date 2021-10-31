---
layout: doc
title: "Porting Cobalt to your Platform with Starboard"
---

This document provides step-by-step instructions for porting Cobalt to run
on your platform. To do so, you'll use Starboard, which is Cobalt's porting
layer and OS abstraction. Starboard encapsulates only the platform-specific
functionality that Cobalt uses.

To complete a port, you need to add code to the Cobalt source tree. We
recognize that many porters will not want to actually share details of
their Starboard implementation with external users. These porters likely
have their own source control and will fork Cobalt into their software
configuration management (SCM) tools. These instructions take that use
case into account and explain how to add your port in a way that will not
conflict with future Cobalt source code changes.

## Prerequisites

To complete the instructions below, you first need to clone the Cobalt source
code repository:

```sh
$ git clone https://cobalt.googlesource.com/cobalt
```

If you prefer, you can instead complete the instructions for
[setting up a Cobalt development environment on Linux](
/cobalt/development/setup-linux.html). Checking out the Cobalt source
code is one step in that process.

## Porting steps

### 1. Enumerate and name your platform configurations

Your first step is to define canonical names for your set of platform
configurations. You will later use these names to organize the code
for your platforms.

A platform configuration has a one-to-one mapping to a production binary.
As such, you will need to create a new platform configuration any time you
need to produce a new binary.

A platform configuration name has two components:

*   The `<family-name>` is a name that encompasses a group of products that
    you are porting to Starboard.
*   The `<binary-variant>` is a string that uniquely describes specifics
    of the binary being produced for that configuration.

The recommended naming convention for a platform configuration is:

    <family-name>-<binary-variant>

For example, suppose a company named BobCo produces a variety of BobBox
devices. Some of the devices use big-endian ARM chips, while others use
little-endian ARM chips. BobCo might define two platform configurations:

*   `bobbox-armeb`
*   `bobbox-armel`

In this example, `bobbox` is the family name and is used in both (all)
of BobCo's platform configurations. The `binary-variant` for devices with
big-endian ARM chips is `armeb`. For devices with little-endian ARM chips,
the `binary-variant` is `armel`.


### 2. Add Source Tree Directories for your Starboard Port

Add the following directories to the source tree for the `<family-name>`
that you selected in step 1:

*   `src/third_party/starboard/<family-name>/`

*   `src/third_party/starboard/<family-name>/shared/`

    This subdirectory contains code that is shared between architectures
    within a product family. For example, if BobBox devices run on many
    different platforms, then BobCo needs to define a different configuration
    for each platform. However, if all of the configurations can use the same
    Starboard function implementation, BobCo can put that function in the
    `shared` directory to make it accessible in every binary variant.

*   `src/third_party/starboard/<family-name>/<binary-variant>/`

    You should create one directory for _each_ `<binary-variant>`. So, for
    example, BobCo could create the following directories:

    *   `src/third_party/starboard/bobbox/shared/`
    *   `src/third_party/starboard/bobbox/armeb/`
    *   `src/third_party/starboard/bobbox/armel/`
    *   `src/third_party/starboard/bobbox/armel/gles/`

Again, functions that work for all of the configurations would go in the
`shared` directory. Functions that work for all little-endian devices would go
in the `armel` directory. And functions specific to little-endian devices
that use OpenGL ES would go in the `armel/gles` directory.

### 3. Add required `binary-variant` files

Each `binary-variant` directory that you created in step 2 must contain
the following files:

*   `atomic_public.h`
*   `configuration_public.h`
*   `gyp_configuration.gypi`
*   `gyp_configuration.py`
*   `starboard_platform.gyp`
*   `thread_types_public.h`

We recommend that you copy the files from the Stub reference implementation,
located at `src/starboard/stub/` to your `binary-variant` directories.
In this approach, you will essentially start with a clean slate of stub
interfaces that need to be modified to work with your platform.

An alternate approach is to copy either the Desktop Linux or Raspberry Pi
ports and then work backward to fix the things that don't compile or work
on your platform.

If you are copying the Stub implementation, you would run the following
command for each `binary-variant` directory:

```sh
cp -R src/starboard/stub
      src/third_party/starboard/<family-name>/<binary-variant>
```

After copying these files, you should be able to compile Cobalt and link it
with your toolchain even though the code itself will not yet work.


#### 3a. Additional files in the stub implementation

The stub implementation contains three additional files that are not listed
among the required files for each `binary-variant` directory:

*   `application_stub.cc`
*   `application_stub.h`
*   `main.cc`

The Starboard `Application` class is designed to do the following:

*   Integrate a generic message loop function with a system message pump that
    can deliver either input or application lifecycle events like
    suspend/resume.
*   Provide a place for graceful platform-specific initialization and teardown.
*   Provide a universally accessible place to store global platform-specific
    state, such as managed UI windows.

Thus, these files provide a framework for fulfilling Starboard's event
dispatching requirements, even though they don't implement any specific
Starboard interface and aren't strictly necessary for any given port. Even so,
anyone who is porting Cobalt will likely need to adapt a copy of
`application_stub.cc` and `application_stub.h` to their platforms needs.

The `application` files do not necessarily need to be per-variant files. Even
if you have multiple variants, it's still possible that you only need one copy
of these files in your `shared` directory. Similarly, you might have a shared
base class along with variant-specific subclasses.

### 4. Make required file modifications

To port your code, you must make the following modifications to the files
that you copied in step 3:


1.  **`atomic_public.h`** - Ensure that this file points at the appropriate
    shared or custom implementation.

1.  **`configuration_public.h`** - Adjust all of the [configuration values](
    ../../reference/starboard/configuration-public.html) as appropriate for
    your platform.

1.  **`gyp_configuration.py`**
    1.  Modify the `CreatePlatformConfig()` function to return the platform
        configuration that you defined in step 1. The example below shows that
        function as it appears in the Stub implementation and the Desktop
        Linux port:

        <pre>
        # Stub:
        def CreatePlatformConfig():
          try:
            return PlatformConfig('<b>stub</b>')
          except RuntimeError as e:
            logging.critical(e)
            return None

        # Desktop Linux
        def CreatePlatformConfig():
          try:
            return gyp_configuration.PlatformConfig('<b>linux-x64x11</b>')
          except RuntimeError as e:
            logging.critical(e)
            return None
        </pre>

        Note that the third line is the only one that is different. The
        key difference in terms of these instructions is changing `stub`
        to `linux-x64x11`. The Desktop Linux port also uses a shared copy
        of `gyp_configuration.py`, which is why the rest of the line is
        different.

    1.  In the `GetVariables` function, ensure that the `clang` variable
        is set to `1` if your toolchain uses clang:

        <pre>
        variables = super(PlatformConfig, self).GetVariables(configuration)
        <b>variables.update({'clang': 1,})</b>
        return variables
        </pre>

    1.  In the `GetEnvironmentVariables` function, set these dictionary values
        to the toolchain analogs for your platform. Note that "target platform"
        refers to the platform being targeted by the port (e.g. Android TV,
        Raspberry Pi) and "host platform" is the workstation platform running
        the cross-compiler to the target (e.g. desktop Linux, desktop Windows).
        *   `CC` - the C compiler for the target platform. Example: `clang`.
        *   `CXX` - the C++ compiler for the target platform.
            Example: `clang++`.
        *   `CC_HOST` - the C compiler for the host platform. Example: `clang`.
        *   `CXX_HOST` - the C++ compiler for the host platform.
            Example: `clang++`.
        *   `LD_HOST` - the C++ linker for the host platform.
            Example: `clang++`.
        *   `ARFLAGS_HOST` - Archiver flags for the host platform. The
            archiver is the toolchain program that creates static libraries.
            Example: `rcs`.
        *   `ARTHINFLAGS_HOST` - Archiver flags for the host platform for
            creating "thin" archives, which are faster for intermediate
            libraries that aren't for direct publishing. Example: `rcsT`.

1.  **`gyp_configuration.gypi`**
    1.  Update the value of the `default_configuration` property and the keys
        in the `configurations` property to be
        `<platform-configuration>_<build-type>` where:
        1.  `<platform-configuration>` is the value that you defined in step 1.
        1.  `<build-type>` is one of the following values:
            *   `debug`
            *   `devel`
            *   `qa`
            *   `gold`

        The following snippet from the configuration file shows how these
        properties appear in the Stub implementation. The Desktop Linux port
        replaces the string `stub` with `linux-x64x11` everywhere that it
        appears:

        ```
        'target_defaults': {
          'default_configuration': 'stub_debug',
          'configurations': {
            'stub_debug': {
              'inherit_from': ['debug_base'],
            },
            'stub_devel': {
              'inherit_from': ['devel_base'],
            },
            'stub_qa': {
              'inherit_from': ['qa_base'],
            },
            'stub_gold': {
              'inherit_from': ['gold_base'],
            },
          }, # end of configurations
          ...
        }
        ```

    1.  Update the following properties in the `variables` dictionary:
        *   `target_arch` - Identifies your architecture. Supported values
            are `arm`, `x64`, and `x86`.
        *   `target_os` - Set to `linux` if your platform is Linux-based.
            Otherwise, remove this variable.
        *   `gl_type` - Set to `system_gles2` if you are using the system EGL
            \+ GLES2 implementation and otherwise set the value to `none`.
        *   `in_app_dial` - Enables (or disables) the DIAL server that runs
            inside Cobalt. (This server only runs when Cobalt is running.)
            The [DIAL protocol](http://www.dial-multiscreen.org/home) enables
            second-screen devices (like tablets and phones) to discover,
            launch, and interface with applications on first-screen devices
            (like televisions, set-top boxes, and Blu-ray players).
            *   Set this value to `0` if you already have system-wide DIAL
                support. In that case, a DIAL server running inside Cobalt would
                be redundant.
            *   Set this value to `1` if you want Cobalt to run a DIAL server
                whenever it is running. That server could only be used to
                connect with the current Cobalt application (e.g. YouTube).


    1.  Update your toolchain command-line flags and libraries. Ensure that
        you do not assume a particular workstation layout since that layout
        may vary from user to user.

    1.  Update the global defines in the `target_defaults.defines` dictionary,
        if necessary.

1.  **`thread_types_public.h`** - Ensure that this file points at the
    appropriate shared or custom implementation.

### 5. Port modules to work on your platform

The `starboard_platform.gyp` file points to all of the source files included
in your new Starboard implementation. If you are starting with a copy of the
Stub implementation, then that file will initially include a lot of files
from the `src/starboard/shared/stub/` directory. Similarly, if you are starting
starting with a copy of the Desktop Linux port, then that file will initially
point to files in the `src/starboard/shared/posix` directory.

The modules are defined so that each one has a set of functions, and each
function is defined in its own file with a consistent naming convention.
For example, the `SbSystemBreakIntoDebugger()` function is defined in the
`system_break_into_debugger.cc` file. The list of files in the
`src/starboard/shared/stub/` directory represents an authoritative list of
supported functions.

Function-by-function and module-by-module, you can now replace stub
implementations with either custom implementations or with other ported
implementations from the `src/starboard/shared/` directory until you have
gone through all of the modules. As you do, update the
`starboard_platform.gyp` file to identify the appropriate source files.

Due to dependencies between modules, you will find it easier to get some
modules to pass the NPLB tests before other modules. We recommend porting
modules in the following order to account for such dependencies:

1.  Configuration
1.  main(), Application, and Event Pump - This is the call into `SbEventHandle`.
1.  Memory
1.  Byte Swap
1.  Time
1.  String/Character/Double
1.  Log
1.  File
1.  Directory
1.  System
1.  Atomic
1.  Thread & Thread Types
1.  Mutex
1.  Condition Variable
1.  Once
1.  Socket
1.  SocketWaiter
1.  Window
1.  Input
1.  Blitter (if applicable)
1.  Audio Sink
1.  Media & Player
1.  DRM
1.  TimeZone
1.  User
1.  Storage
