This is a partial fork of the Android bionic C library, taken from:

https://android.googlesource.com/platform/bionic/+/5e62b34c0d6fa545b487b9b64fb4a04a0589bc13/libc


The purpose is to build into the Android Starboard port necesary parts of bionic
that are not available on all versions of Android that we support, including:

* `ifaddrs` (and its dependencies) - Used by `SbSocketGetInterfaceAddress`, but
  not available until API 24.

