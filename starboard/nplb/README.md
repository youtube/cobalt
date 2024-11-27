# No Platform Left Behind

Starboard attempts to make bringing up new platforms as easy as possible. No
Platform Left Behind (NPLB) services this goal by providing a compliance test
suite that new platform implementations can use to determine achieved progress.

## Current State

All defined APIs are at least called by NPLB, and most are verified in one way
or another. The APIs that are most likely to just be implemented by a single
system call are not exhaustively tested. In particular, the Starboard functions
defined in `string.h` and `memory.h`.

NPLB tests must work on all Starboard implementations, so they may make no
assumptions about platform-specific details. They attempt to define a living
contract for the APIs on all platforms.

## Test Organization

The tests are broken out into files by both Starboard module and function.

    src/starboard/nplb/<module>_<function>_test.cc

Each test may incidentally test other functions. There is an attempt to keep
things as self-contained within a function and module as possible, but also an
attempt to avoid testing behavior that is effectively tested as part of another
test.

For example, `SbSocketSendTo` and `SbSocketReceiveFrom` are tested together,
ensuring that the API is self-consistent on both sides of a connection.
Therefore, only one set of tests exist to cover those use cases, in
`socket_receive_from_test.cc`.
