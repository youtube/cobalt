Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Porting Cobalt 27 to your Platform with Starboard 18

This document provides step-by-step instructions for porting Cobalt 27 to your
platform. You will use Starboard 18, Cobalt's porting layer and OS abstraction,
to do this. Starboard encapsulates only the platform-specific functionality
that Cobalt requires.

To complete a port, you must add code to the Cobalt source tree. Because many
porters prefer not to share details of their Starboard implementation, these
instructions explain how to add your port without conflicting with future
Cobalt updates.

## Prerequisites

To complete the instructions below, you first need to clone the Cobalt source
code repository:

```bash
mkdir ~/cobalt && cd ~/cobalt
git clone --single-branch https://github.com/youtube/cobalt.git src
gclient config --name=src https://github.com/youtube/cobalt.git
cd src
gclient sync --no-history -r $(git rev-parse @)
```

If you prefer, you can instead complete the instructions for
[setting up a Cobalt development environment on Linux](../development/setup-linux.md).
Checking out the Cobalt source code is one step in that process.

Additionally, you should identify your target architecture (e.g., x86_64, ARMv7, ARMv8) and check if a valid Starboard ABI (SABI) file exists for it in [starboard/sabi](https://github.com/youtube/cobalt/tree/27.lts/starboard/sabi). If not, you may need to create one based on the schema provided there. See [starboard/doc/starboard_abi.md](https://github.com/youtube/cobalt/blob/27.lts/starboard/doc/starboard_abi.md) for details.

## Porting steps

### 1. Enumerate and name your platform configurations

Your first step is to define canonical names for your platform configurations.
You will later use these names to organize the code for your platform and to
specify the target when building.

A platform configuration maps one-to-one to a production binary. Consequently,
you must create a new platform configuration whenever you need to produce a
new binary.

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
that you selected in step 1. While internal platforms are located
in the `starboard/` directory (e.g., `starboard/linux/`), custom and external
ports should be placed in `third_party/starboard/`:

*   `third_party/starboard/<family-name>/`

*   `third_party/starboard/<family-name>/shared/`

    This subdirectory contains code that is shared between architectures
    within a product family. If all of the configurations can use the same
    Starboard function implementation, you can put that function here to make
    it accessible in every binary variant.

*   `third_party/starboard/<family-name>/<binary-variant>/`

    You should create one directory for _each_ `<binary-variant>`.

Again, functions that work for all of the configurations would go in the
`shared` directory. Functions specific to little-endian devices would go
in the `armel` directory.

### 3. Add required `binary-variant` files and select a baseline

Each `binary-variant` directory that you created in step 2 must contain
the following configuration and build files:

*   `atomic_public.h`
*   `BUILD.gn`
*   `configuration_public.h`
*   `platform_configuration/BUILD.gn`
*   `platform_configuration/configuration.gni`
*   `toolchain/BUILD.gn`
*   `starboard_abi.json` (or a reference to an existing one in [starboard/sabi](https://github.com/youtube/cobalt/tree/27.lts/starboard/sabi))

To populate these files, you should select a baseline that best matches your platform instead of starting from scratch:

*   **Stub Baseline**: Copy files from [starboard/stub](https://github.com/youtube/cobalt/tree/27.lts/starboard/stub) to your `binary-variant` directory. This provides a clean slate of stub interfaces that need to be modified. This is the most generalized starting point.
*   **Reference Baseline (Linux)**: If your platform is POSIX-compliant, you can reference or copy from [starboard/linux/x64x11](https://github.com/youtube/cobalt/tree/27.lts/starboard/linux/x64x11) or [starboard/linux/shared](https://github.com/youtube/cobalt/tree/27.lts/starboard/linux/shared).
*   **Shared Modules**: Regardless of the baseline, strongly consider reusing common implementations located in [starboard/shared](https://github.com/youtube/cobalt/tree/27.lts/starboard/shared) (e.g., `posix`, `pthread`, `egl`, `gles`) by referencing them in your `BUILD.gn` file, as seen in `starboard/linux/shared/BUILD.gn`.

If you are copying the Stub implementation, you would run the following
command for each `binary-variant` directory:

```sh
cp -R starboard/stub/* third_party/starboard/<family-name>/<binary-variant>/
```

After copying these files, you should be able to compile Cobalt and link it
with your toolchain even though the code itself will not yet work.

#### 3a. Additional files in the stub implementation

The stub implementation contains three additional files that are not listed
among the required files for each `binary-variant` directory:

*   `application_stub.cc`
*   `application_stub.h`
*   `main.cc`

The Starboard `Application` class is designed to:

*   Integrate a generic task runner function with a system message pump that
    can deliver either input or application lifecycle events, such as suspend
    and resume.
*   Provide a hook for graceful platform-specific initialization and teardown.
*   Provide a universally accessible storage for global platform-specific
    state, such as managed UI windows.

These files provide a framework for fulfilling Starboard's event
dispatching requirements. Although they do not implement a specific Starboard
interface and are not strictly necessary for a port, you will likely need to
adapt a copy of `application_stub.cc` and `application_stub.h` to your platform's
needs.

The `application` files do not need to be per-variant files. Even with multiple
variants, you may only need one copy of these files in your `shared` directory.
Alternatively, you can use a shared base class with variant-specific
subclasses.

### 4. Make required file modifications

To port your code, add your platform to `starboard/build/platforms.py`
and then make the following modifications to the files copied in step 3:

Note that [cobalt/build/gn.py](https://github.com/youtube/cobalt/blob/27.lts/cobalt/build/gn.py) is the main entry point for configuring your build. You will use it to specify your platform configuration name when running GN.

Porters should also be aware of [cobalt/app/cobalt_switch_defaults_starboard.cc](https://github.com/youtube/cobalt/blob/27.lts/cobalt/app/cobalt_switch_defaults_starboard.cc), which sets default command-line switches for Starboard platforms. You may need to review or override these for your platform.

1.  **`atomic_public.h`** - Ensure that this file points at the appropriate
    shared or custom implementation.

1.  **`configuration_public.h`** - Adjust all of the configuration values as appropriate for
    your platform.

1.  **`platform_configuration/BUILD.gn`**
    1.  Update your platform command-line flags and libraries. Ensure that
        you do not assume a particular workstation layout since that layout
        may vary from user to user.

1.  **`platform_configuration/configuration.gni`**
    1.  Update the following variables in the file from their default in
        `starboard/build/config/base_configuration.gni` if necessary:
        *   `gl_type` - Set to `system_gles2` if you are using the system EGL
            \+ GLES2 implementation and otherwise set the value to `none`.
        *   `enable_in_app_dial` - Enables or disables the DIAL server that
            runs inside Cobalt (only when Cobalt is running).
            The [DIAL protocol](https://www.dial-multiscreen.org/) allows
            devices like tablets and phones to discover,
            launch, and interface with applications on devices, including
            televisions, set-top boxes, and Blu-ray players.
            *   Set this value to `false` if you already have system-wide DIAL
                support. In that case, a DIAL server running inside Cobalt
                is redundant.
            *   Set this value to `true` if you want Cobalt to run a DIAL
                server. That server can be used only to connect with the
                current Cobalt application (for example, YouTube).
        *   `sabi_path` - Set this to the path of your Starboard ABI file (e.g., `"//starboard/sabi/x64/sabi.json"`). This is required for ABI verification.

1.  **`toolchain/BUILD.gn`**
    1.  If your platform uses a simple clang toolchain, pass the path to the
        toolchain to the `clang_toolchain` template. This template assumes
        names for each of the tools; for example, the `clang++` executable
        must be at `${clang_base_path}/clang++`.

        Modify the host's `toolchain_args` to pass the correct OS and CPU for
        your host platform.

        If your toolchain uses GCC or has tool names that differ from what
        the `clang_toolchain` template assumes, use the `gcc_toolchain`
        template (also provided in `//build/toolchain/gcc_toolchain.gni`).
        To use this template, pass full paths to each required tool
        (`ar`, `cc`, `cxx`, and `ld`), as well as any optional tools. If your
        toolchain uses clang, ensure you pass `is_clang = true` in
        `toolchain_args`.

    2.  Update your toolchain command-line flags and libraries. Do not assume
        a particular workstation layout, as it might vary between users.

### 5. Port modules to work on your platform

The `BUILD.gn` file specifies all source files included in your Starboard
implementation. If you start with a copy of the Stub implementation, this file
initially includes many files from `starboard/shared/stub/`. If you start with
the Desktop Linux port, it initially references files in
`starboard/shared/posix/`.

The modules are defined so that each one has a set of functions, and each
function is defined in its own file with a consistent naming convention.
For example, the `SbSystemBreakIntoDebugger()` function is defined in the
`system_break_into_debugger.cc` file. The list of files in the
`starboard/shared/stub/` directory represents an authoritative list of
supported functions.

**Recommendation:** When porting modules, strongly prefer reusing existing implementations in [starboard/shared](https://github.com/youtube/cobalt/tree/27.lts/starboard/shared) (such as `starboard/shared/posix` and `starboard/shared/pthread`) to minimize the burden on the porter and maintain consistency.

Replace stub implementations function-by-function and
module-by-module until you have gone through all of the modules. Use either custom implementations or existing ports from
`starboard/shared/`. As you proceed, update `BUILD.gn` to reference the correct
source files.

Because of dependencies between modules, some are easier to verify with NPLB
before others. We recommend porting modules in the following order:

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

## Verification and Testing

Once you have implemented the modules, you need to verify your port.

### Running Tests
Cobalt provides a script to build and run tests. You can use [cobalt/tools/build_and_run_tests.sh](https://github.com/youtube/cobalt/blob/27.lts/cobalt/tools/build_and_run_tests.sh) to execute the test suite (including NPLB) on your platform.

### Test Filters
It is common for some tests to fail on new platforms or to be inapplicable. You can manage test expectations by adding filters in the [cobalt/testing/filters](https://github.com/youtube/cobalt/tree/27.lts/cobalt/testing/filters) directory for your platform. This allows you to track known issues and ignore failures that are not critical for your port.
