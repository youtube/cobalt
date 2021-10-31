---
layout: doc
title: "Starboard ABI"
---
# Starboard ABI

The Starboard ABI was introduced to provide a single, consistent method for
specifying the Starboard API version and the ABI. This specification ensures
that any two binaries, built with the same Starboard ABI and with arbitrary
toolchains, are compatible.

## Background

The Starboard ABI is the set of features, such as the Starboard API version or
the sizes of data types, that collectively describes the relationship between
the Starboard API and ABI of a binary. In the past, each of these features were
inconsistently and ambiguously defined in a variety of files. This led to
confusion, and made it difficult to track down the feature values for a
platform. To simplify how Starboard is configured for a target platform, all of
these features, and their concrete values, are now listed for each distinct
target architecture in Starboard ABI files. These files provide a consistent,
consolidated view of the values for each of these features, decoupling
platform-specific details from architecture.

## Goals

The overall goal of the Starboard ABI is to provide a way to implement and
verify binary compatibility on a target platform, and this goal can be broken
down into the following more concise motivations:

*   Separate platform and architecture into distinct concepts.
*   Establish a consistent set of values for the various features of each target
    architecture.
*   Consolidate this set of features and their values in a consistent,
    predictable location.
*   Provide the ability to validate the values of each feature in produced
    binaries.

## Using Starboard ABI Files

With the Starboard ABI being the source of truth for all things architecture
related, each platform must now include a Starboard ABI file in its build (see
[//starboard/sabi](../sabi)
for examples). Starboard ABI files are JSON, and should all contain identical
keys with the values being appropriate for the architecture. Each platform must
override the new
[GetPathToSabiJsonFile](../build/platform_configuration.py##339)
method in its platform configuration to return the path to the desired Starboard
ABI file (e.g.
[//starboard/linux/shared/gyp\_configuration.py](../linux/shared/gyp_configuration.py)).
By default, an empty and invalid Starboard ABI file is provided.

Additionally, all platforms must include the
[sabi.gypi](../sabi/sabi.gypi)
in their build configuration. This file will consume the specified Starboard ABI
file, resulting in the creation of a set of GYP variables and preprocessor
macros. The preprocessor macros are passed directly to the compiler and replace
the macros you might be familiar with, such as `SB_HAS_32_BIT_LONG`.

The newly defined GYP variables need to be transformed into toolchain specific
flags; these flags are what actually makes the build result in a binary for the
desired architecture. These flags will, in most cases, be identical to the flags
already being used for building.

The process outlined above is shown in the diagram below.

![Starboard ABI Overview](resources/starboard_abi_overview.png)

### Post-Starboard ABI File Cleanup

A number of GYP variables and preprocessor macros should no longer be defined
directly, and instead the Starboard ABI file will be used to define them. These
definitions need to be removed.

From `configuration_public.h`:

*   `SB_IS_ARCH_*`
*   `SB_IS_BIG_ENDIAN`
*   `SB_IS_32_BIT`
*   `SB_IS_64_BIT`
*   `SB_HAS_32_BIT_LONG`
*   `SB_HAS_64_BIT_LONG`
*   `SB_HAS_32_BIT_POINTERS`
*   `SB_HAS_64_BIT_POINTERS`

From `gyp_configuration.gypi`:

*   `target_arch`

## FAQ

### What Starboard ABI files are provided?

The Cobalt team provides, and maintains, Starboard ABI files for the following
architectures:

*   x86\_32
*   x86\_64
*   ARM v7 (32-bit)
*   ARM v8 (64-bit)

If you find that no valid Starboard ABI file exists for your architecture, or
that you need to change any values of a provided Starboard ABI file, please
reach out to the Cobalt team to advise.

### How can I verify that my build is configured correctly?

Similar to the process prior to Starboard ABI files, there are multiple levels
of verification that occur:

1.  When configuring your build, the Starboard ABI file that was specified will
    have its values sanity checked against a provided
    [schema](../sabi/sabi.schema.json).
1.  When building, a number of static assertions will assert correctness of a
    number of features generated from the Starboard ABI file against the
    features of the binary.
1.  The NPLB test suite has been expanded to include [additional
    tests](../nplb/sabi/)
    capable of verifying the remaining features of the binary.

Finally, binaries produced by the Cobalt team for your architecture, including
NPLB, will be made available to ensure end-to-end correctness of the produced
binaries.
