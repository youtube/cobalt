[![Build](https://github.com/khaledhosny/ots/actions/workflows/ci.yml/badge.svg)](https://github.com/khaledhosny/ots/actions/workflows/ci.yml)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/ots.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:ots)

OpenType Sanitizer
==================

The OpenType Sanitizer (OTS) parses and serializes OpenType files (OTF, TTF)
and WOFF and WOFF2 font files, validating them and sanitizing them as it goes.

The C library is integrated into Chromium and Firefox, and also simple
command line tools to check files offline in a Terminal.

The CSS [font-face property][1] is great for web typography. Having to use images
in order to get the correct typeface is a great sadness; one should be able to
use vectors.

However, on many platforms the system-level TrueType font renderers have never
been part of the attack surface before, and putting them on the front line is
a scary proposition... Especially on platforms like Windows, where it's a
closed-source blob running with high privilege.

Building from source
--------------------

Instructions below are for building standalone OTS utilities, if you want to
use OTS as a library then the recommended way is to copy the source code and
integrate it into your existing build system. Our build system does not build a
shared library intentionally.

Build OTS:

    $ meson build
    $ ninja -C build

Run the tests (if you wish):

    $ ninja -C build test

Usage
-----

See [docs](docs)

* * *

Thanks to Alex Russell for the original idea.

[1]: http://www.w3.org/TR/CSS2/fonts.html#font-descriptions
