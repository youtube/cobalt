# Starboard

Starboard is Cobalt's porting layer and OS abstraction. It attempts to encompass
all the platform-specific functionality that Cobalt actually uses, and nothing
that it does not.

## Current State

Starboard is still very much in development, and does not currently run
Cobalt. Mainly, it runs NPLB (see below) and Chromium base, and only on Linux.

The work so far has focused on supporting Chromium base and net, and their
dependencies, so the APIs for things like media decoding, decryption, DRM, and
graphics haven't yet been defined.

## Interesting Source Locations

All source locations are specified relative to `src/starboard/` (this directory).

  * `.` - The current directory contains all the public headers that Starboard
    defines.
  * `nplb/` - No Platform Left Behind: Starboard's platform verification test
    suite.
  * `linux/` - The home of the Linux Starboard implementation
  * `shared/` - The home of all code that can be shared between Starboard
    implementations. Directly in this directory are files that only reference
    other Starboard APIs, and therefore should be completely
    reusable. Subdirectories delimit code that can be shared between platforms
    that share some facet of their OS API.
    * `shared/iso` - Implementation files that follow ISO C and C++
      standards. Platforms that are ISO-compliant should be able to use these.
    * `shared/posix` - POSIX-specific implementation files. Starboard
      implementations may pull some or all of this directory to implement
      Starboard APIs if they are POSIX-compliant.
    * `shared/libevent` - libevent-specific implementation files, for platforms
      that can build and run libevent.
    * `shared/pthread` - pthread-specific implementation files, for platforms
      that provide a reasonable implementation of POSIX threads.


## Building with Starboard

Follow the Cobalt instructions, except when invoking gyp:

    $ cobalt/build/gyp_cobalt -C Debug starboard_linux

and when invoking ninja:

    $ ninja -C out/SbLinux_Debug cobalt
