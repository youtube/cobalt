# Starboard Modular ABI Wrappers

This directory contains code that will handle translations of data types across
ABI boundaries.

These wrappers are used in modular builds (including Evergreen). They are needed
to handle dealing with differences in platform-specific toolchain types and
musl-based types. For example, the `struct timespec` and `struct timeval` types
may contain differently sized member fields and when the caller is Cobalt code
compiled with musl it will provide argument data that needs to be adjusted when
passed to platform-specific system libraries.

To do this adjustment across compilation toolchains, there are 2 sets of files
in this directory that will be compiled in either the Cobalt toolchain or the
Starboard plaform-specific toolchain:

 * `cobalt_layer_posix_XYZ_abi_wrappers.cc` - these are compiled only in the
   Cobalt toolchain for modular builds (excluding Evergreen). The logic in
   these files simply defines the API we want to wrap and re-calls an external
   function named `__abi_wrap_XYZ` using the exact same parameter and return
   value types. This is simply to get the libcobalt.so to not link against
   the glibc implementations and instead rely on an external implementation
   that will be defined in libstarboard.so. Similar logic is done specifically
   for Evergreen in exported_symbols.cc, since we have a custom ELF loader in
   that case to handle remapping the functions.
 * `starboard_layer_posix_XYZ_abi_wrappers.{h,cc}` - these are compiled only
   in the Starboard platform-specific toolchain for modular builds (including
   Evergreen). The header files define corresponding musl-compatible types
   (copying definitions provided in //third_party/musl/include/alltypes.h.in)
   for the cases where data types are not guaranteed to match across ABI
   boundaries. These files then define the implementation of the
   `__abi_wrap_XYZ` functions, which will be called from the Cobalt layer.
   Since these functions are being implemented/compiled in the Starboard
   layer, they will rely on the platform-specific toolchain to get the
   platform-specific type definitions from the system headers (e.g. <time.h>).
   The implementations will handle doing data type conversions across the
   ABI boundary (both for parameters and return types).


To help illustrate how this works, we can use the POSIX function `clock_gettime`
as an example. When we build modularly, the following table shows how the
symbols would look using a tool like `nm`:

```
 libstarboard.so:
   U clock_gettime@GLIBC_2.17  (undefined reference to platform library)
   T __abi_wrap_clock_gettime  (concrete definition of ABI wrapper function)

 libcobalt.so
   U __abi_wrap_clock_gettime  (undefined reference to the Starboard wrapper)
   t clock_gettime             (concrete definition that calls __abi_wrap_*)
```
