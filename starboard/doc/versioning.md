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
TBD: Timelines and communication around when an upcoming Cobalt release will
require porters to implement a newer version of Starboard.

## Using new Starboard APIs from Starboard Applications
Usage of a Starboard API that is not available in all supported Starboard API
versions must be guarded with a check for `SB_API_VERSION`. Starboard
applications must continue to function correctly and must not disable existing
functionality if this check evaluates to false, but it’s acceptable to disable
new functionality in Starboard applications if this evaluates to false.

## Adding and using new Starboard APIs
### “experimental” Starboard version
At any given time, exactly one version of Starboard will be denoted as the
“experimental” version, as defined by the `SB_EXPERIMENTAL_API_VERSION` macro in
starboard/configuration.h. It is generally not recommended to declare support
for this version. Any Starboard APIs defined in the experimental version are
subject to change and API requirements could be added, removed, or changed at
any time.
### “frozen” Starboard versions
All Starboard versions that are less than the experimental version are
considered frozen. Any Starboard APIs in a frozen version shall not change as
long as that version is supported by Cobalt.
### Life of a Starboard API
New Starboard APIs should be defined in the experimental version.

When introducing a new Starboard API (or modifying an existing one), a new
feature version define should be created within the "Experimental Feature
Defines" section of starboard/configuration.h, and set to
`SB_EXPERIMENTAL_API_VERSION`. A well written comment should be added in front
of the feature define that describes exactly what is introduced by the feature.
In the comment, all new/modified/removed symbols should be identified, and all
modified header files should be named.
For example,

```
// --- Experimental Feature Defines ------------------------------------------

...

// Introduce a new API in starboard/screensaver.h, which declares the following
// functions for managing the platform's screensaver settings:
//   SbScreensaverDisableScreensaver()
//   SbScreensaverEnableScreensaver()
// Additionally, a new event, kSbEventTypeScreensaverStarted, is introduced in
// starboard/event.h.
#define SB_SCREENSAVER_FEATURE_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Introduce a new API in starboard/new_functionality.h which declares the
// function SbNewFunctionality().
#define SB_MY_NEW_FEATURE_API_VERSION SB_EXPERIMENTAL_API_VERSION
```

When declaring the new interface, the following syntax should be used:

```
// starboard/new_functionality.h
#if SB_API_VERSION >= SB_MY_NEW_FEATURE_API_VERSION
void SbNewFunctionality();
#endif
```

Starboard application features that use a new API must have a similar check:

```
// cobalt/new_feature.cc
#if SB_API_VERSION >= SB_MY_NEW_FEATURE_API_VERSION
void DoSomethingCool() {
  SbNewFunctionality();
}
#endif
```

When a new version of a Starboard Application that requires new Starboard APIs
is to be released, these new Starboard APIs will be frozen to the value of the
current experimental version, and the experimental version will be incremented
by one.  Additionally, the feature define should be removed, and its comment
moved into the (newly created) section for the corresponding version in
starboard/CHANGELOG.md.

```
// starboard/new_functionality.h
#if SB_API_VERSION >= 7
void SbNewFunctionality();
#endif

// starboard/other_new_functionality.h
#if SB_API_VERSION >= SB_MY_OTHER_NEW_FEATURE_API_VERSION
void SbStillInDevelopment();
#endif

// starboard/configuration.h
#define SB_EXPERIMENTAL_API_VERSION 8

// cobalt/new_feature.cc
#if SB_API_VERSION >= 7
void DoSomethingCool() {
  SbNewFunctionality();
}
#endif
```

A developer who increments the experimental version must ensure that stubs and
reference platforms declare support for the new experimental version through
their respective `SB_API_VERSION` macros.

### Communicating Starboard API changes to porters
When a new version of Starboard is released, [starboard/CHANGELOG.md](../CHANGELOG.md) should be
updated with the feature define comments for all features enabled in that
version.
