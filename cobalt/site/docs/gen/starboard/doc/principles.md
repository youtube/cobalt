---
layout: doc
title: "Starboard Design Principles"
---
# Starboard Design Principles

An overview of the goals and design principles of Starboard with the perspective
of hindsight.

**Status:** REVIEWED\
**Created:** 2016-11-12

Starboard is a porting abstraction and a collection of implementation fragments
used to abstract operating system facilities and platform quirks from C or C++
applications. It occupies a similar space to SDL, DirectFB, Marmalade, and
various others.

## Background

Starboard was created as a response to the significant effort it has
historically taken to port desktop-oriented web browsers to non-traditional
device platforms like game consoles. Chromium in particular mixes
platform-specific code with common code throughout the technology stack, making
it very difficult to know what has to be done for a new platform or how much
work it is going to be.

## Goals

Here are the main goals of Starboard, stack-ranked from most-to-least important.

  * **G1** Minimize the total time and effort required to port Starboard Client
    Applications to new platforms.
  * **G2** Minimize the incremental time and effort required to rebase Starboard
    Client Applications across platforms.
  * **G3** Enable third parties to port Starboard to their platform without
    significant engineering support from the Starboard team.
  * **G4** Ensure support for low-profile platforms that are not geared towards
    broad native C/C++ development access.
  * **G5** Provide an organization framework for platform-specific code, clearly
    delineating what is common and what is platform-specific, consolidating
    platform-specific code into a single location, and enumerating all the APIs
    needed to provide full functionality.
  * **G6** Encapsulate all platform-provided services needed to build graphical
    media applications into a single API.
  * **G7** Reduce the total surface area needed to port to new platforms.
  * **G8** Improve and encourage sharing of platform-specific implementation
    components between platforms.
  * **G9** Maximize the amount of common (platform-independent) code, to avoid
    variances between platforms, and to increase the leverage of testing,
    including fuzz testing which must often be done on particular platforms.
  * **G10** Maintain a loose binding between a Starboard Platform Implementation
    and a Starboard Client Application, such that they can be updated
    independently at a source level.
  * **G11** Avoid the pitfalls of trying to emulate POSIX, including, but not
    limited to: auto-included headers, global defines of short and common
    symbols, wrapping misbehaving or misprototyped system APIs, using custom
    toolchain facilities, and conflicts with platform headers.

## Principles

### Make APIs sufficient for their purpose, but minimally so.

APIs can generally be augmented without serious backwards-compatibility
consequences, but they can not be changed or pruned so easily, so it is better
to **err on the side of providing less**.

#### Corollary: Implementation details should be as hidden as possible.

#### Corollary: Anything that could be implemented purely on top of Starboard APIs should be implemented purely on top of Starboard APIs.

#### Exception: If there are good reasons why an API may need to be implemented in a platform-specific manner on one or more platforms, but can be commonly implemented on other platforms, it should be part of the API, with a shared Starboard-based implementation.

#### Exception: For the select few cases where Starboard implementations also need to use it, it should be included in the Starboard API so that can happen without creating circular dependencies.

### Specify public APIs concretely and narrowly.

A broader specification of the behavior of an API function makes life easier for
the implementor, but more difficult for anyone attempting to use the API. An API
can be so weakly specified that it is not usable across platforms. It can also
be so strictly specified that it is not implementable across platforms. **Err on
the side of narrower specifications**, requiring generality only when
necessitated by one or more platforms.

#### Corollary: Documentation should be exhaustive and clear.

#### Corollary: Avoid overly-flexible convention-based interfaces.

For example, passing in a set of string-string name-value pairs. This takes the
compiler out of any kind of validation, and can encourage mismatches of
understanding between Clients and Platforms.

### Minimize the burden on the porter.

Whenever adding or changing an API, or specifying a contract, consider whether
this places a large burden on some platform implementers. This burden could be
because of a wide porting surface, or complex requirements that are difficult to
implement. It could be caused by a fundamental piece of infrastructure that
isn't provided by a particular platform.

We can always make APIs that are burdensome to use easier with more common code.

### Be consistent and predictable.

Consistency, not just in formatting, but in semantics, leads to
predictability. Some people just won't read the documentation, even if it's
thorough. Perhaps especially if it's thorough. The names of API entities should
convey the intention as completely as possible.

Yet, overly-verbose naming will make the API difficult to remember, read, and
use.

### Assume the porter knows nothing.

Engineers from a broad set of backgrounds and environments will end up being
dropped into porting Starboard. They may not have knowledge of any particular
technologies, best practices, or design patterns. They may be under an
aggressive deadline.

### Always consider threading (and document it).

Each function and module should have a strategy for dealing with multiple
threads. It should make sense for the expected use cases of the interface
entities in question. As the interface designer, it is most clear to you how the
API should be used with respect to threads.

Some may be "thread-safe," such that any functions can be called from any thread
without much concern. Note that this places the burden on the implementer to
ensure that an implementation meets that specification, and they MAY not know
how to do that. This can also be more complex than just acquiring a mutex inside
every function implementation, as there may be inherent race conditions between
function calls even when using a synchronization mechanism.

The path of least burden to the porter is to say that an interface is not
thread-safe at all, which means applications will have to take care how the API
is used. But, sometimes even this requires clarification as to what modes of
access are dangerous and how to use the API safely.

### Don't expose Informational-Only result codes, but do DLOG them.

"Informational-Only" is defined by a result code that doesn't change the
behavior of the caller. Many times, why something failed does not matter when
the product is already in the hands of the user. We often want diagnostic
logging during development

### Trust but Verify. Whenever possible, write NPLB tests for all contracts declared by the interface.

#### Corollary: For hard-to-test features (like Input) add an example sandbox app for testing.

### We will get it wrong the first time, so plan for some kind of change mechanism.

Starboard has a [versioning mechanism](versioning.md) to manage change.

## References
  * [Joshua Bloch's presentation about API design](https://www.youtube.com/watch?v=aAb7hSCtvGw)
  * [Joshua Bloch's bumper sticker API design rules](http://www.infoq.com/articles/API-Design-Joshua-Bloch)
  * [digithead's collection of API design links (I didn't read them all)](http://digitheadslabnotebook.blogspot.com/2010/07/how-to-design-good-apis.html)
