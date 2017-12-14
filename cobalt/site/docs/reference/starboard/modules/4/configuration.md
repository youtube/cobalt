---
layout: doc
title: "Starboard Module Reference: configuration.h"
---

Provides a description of the current platform in lurid detail so that common
code never needs to actually know what the current operating system and
architecture are.

It is both very pragmatic and canonical in that if any application code finds
itself needing to make a platform decision, it should always define a Starboard
Configuration feature instead. This implies the continued existence of very
narrowly-defined configuration features, but it retains porting control in
Starboard.

## Macros ##

### SB_ALIGNAS(byte_alignment) ###

Specifies the alignment for a class, struct, union, enum, class/struct field, or
stack variable.

### SB_ALIGNOF(type) ###

Returns the alignment reqiured for any instance of the type indicated by `type`.

### SB_ARRAY_SIZE(array) ###

A constant expression that evaluates to the size_t size of a statically-sized
array.

### SB_ARRAY_SIZE_INT(array) ###

A constant expression that evaluates to the int size of a statically-sized
array.

### SB_CAN(SB_FEATURE) ###

Determines a compile-time capability of the system.

### SB_CAN_MEDIA_USE_STARBOARD_PIPELINE ###

Specifies whether the starboard media pipeline components (SbPlayerPipeline and
StarboardDecryptor) are used. Set to 0 means they are not used.

### SB_COMPILE_ASSERT(expr, msg) ###

Will cause a compiler error with `msg` if `expr` is false. `msg` must be a valid
identifier, and must be a unique type in the scope of the declaration.

### SB_DEPRECATED(FUNC) ###

SB_DEPRECATED(int Foo(int bar)); Annotates the function as deprecated, which
will trigger a compiler warning when referenced.

### SB_DEPRECATED_EXTERNAL(FUNC) ###

SB_DEPRECATED_EXTERNAL(...) annotates the function as deprecated for external
clients, but not deprecated for starboard.

### SB_DISALLOW_COPY_AND_ASSIGN(TypeName) ###

A macro to disallow the copy constructor and operator= functions This should be
used in the private: declarations for a class

### SB_EXPERIMENTAL_API_VERSION ###

The API version that is currently open for changes, and therefore is not stable
or frozen. Production-oriented ports should avoid declaring that they implement
the experimental Starboard API version.

### SB_FUNCTION ###

Whether we use **PRETTY_FUNCTION** PRETTY_FUNCTION or **FUNCTION** FUNCTION for
logging.

### SB_HAS(SB_FEATURE) ###

Determines at compile-time whether this platform has a standard feature or
header available.

### SB_HAS_64_BIT_ATOMICS ###

Whether the current platform has 64-bit atomic operations.

### SB_HAS_GLES2 ###

Specifies whether this platform has a performant OpenGL ES 2 implementation,
which allows client applications to use GL rendering paths. Derived from the gyp
variable `gl_type` gl_type which indicates what kind of GL implementation is
available.

### SB_HAS_GRAPHICS ###

Specifies whether this platform has any kind of supported graphics system.

### SB_HAS_QUIRK(SB_FEATURE) ###

Determines at compile-time whether this platform has a quirk.

### SB_INT64_C(x) ###

Declare numeric literals of signed 64-bit type.

### SB_IS(SB_FEATURE) ###

Determines at compile-time an inherent aspect of this platform.

### SB_IS_LITTLE_ENDIAN ###

Legal values for SB_PREFERRED_RGBA_BYTE_ORDER are defined in this file above as
SB_PREFERRED_RGBA_BYTE_ORDER_*. If your platform uses GLES, you should set this
to SB_PREFERRED_RGBA_BYTE_ORDER_RGBA.Whether the current platform is little
endian.

### SB_LIKELY(x) ###

Macro for hinting that an expression is likely to be true.

### SB_MAXIMUM_API_VERSION ###

The maximum API version allowed by this version of the Starboard headers,
inclusive.

### SB_MICROPHONE_LABEL_API_VERSION ###

Adds string label to SbMicrophoneInfo . This should indicate the friendly name
of the microphone type.

### SB_MINIMUM_API_VERSION ###

The minimum API version allowed by this version of the Starboard headers,
inclusive.

### SB_NORETURN ###

Macro to annotate a function as noreturn, which signals to the compiler that the
function cannot return.

### SB_OVERRIDE ###

Declares a function as overriding a virtual function on compilers that support
it.

### SB_PREFERRED_RGBA_BYTE_ORDER_RGBA ###

An enumeration of values for the SB_PREFERRED_RGBA_BYTE_ORDER configuration
variable. Setting this up properly means avoiding slow color swizzles when
passing pixel data from one library to another. Note that these definitions are
in byte-order and so are endianness-independent.

### SB_PRINTF_FORMAT(format_param, dots_param) ###

Tells the compiler a function is using a printf-style format string.
`format_param` is the one-based index of the format string parameter;
`dots_param` is the one-based index of the "..." parameter. For v*printf
functions (which take a va_list), pass 0 for dots_param. (This is undocumented
but matches what the system C headers do.) (Partially taken from
base/compiler_specific.h)

### SB_RELEASE_CANDIDATE_API_VERSION ###

The next API version to be frozen, but is still subject to emergency changes. It
is reasonable to base a port on the Release Candidate API version, but be aware
that small incompatible changes may still be made to it. The following will be
uncommented when an API version is a release candidate.

### SB_RESTRICT ###

Include the platform-specific configuration. This macro is set by GYP in
starboard_base_target.gypi and passed in on the command line for all targets and
all configurations.Makes a pointer-typed parameter restricted so that the
compiler can make certain optimizations because it knows the pointers are
unique.

### SB_STRINGIFY(x) ###

Standard CPP trick to stringify an evaluated macro definition.

### SB_UINT64_C(x) ###

Declare numeric literals of unsigned 64-bit type.

### SB_UNLIKELY(x) ###

Macro for hinting that an expression is likely to be false.

### SB_UNREFERENCED_PARAMETER(x) ###

Trivially references a parameter that is otherwise unreferenced, preventing a
compiler warning on some platforms.

### SB_WARN_UNUSED_RESULT ###

Causes the annotated (at the end) function to generate a warning if the result
is not accessed.
