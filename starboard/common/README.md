# Starboard Common Library
Shared library code of C++ helper classes on top of the C Starboard APIs.
A good example is the `Socket` class built on top of the `SbSocket` API. The
class provides RAII-style mechanism to manage resources.

## Intended Use
The library is intended to be used by both Starboard and Cobalt layers.
