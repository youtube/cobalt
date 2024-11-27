# Starboard Versioning

## Motivation

When a porter implements Starboard for a platform, it is more precise to say
that they have implemented support for a certain version of Starboard.
Changes to the Starboard API are associated with Starboard versions. Any usage
of new Starboard APIs must also be protected by a compile-time check for the
Starboard version it belongs to. This decoupling of Cobalt and Starboard
versions ensures that a porter can update to a newer version of Cobalt, but not
be required to implement new Starboard APIs, if the version of Starboard they
have implemented is still supported.

## Starboard API version vs. Starboard application version

The Starboard version describes the set of Starboard APIs available to Starboard
applications. It will be incremented with every open-source release that
includes changes to the Starboard API. Reasons to increment the Starboard API
version include:

* New Starboard APIs
* Removed Starboard APIs
* Modified semantics to existing APIs

Some notable cases that do not justify incrementing the Starboard version
include:

* More descriptive or clearer comments for existing APIs that do not change the
  semantics
* New utility classes that are built on top of Starboard APIs, such as
  `starboard::ScopedFile`
* Changes that affect the upward API to Starboard applications, but not the
  downward API to porters. For example, defining new upward APIs in terms of
  existing Starboard APIs.

A particular Starboard application may be versioned independently of the
Starboard API. A given version of a Starboard application may support a range of
Starboard versions. It may be the case that some new functionality in a
Starboard application requires Starboard APIs that were added to a particular
API version. If a porter wants to use such a feature, they must therefore also
implement the required version of the Starboard API. For example, Voice Search
was added to Cobalt version 5 and requires the SbMicrophone APIs which were
added to Starboard version 2. Platforms that implemented Starboard version 1
continued to build and run Cobalt 5 correctly, but the Voice Search feature
would be unavailable.

## Range of supported Starboard versions

The minimum supported API version is defined by the `SB_MINIMUM_API_VERSION`
macro, which is defined in starboard/configuration.h. Likewise, the
`SB_MAXIMUM_API_VERSION` macro defines the maximum supported API version. All
platforms must declare a `SB_API_VERSION` macro in the platform’s
configuration.h to declare the starboard version the platform has implemented.
Declaring implementation for an API version outside this range will result in an
error at compilation time.
Generally Starboard applications will not support all versions of the Starboard
API indefinitely. Starboard application owners may increment the minimum
required Starboard version at their discretion.

## Using new Starboard APIs from Starboard Applications

Usage of a Starboard API that is not available in all supported Starboard API
versions must be guarded with a check for `SB_API_VERSION`. Starboard
applications must continue to function correctly and must not disable existing
functionality if this check evaluates to false, but it’s acceptable to disable
new functionality in Starboard applications if this evaluates to false.

## Adding and using new Starboard APIs


### "Frozen" Starboard versions

All Starboard versions that are less than the `SB_MAXIMUM_API_VERSION` version
are considered frozen. Any Starboard APIs in a frozen version MUST not change.

### Communicating Starboard API changes to porters

When a new version of Starboard is released, [starboard/CHANGELOG.md](../CHANGELOG.md) should be
updated with the feature define comments for all features enabled in that
version.
