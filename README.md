# Cobalt [![Build Matrix](https://img.shields.io/badge/-Build%20Matrix-blueviolet)](https://github.com/youtube/cobalt/blob/main/BUILD_STATUS.md)

[![codecov](https://codecov.io/github/youtube/cobalt/branch/main/graph/badge.svg?token=RR6MKKNYNV)](https://codecov.io/github/youtube/cobalt)
[![lint](https://github.com/youtube/cobalt/actions/workflows/lint.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/lint.yaml?query=event%3Apush+branch%3Amain)
[![java](https://github.com/youtube/cobalt/actions/workflows/gradle.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/gradle.yaml?query=event%3Apush+branch%3Amain)
[![python](https://github.com/youtube/cobalt/actions/workflows/pytest.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/pytest.yaml?query=event%3Apush+branch%3Amain) \
[![android](https://github.com/youtube/cobalt/actions/workflows/android.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/android.yaml?query=event%3Apush+branch%3Amain)
[![evergreen](https://github.com/youtube/cobalt/actions/workflows/evergreen.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/evergreen.yaml?query=event%3Apush+branch%3Amain)
[![linux](https://github.com/youtube/cobalt/actions/workflows/linux.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/linux.yaml?query=event%3Apush+branch%3Amain)
[![raspi-2](https://github.com/youtube/cobalt/actions/workflows/raspi-2.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/raspi-2.yaml?query=event%3Apush+branch%3Amain)

## Overview

Cobalt is a lightweight application container (i.e. an application runtime, like
a JVM or the Flash Player) that is compatible with a subset of the W3C HTML5
specifications. If you author a single-page web application (SPA) that complies
with the Cobalt Subset of W3C standards, it will run as well as possible on all
the devices that Cobalt supports.


## Motivation

The Cobalt Authors originally maintained a port of Chromium called H5VCC, the
HTML5 Video Container for Consoles, ported to each of the major game consoles,
designed to run our HTML5-based video browse and play application. This took a
long time to port to each platform, consisted of 9 million lines of C++ code
(before we touched it), was dangerous to modify without unintended consequences,
and was thoroughly designed for a resource-rich, multi-process environment
(e.g. a desktop, laptop, or modern smartphone).

After wrestling with this for several years, we imagined an environment that was
not designed for traditional scrolling web content, but was intended to be a
runtime environment for rich client applications built with the same
technologies -- HTML, CSS, JavaScript -- and designed from the ground-up to run
on constrained, embedded, Living Room Consumer Electronics (CE) devices, such as
Game Consoles, Set-Top Boxes (e.g. Cable, Satellite), OTT devices (e.g. Roku,
Apple TV, Chromecast, Fire TV), Blu-ray Disc Players, and Smart TVs.

These constraints (not intended to be a canonical list) make this device
spectrum vastly different from the desktop computer environment targeted by
Chromium, FireFox, and IE:

  * **Limited Memory.** All except the very latest, expensive CE devices have a
    very small amount of memory available for applications. This usually is
    somewhere in the ballpark of 500MB, including graphics and media
    memory, as opposed to multiple gigabytes of CPU memory (and more gigabytes
    of GPU memory) in modern desktop and laptop computers, and mobile devices.
  * **Slow CPUs.** Most CE devices have much slower CPUs than what is available
    on even a budget desktop computer. Minor performance concerns can be greatly
    exaggerated, which seriously affects priorities. Cobalt currently expects a
    4-core 32-bit ARMv7 CPU as a baseline.
  * **Minimal GPU.** Not all CE devices have a monster GPU to throw shaders at
    to offload CPU work. As CE devices now have a standard GPU (though not
    nearly as powerful as even a laptop), OpenGL ES 2.0 is now required
    by Cobalt.
  * **Sometimes No JIT.** Many CE devices are dealing with "High-Value Content,"
    and, as such, are very sensitive to security concerns. Ensuring that
    writable pages are not executable is a strong security protocol that can
    prevent a wide spectrum of attacks. But, as a side effect, this also means
    no ability to JIT.
  * **Heterogeneous Development Environments.** This is slowly evening out, but
    all CE devices run on custom hardware, often with proprietary methods of
    building, packaging, deploying, and running programs. Sometimes the
    toolchain doesn't support latest C++ language features. Sometimes the OS
    does not support POSIX, or it is only partially implemented.
    Sometimes the program entry point is in another language or architecture
    that requires a "trampoline" over to native binary code.
  * **No navigation.** The point of a Single-Page Application is that you don't
    go through the HTTP page dance every time you switch screens. It's slow, and
    provides poor user feedback, not to mention a jarring transition. Instead,
    one loads data from an XMLHttpRequest (XHR), and then updates one's DOM to
    reflect the new data. AJAX! Web 2.0!!


## Architecture

The Cobalt Authors forked H5VCC, removed most of the Chromium code -- in
particular WebCore and the Chrome Renderer and Compositor -- and built up from
scratch an implementation of a simplified subset of HTML, the CSS Box Model for
layout, and the Web APIs that were really needed to build a full-screen SPA
browse and play application.

The Cobalt technology stack has these major components, roughly in a high-level
application to a low-level platform order:

  * **Web Implementation** - This is where the W3C standards are implemented,
    ultimately producing an annotated DOM tree that can be passed into the
    Layout Engine to produce a Render Tree. Cobalt uses a forked copy of
    [Chrome Blink's Web IDL compiler](https://www.chromium.org/blink/webidl/) to
    turn JavaScript IDLs to generated C++ bindings code.
  * **JavaScript Engine** - We have, perhaps surprisingly, *not* written our own
    JavaScript Engine from scratch. Cobalt is running on [Chromiums V8](https://v8.dev)
    JavaScript engine. V8 supports all of our target platforms, including very
    restricted ones where write-and-execute memory pages (needed for JIT) are
    not available.
  * **Layout Engine** - The Layout Engine takes an annotated DOM Document
    produced by the Web Implementation and JavaScript Engine working together,
    and calculates a tree of rendering commands to send to the renderer (i.e. a
    Render Tree). It caches intermediate layout artifacts so that subsequent
    incremental layouts can be sped up.
  * **Renderer/Skia** - The Renderer walks a Render Tree produced by the Layout
    Engine, rasterizes it using the [Chromium graphics library Skia](https://skia.org/), and swaps
    it to the front buffer. This is accomplished using Hardware Skia on OpenGL
    ES 2.0. Note that the renderer runs in a different thread from the Layout
    Engine, and can interpolate animations that do not require re-layout. This
    decouples rendering from Layout and JavaScript, allowing for smooth,
    consistent animations on platforms with a variety of capabilities.
  * **Net / Media** - These are Chromium's Network and Media engines. We are
    using them directly, as they don't cause any particular problems with the
    extra constraints listed above.
  * **Base** - This is Chromium's "Base" library, which contains a wide variety
    of useful things used throughout Cobalt, Net, and Media. Cobalt uses a
    combination of standard C++ containers (e.g. vector, string) and Base as the
    foundation library for all of its code.
  * **Other Third-party Libraries** - Most of these are venerable, straight-C,
    open-source libraries that are commonly included in other open-source
    software. Mostly format decoders and parsers (e.g. libpng, libxml2,
    zlib). We fork these from Chromium, as we want them to be the most
    battle-tested versions of these libraries.
  * **Starboard** - **Starboard** is the Cobalt porting
    interface. One major difference between Cobalt and Chromium is that we have
    created a hard straight-C porting layer, and ported ALL of the compiled
    code, including Base and all third-party libraries, to use it instead of
    directly using POSIX standard libraries, which are not consistent, even on
    modern systems (see Android, Windows, MacOS X, and iOS). Additionally,
    Starboard includes APIs that haven't been effectively standardized across
    platforms, such as display Window creation, Input events, and Media
    playback. A good overview of which OS interfaces are abstracted by Starboard
    can be found in the [reference documentation.](https://cobalt.dev/reference/starboard/modules/configuration.html)
  * **ANGLE and Glimp** [**ANGLE** is a Chromium library](https://angleproject.org/)
    that adapts OpenGL ES 2.0 graphics to various other platform-native graphics APIs.
    Cobalt uses it on Windows platforms to run on DirectX. Glimp is a similar
    custom adapter layer that translatest from GL ES2.0 to PlayStation native graphics.

## The Cobalt Subset

> Oh, we got both kinds of HTML tags,\
> we got `<span>` and `<div>`! \
> We even have CSS Flexbox now, hooray!

See the [Cobalt Subset
specification](https://cobalt.dev/development/reference/supported-features.html)
for more details on which tags, properties, and Web APIs are supported in
Cobalt.

## Interesting Source Locations

All source locations are specified relative to `src/` (this directory).

  * `base/` - Chromium's Base library. Contains common utilities, and a light
    platform abstraction, which has been superseded in Cobalt by Starboard.
  * `net/` - Chromium's Network library. Contains enough infrastructure to
    support the network needs of an HTTP User-Agent (like Chromium or Cobalt),
    an HTTP server, a DIAL server, and several abstractions for networking
    primitives. Also contains SPDY and QUIC implementations.
  * `cobalt/` - The home of all Cobalt application code. This includes the Web
    Implementation, Layout Engine, Renderer, and some other Cobalt-specific
    features.
      * `cobalt/build/` - The core build generation system, `gn.py`, and
        configurations for supported platforms.
      * `cobalt/doc/` - Contains a wide range of detailed information and guides
        on Cobalt features, functionality and best practices for Cobalt
        development.
      * `cobalt/media/` - Chromium's Media library. Contains all the code that
        parses, processes, and manages buffers of video and audio data. It
        send the buffers to the SbPlayer implementation for playback.
  * `starboard/` - Cobalt's porting layer. Please see Starboard's
    [`README.md`](starboard/README.md) for more detailed information about
    porting Starboard (and Cobalt) to a new platform.
  * `third_party/` - Where all of Cobalt's third-party dependencies live. We
    don't mean to be pejorative, we love our third-party libraries! This
    location is dictated by Google OSS release management rules...


## Building and Running the Code

  See the below reference port setup guides for more details:

  * [Linux](cobalt/site/docs/development/setup-linux.md)
  * [Raspi](cobalt/site/docs/development/setup-raspi.md)
  * [Android](cobalt/site/docs/development/setup-android.md)
  * [Docker](cobalt/site/docs/development/setup-docker.md)
  * [RDK](cobalt/site/docs/development/setup-rdk.md)

## Build Types

Cobalt has four build optimization levels, going from the slowest, least
optimized, with the most debug information at the top (debug) to the fastest,
most optimized, and with the least debug information at the bottom (gold):

 Type  | Optimizations | Logging | Asserts | Debug Info | Console
 :---- | :------------ | :------ | :------ | :--------- | :-------
 debug | None          | Full    | Full    | Full       | Enabled
 devel | Full          | Full    | Full    | Full       | Enabled
 qa    | Full          | Limited | None    | None       | Enabled
 gold  | Full          | None    | None    | None       | Disabled

When building for release, you should always use a gold build for the final
product.

## Origin of this Repository

This is a fork of the chromium repository at http://git.chromium.org/git/chromium.git
