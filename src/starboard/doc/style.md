# Starboard C and C++ Style Guide

A description of the coding conventions for Starboard code and API headers.

**Status:** REVIEWED\
**Created:** 2016-11-08

Starboard generally tries to follow the coding conventions of Cobalt, which
itself mostly follows the conventions of Chromium, which mostly follows the
externally-published Google C++ coding conventions. But, Starboard has some
special requirements due to its unusual constraints, so it must add a few new
conventions and loosen some of the existing style prescriptions.

## Background

Before looking at this document, bear in mind that it is not intending to
completely describe all conventions that apply to Starboard. You probably want
to take some time to familiarize yourself with these documents first, probably
in this order:

  * [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
  * [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md)
  * [Cobalt Style Guide](http://cobalt.foo/broken)

The main additional constraints that Starboard has to deal with are:

  * The Starboard API is defined in straight-C. It must be able to interface
    with all possible third-party components, many of which are in C and not
    C++.
  * Starboard is a public API. Starboard platform implementations and
    applications written on top of Starboard will change independently. This
    means there are intense requirements for API stability, usage
    predictability, searchability, and documentation.
  * Note that even though it is presented as a "style guide," the conventions
    presented here are required to be approved for check-in unless otherwise
    noted.


## Definitions

### snake-case

Words separated with underscores.

    lower_snake_case
    ALL_CAPS_SNAKE_CASE

### camel-case

Words separated by letter capitalization.

    camelCase
    CapitalizedCamelCase

## C++ Guidelines

What follows are hereby the guidelines for Starboard C and C++ code. Heretofore
the guidelines follow thusly as follows.

### API Definitions

  * Starboard API definitions must always be compatible with straight-C99 compilers.
  * All public API declarations must be specified in headers in
    `src/starboard/*.h`, not in any subdirectories.
  * Non-public declarations must NOT be specified in headers in
    `src/starboard/*.h`.
  * C++ inline helper definitions may be included inside an `#if
    defined(__cplusplus)` preprocessor block. They must only provide
    convenience, and must NOT be required for any API functionality.
  * All public API functions should be exported symbols with the SB_EXPORT
    attribute.
  * No non-const variables shall be exposed as part of the public API.
  * All APIs should be implemented in C++ source files, not straight-C source files.

### Modules

  * Each module header must be contained with a single header file.

  * The name of the module must be the singular form of the noun being
    interfaced with by the module, without any "sb" or "starboard".
      * `file.h`
      * `directory.h`
      * `window.h`
  * Module interfaces should not have circular dependencies.

### File Names

  * Like in the other conventions (e.g. Google, Chromium), file names must be in
    `lower_snake_case`.
  * File names must not contain `sb_` or `starboard_`.
  * The name of a module header file must be the `lower_snake_case` form of the
    module name.
      * `SbConditionVariable` ➡ `starboard/condition_variable.h`
  * A header that is intended to be an internal implementation detail of one or
    more platform implementations should have the suffix `_internal.h`, and
    include the header `starboard/shared/internal_only.h`.
  * See "Implementations" for conventions about where to place implementation files.

### Types

  * Like in the other conventions, types should be `CapitalizedCamelCase`.
  * Every public Starboard type must start with `Sb`. There are no namespaces in
    C, so `Sb` is the Starboard namespace.
  * Every public Starboard type must be declared by a module, and must have the
    name of the module following the `Sb`.
      * `file.h` contains `SbFile`, `SbFileInfo`, `SbFileWhence`, etc...
  * Every seemingly-allocatable, platform-specific Starboard type should be
    defined as an opaque handle to a publically undefined struct with the
    `Private` suffix. Follow this pattern for all such type declarations.
      * `struct SbFilePrivate` is declared, but not defined in the public header.
      * `SbFilePrivate` is `typedef`'d to `struct SbFilePrivate`. This is a C
        thing where types are defined as names with the "`struct`" keyword
        prepended unless `typedef`'d.
      * `SbFile` is defined as a `typedef` of `struct SbFilePrivate*`.
  * C structs may be defined internally to have functions and visibility. It is
    allowed for such structs to have constructors, destructors, methods,
    members, and public members.
  * It is also considered idiomatic to never define the private struct but to
    just treat it like a handle into some other method of object tracking,
    casting the handle back and forth to the pointer type.
  * If a word in the name of a type is redundant with the module name, it is
    omitted.
        * A monotonic time type in the Time module is `SbTimeMonotonic`, not
          ~~`SbMonotonicTime`, `SbTimeMonotonicTime`, or
          `SbTimeMonotonicSbTime`~~.

### Functions

  * Like in the other conventions, functions should be `CapitalizedCamelCase`.
  * Every public Starboard function must start with `Sb`. There are no namespaces
    in C, so `Sb` is the Starboard namespace.
  * Every public Starboard function must be declared by a module, and must have
    the name of the module following the `Sb`.
      * `system.h` contains `SbSystemGetPath()`
      * `file.h` contains `SbFileOpen()`
  * After the Starboard and Module prefix, functions should start with an
    imperative verb indicating what the function does.
      * The Thread module defines `SbThreadCreateLocalKey()` to create a key for
        thread local storage.
  * If a word in the name of a function is redundant with the module name, it is
    omitted.
      * The `File` module as the function `SbFileOpen`, not ~~`SbOpenFile`,
        `SbFileOpenFile` or `SbFileOpenSbFile`~~.
      * If this gets awkward, it may indicate a need to split into a different
        module.

### Variables, Parameters, Fields

  * Like in the other conventions, variable, function parameter, and field names
    must be in `lower_snake_case`.
  * Private member fields end in an underscore.
  * Public member fields do not end in an underscore.

### Namespaces

Most Starboard API headers are straight-C compatible, so cannot live inside a
namespace. Implementations, since they implement straight-C interface functions,
also cannot live inside a namespace.

But, in all other cases, Starboard C++ code should follow the inherited
conventions and use a namespace for each directory starting with a "starboard"
namespace at the starboard repository root.

### Preprocessor Macros

  * Like in the other conventions, variable, function parameter, and field names
    must be in `ALL_CAPS_SNAKE_CASE`.
  * Macros may be used as compile-time constants because straight-C does not
    have a proper facility for typed constants. This is as opposed to macros
    used primarily at preprocessor-time to filter or modify what gets sent to
    the compiler. Macros used as compile-time constants and that are not
    configuration parameters should be explicitly-typed with a c-style cast, and
    should follow the Constants naming conventions.
  * Macros must start with `SB_`, and then must further be namespaced with the
    module name, with the exception of configuration definitions.
  * Configuration definitions should be namespaced with the module name that
    they primarily affect, if applicable, or a scope that generally indicates
    its domain.
      * `SB_FILE_MAX_NAME`
      * `SB_MEMORY_PAGE_SIZE`
  * Always use `#if defined(MACRO)` over `#ifdef MACRO`.

### Constants

  * Constants (including enum entries) are named using the Google constant
    naming convention, `CapitalizedCamelCase`d, but starting with a lower-case
    `k`.
  * After the `k`, all constants have `Sb`, the Starboard namespace.
      * `kSb`
  * After `kSb`, all constants then have the module name.
      * `kSbTime`
      * `kSbFile`
  * After `kSb<module>` comes the rest of the name of the constant.
      * `kSbTimeMillisecond`
      * `kSbFileInvalid`
  * Enum entries are prefixed with the full name of the enum.
      * The enum `SbSystemDeviceType` contains entries like
        `kSbSystemDeviceTypeBlueRayDiskPlayer`.

### Comments

  * All files must have a license and copyright comment.
  * It is expected that the straight-C compiler supports C99 single-line
    comments. Block comments should be avoided whenever possible, even in
    license and copyright headers.
  * Each public API module file should have a Module Overview documentation
    comment below the license explaining what the module is for, and how to use
    it effectively.
  * The Module Overview must be separated by a completely blank line from the
    license comment.
  * The first line of the Module Overview documentation comment must say
    "`Module Overview: Starboard <module-name> module`", followed by a blank
    comment line (i.e. a line that contains a `//`, but nothing else).
  * The first sentence of a documentation comment describing any entity (module,
    type, function, variable, etc...) should assume "This module," "This type,"
    "This function," or "This variable" at the beginning of the sentence, and
    not include it.
  * The first sentence of a documentation comment describing any entity should
    be a single-sentence summary description of the entire entity.
  * The first paragraph of a documentation comment should describe the overall
    behavior of the entity.
  * Paragraphs in comments should be separated by a blank comment line.
  * All public entities must have documentation comments, including enum
    entries.
  * Documentation comments should be formatted with Markdown.
  * Variables, constants, literals, and expressions should be referenced in
    comments with pipes around them.
  * All comments must be full grammatically-correct English sentences with
    proper punctuation.
  * Comments in Starboard headers must be written as requirements for the
    porter, for example: "must not return NULL" or "should not return
    NULL" rather than "will not return NULL". The choice of "must" vs "should"
    must follow the guidelines of IETF RFC,
    https://www.ietf.org/rfc/rfc2119.txt .

### Implementations

  * Each API implementation should attempt to minimize other platform
    assumptions, and should therefore use Starboard APIs to accomplish
    platform-specific work unless directly related to the platform functionality
    being implemented.
        * For example, `SbFile` can use POSIX file I/O, because that what it is
          abstracting, but it should use `SbMemoryAllocate` for any memory
          allocations, because it might be used with a variety of `SbMemory`
          implementations.
  * Whenever possible, each shared function implementation should be implemented
    in an individual file so as to maximize the chances of reuse between
    implementations.
  * This does not apply to platform-specific functions that have no chance of
    being reusable on other platforms.
  * Implementation files that can conceivably be shared between one or more
    implementations should be placed in a `starboard/shared/<dependency>/`
    directory, where `<dependency>` is the primary platform dependency of that
    implementation. (e.g. `libevent`, `posix`, `c++11`, etc.)
  * Implementation files that don't have a specific platform dependency, but
    whether to use them should be a platform decision should be placed in
    `starboard/shared/starboard/`, and must only have dependencies on other
    Starboard APIs.
  * Implementation files that definitely can be common to ALL implementations
    should be placed in `starboard/common/`.

### Language Features

  * In public headers, particularly in inline functions and macros, only C-Style
    casts may be used, though they are forbidden everywhere else.
  * It is expected that the C compiler supports inline functions. They must be
    declared `static`, and they must use the `SB_C_INLINE` or
    `SB_C_FORCE_INLINE` attribute. In straight-C code, there is no anonymous
    namespace, so `static` is allowed and required for inline functions.
  * No straight-C ISO or POSIX headers should be assumed to exist. Basic C++03
    headers may be assumed to exist in C++ code. The ISO C standards have grown
    up over a long period of time and have historically been implemented with
    quirks, missing pieces, and so on. Support for the core C++ standard library
    is much more consistent on those platforms that do support it.
  * It is idiomatic to include thin C++ inline wrappers inside public API
    headers, gated by an `#if defined(cplusplus__)` check.
