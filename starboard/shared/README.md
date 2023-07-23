# Starboard Shared Implementations

`src/starboard/shared` and subdirectories contain all implementation code that
could concievably be shared between platforms.

## Organization

As much code as possible is pushed into the shared directory. Each subdirectory
`foo` means "files in here depend on *foo*."

All source locations are specified relative to `src/starboard/`.

 * `shared/iso/` - Implementation files that follow ISO C and C++ standards, but
    standards that are either new, or have historically had poor support on a
    variety of embedded platforms. Platforms that are ISO-compliant with the
    latest standards should be able to use these.
 * `shared/libevent/` - libevent-specific implementation files, for platforms
   that can build and run libevent. See `shared/libevent/README.md` for more
   information.
 * `shared/posix/` - POSIX-specific implementation files. Starboard
   implementations may pull some or all of this directory to implement Starboard
   APIs, if the platform is POSIX-compliant.
 * `shared/pthread/` - pthread-specific implementation files, for platforms that
   provide a *reasonable* implementation of POSIX threads. This is called out
   separately from POSIX, because we've found that there are some platforms that
   are fairly POSIX-y, except in their threading APIs.
 * `shared/starboard/` - Implementations of Starboard API functions that only
   rely on other Starboard API functions. Code in here should be reusable on any
   platform that implements the needed APIs.
