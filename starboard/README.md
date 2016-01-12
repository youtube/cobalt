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
