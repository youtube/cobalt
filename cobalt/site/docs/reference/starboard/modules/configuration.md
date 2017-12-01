---
layout: doc
title: "Starboard Module Reference: configuration.h"
---

Provides a description of the current platform in lurid detail so that
common code never needs to actually know what the current operating system
and architecture are.<br>
It is both very pragmatic and canonical in that if any application code
finds itself needing to make a platform decision, it should always define
a Starboard Configuration feature instead. This implies the continued
existence of very narrowly-defined configuration features, but it retains
porting control in Starboard.

## Macros

<div id="macro-documentation-section">

<h3 id="sb_alignas" class="small-h3">SB_ALIGNAS</h3>

Specifies the alignment for a class, struct, union, enum, class/struct field,
or stack variable.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_alignof" class="small-h3">SB_ALIGNOF</h3>

Returns the alignment reqiured for any instance of the type indicated by
`type`.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_array_size" class="small-h3">SB_ARRAY_SIZE</h3>

A constant expression that evaluates to the size_t size of a statically-sized
array.

<h3 id="sb_array_size_int" class="small-h3">SB_ARRAY_SIZE_INT</h3>

A constant expression that evaluates to the int size of a statically-sized
array.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_can" class="small-h3">SB_CAN</h3>

Determines a compile-time capability of the system.

<h3 id="sb_can_media_use_starboard_pipeline" class="small-h3">SB_CAN_MEDIA_USE_STARBOARD_PIPELINE</h3>

Specifies whether the starboard media pipeline components (SbPlayerPipeline
and StarboardDecryptor) are used.  Set to 0 means they are not used.

<h3 id="sb_compile_assert" class="small-h3">SB_COMPILE_ASSERT</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_deprecated" class="small-h3">SB_DEPRECATED</h3>

SB_DEPRECATED(int Foo(int bar));
Annotates the function as deprecated, which will trigger a compiler
warning when referenced.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_deprecated_external" class="small-h3">SB_DEPRECATED_EXTERNAL</h3>

SB_DEPRECATED_EXTERNAL(...) annotates the function as deprecated for
external clients, but not deprecated for starboard.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_disallow_copy_and_assign" class="small-h3">SB_DISALLOW_COPY_AND_ASSIGN</h3>

A macro to disallow the copy constructor and operator= functions
This should be used in the private: declarations for a class

<h3 id="sb_experimental_api_version" class="small-h3">SB_EXPERIMENTAL_API_VERSION</h3>

The API version that is currently open for changes, and therefore is not
stable or frozen. Production-oriented ports should avoid declaring that they
implement the experimental Starboard API version.

<h3 id="sb_false" class="small-h3">SB_FALSE</h3>

<h3 id="sb_function" class="small-h3">SB_FUNCTION</h3>

Whether we use __PRETTY_FUNCTION__ or __FUNCTION__ for logging.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_has" class="small-h3">SB_HAS</h3>

Determines at compile-time whether this platform has a standard feature or
header available.

<h3 id="sb_has_64_bit_atomics" class="small-h3">SB_HAS_64_BIT_ATOMICS</h3>

Whether the current platform has 64-bit atomic operations.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_has_drm_key_statuses" class="small-h3">SB_HAS_DRM_KEY_STATUSES</h3>

<h3 id="sb_has_gles2" class="small-h3">SB_HAS_GLES2</h3>

Specifies whether this platform has a performant OpenGL ES 2 implementation,
which allows client applications to use GL rendering paths.  Derived from
the gyp variable 'gl_type' which indicates what kind of GL implementation
is available.

<h3 id="sb_has_graphics" class="small-h3">SB_HAS_GRAPHICS</h3>

Specifies whether this platform has any kind of supported graphics system.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_has_quirk" class="small-h3">SB_HAS_QUIRK</h3>

Determines at compile-time whether this platform has a quirk.

<h3 id="sb_input_on_screen_keyboard_api_version" class="small-h3">SB_INPUT_ON_SCREEN_KEYBOARD_API_VERSION</h3>

<h3 id="sb_int64_c" class="small-h3">SB_INT64_C</h3>

Declare numeric literals of signed 64-bit type.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_is" class="small-h3">SB_IS</h3>

Determines at compile-time an inherent aspect of this platform.

<h3 id="sb_is_compiler_gcc" class="small-h3">SB_IS_COMPILER_GCC</h3>

<h3 id="sb_is_compiler_msvc" class="small-h3">SB_IS_COMPILER_MSVC</h3>

<h3 id="sb_is_little_endian" class="small-h3">SB_IS_LITTLE_ENDIAN</h3>

Whether the current platform is little endian.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_likely" class="small-h3">SB_LIKELY</h3>

Macro for hinting that an expression is likely to be true.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_maximum_api_version" class="small-h3">SB_MAXIMUM_API_VERSION</h3>

The maximum API version allowed by this version of the Starboard headers,
inclusive.

<h3 id="sb_minimum_api_version" class="small-h3">SB_MINIMUM_API_VERSION</h3>

The minimum API version allowed by this version of the Starboard headers,
inclusive.

<h3 id="sb_my_experimental_feature_version" class="small-h3">SB_MY_EXPERIMENTAL_FEATURE_VERSION</h3>

EXAMPLE:
// Introduce new experimental feature.
//   Add a function, `SbMyNewFeature()` to `starboard/feature.h` which
//   exposes functionality for my new feature.

<h3 id="sb_noreturn" class="small-h3">SB_NORETURN</h3>

Macro to annotate a function as noreturn, which signals to the compiler
that the function cannot return.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_override" class="small-h3">SB_OVERRIDE</h3>

Declares a function as overriding a virtual function on compilers that
support it.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_player_with_url_api_version" class="small-h3">SB_PLAYER_WITH_URL_API_VERSION</h3>

<h3 id="sb_preferred_rgba_byte_order_argb" class="small-h3">SB_PREFERRED_RGBA_BYTE_ORDER_ARGB</h3>

An enumeration of values for the SB_PREFERRED_RGBA_BYTE_ORDER configuration
variable.  Setting this up properly means avoiding slow color swizzles when
passing pixel data from one library to another.  Note that these definitions
are in byte-order and so are endianness-independent.

<h3 id="sb_preferred_rgba_byte_order_bgra" class="small-h3">SB_PREFERRED_RGBA_BYTE_ORDER_BGRA</h3>

An enumeration of values for the SB_PREFERRED_RGBA_BYTE_ORDER configuration
variable.  Setting this up properly means avoiding slow color swizzles when
passing pixel data from one library to another.  Note that these definitions
are in byte-order and so are endianness-independent.

<h3 id="sb_preferred_rgba_byte_order_rgba" class="small-h3">SB_PREFERRED_RGBA_BYTE_ORDER_RGBA</h3>

An enumeration of values for the SB_PREFERRED_RGBA_BYTE_ORDER configuration
variable.  Setting this up properly means avoiding slow color swizzles when
passing pixel data from one library to another.  Note that these definitions
are in byte-order and so are endianness-independent.

<h3 id="sb_printf_format" class="small-h3">SB_PRINTF_FORMAT</h3>

Tells the compiler a function is using a printf-style format string.
`format_param` is the one-based index of the format string parameter;
`dots_param` is the one-based index of the "..." parameter.
For v*printf functions (which take a va_list), pass 0 for dots_param.
(This is undocumented but matches what the system C headers do.)
(Partially taken from base/compiler_specific.h)

<h3 id="sb_release_candidate_api_version" class="small-h3">SB_RELEASE_CANDIDATE_API_VERSION</h3>

The next API version to be frozen, but is still subject to emergency
changes. It is reasonable to base a port on the Release Candidate API
version, but be aware that small incompatible changes may still be made to
it.
The following will be uncommented when an API version is a release candidate.

<h3 id="sb_restrict" class="small-h3">SB_RESTRICT</h3>

Makes a pointer-typed parameter restricted so that the compiler can make
certain optimizations because it knows the pointers are unique.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_stringify" class="small-h3">SB_STRINGIFY</h3>

Standard CPP trick to stringify an evaluated macro definition.

<h3 id="sb_stringify2" class="small-h3">SB_STRINGIFY2</h3>

Standard CPP trick to stringify an evaluated macro definition.

<h3 id="sb_true" class="small-h3">SB_TRUE</h3>

<h3 id="sb_uint64_c" class="small-h3">SB_UINT64_C</h3>

Declare numeric literals of unsigned 64-bit type.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_unlikely" class="small-h3">SB_UNLIKELY</h3>

Macro for hinting that an expression is likely to be false.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_unreferenced_parameter" class="small-h3">SB_UNREFERENCED_PARAMETER</h3>

Trivially references a parameter that is otherwise unreferenced, preventing a
compiler warning on some platforms.

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_warn_unused_result" class="small-h3">SB_WARN_UNUSED_RESULT</h3>

Causes the annotated (at the end) function to generate a warning if the
result is not accessed.

<h3 id="sb_window_size_changed_api_version" class="small-h3">SB_WINDOW_SIZE_CHANGED_API_VERSION</h3>

</div>

